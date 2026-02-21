#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <sys/inotify.h>
#include <thread>

#include "Application.hpp"

class RuntimeStateManager {};

extern SD::Application* CreateApplication(RuntimeStateManager* runtimeManager);

std::atomic<bool> reloadTriggered{false};

std::atomic stopFlag{false};
std::atomic returned{false};

void* handle = nullptr;


auto load_create() -> SD::Application* (*)() {
  // Copy to temp to avoid lock during rebuild.

  std::filesystem::copy("../lib/libSandboxApp.so", "./libSandboxApp.so.tmp",
                        std::filesystem::copy_options::overwrite_existing);

  void* new_handle = dlopen("./libSandboxApp.so.tmp", RTLD_LAZY | RTLD_LOCAL);
  if (!new_handle) {
    std::cerr << "dlopen: " << dlerror() << '\n';
    return nullptr;
  }
  if (handle)
    dlclose(handle); // Close old only after new load.
  handle = new_handle;

  auto* create = reinterpret_cast<SD::Application* (*)()>(dlsym(handle, "CreateApplication"));
  if (!create) {
    std::cerr << "dlsym: " << dlerror() << '\n';
    dlclose(handle);
    return nullptr;
  }
  return create;
}

void inotify_monitor(const std::string& so_path) {
  int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  int wd = inotify_add_watch(fd, so_path.c_str(), IN_MODIFY | IN_MOVED_TO);

  char buf[4096];
  while (true) {
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len > 0) {
      auto* event = reinterpret_cast<inotify_event*>(buf);
      while (len > 0) {
        if (event->mask & (IN_MODIFY | IN_MOVED_TO)) {
          reloadTriggered.store(true, std::memory_order_release);
          return; // Trigger main reload.
        }
        len -= sizeof(inotify_event) + event->len;
        event = reinterpret_cast<inotify_event*>(reinterpret_cast<char*>(event) +
                                                 sizeof(inotify_event) + event->len);
      }
    }
    usleep(10000); // Minimal backoff if needed.
  }
  close(fd);
}


int main() {
  SD::Log::Init();
  // load func

  // TODO: Create scene manager
  RuntimeStateManager stateManager;

  auto create = load_create();
  if (!create)
    return 1;

  std::thread monitor{inotify_monitor, "./libSandboxApp.so"};
  std::thread* worker = nullptr;

  SD::Application* app = nullptr;
  app = create(&stateManager);
  worker = new std::thread{[app]() {
    app->Run(&stopFlag);
    returned.store(true);
  }};


  while (true) {
    // if its terminated manually, just end for now, probably want to expose a way to restart when
    // we want
    if (returned.load(std::memory_order_acquire)) {
      break;
    }
    // check for updated .so, if updated, then delete app, and restart another one
    if (reloadTriggered.exchange(false, std::memory_order_acq_rel)) {
      stopFlag.store(true, std::memory_order_release);

      worker->join();
      if (worker) {
        delete worker;
      }
      if (app)
        delete app;

      create = load_create();
      if (!create)
        continue;

      stopFlag.store(false, std::memory_order_release);
      returned.store(false);
      app = create();
      worker = new std::thread{[app]() {
        app->Run(&stopFlag);
        returned.store(true);
      }};
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  monitor.join();
  if (worker)
    delete worker;
  if (app)
    delete app;
  if (handle)
    dlclose(handle);
  return 0;
}
