#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace SD {

class Application;
class Scene;

class RuntimeStateManager {
public:
  RuntimeStateManager();
  ~RuntimeStateManager();

  void Serialize();
  void Restore(Application* app);
  void SetApplication(Application* app);
  bool HasState() const;

private:
  Application* mApp = nullptr;
  std::unordered_map<std::string, std::vector<char>> mSerializedScenes;
};

} // namespace SD
