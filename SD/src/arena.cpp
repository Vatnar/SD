#include "SD/arena.hpp"

#include <cinttypes>

#include <sys/mman.h>

#include "SD/core/types.hpp"


#define SLLStackPush_N(f, n, next) ((n)->next = (f), (f) = (n))
#define SLLStackPop_N(f, next)     ((f) = (f)->next)

#define SLLStackPush(f, n)         SLLStackPush_N(f, n, next)
#define SLLStackPop(f)             SLLStackPop(f, next)

#define MemoryZero(s, z)           memset((s), 0, (z))

#if defined(__SANITIZE_ADDRESS__)
extern "C" void __asan_poison_memory_region(const volatile void* addr, size_t size);
extern "C" void __asan_unpoison_memory_region(const volatile void* addr, size_t size);
#define AsanPoisonMemoryRegion(addr, size)   __asan_poison_memory_region((addr), (size))
#define AsanUnpoisonMemoryRegion(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#define AsanPoisonMemoryRegion(addr, size)   ((void)(addr), (void)(size))
#define AsanUnpoisonMemoryRegion(addr, size) ((void)(addr), (void)(size))
#endif


void* reserve_memory(U64 size) {
  void* result = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    result = nullptr;
  }
  return result;
}
void* reserve_memory_large(U64 size) {
  void* result = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (result == MAP_FAILED) {
    result = nullptr;
  }
  return result;
}

bool commit_memory(void* ptr, U64 size) {
  mprotect(ptr, size, PROT_READ | PROT_WRITE);
  return true;
}

bool commit_memory_large(void* ptr, U64 size) {
  mprotect(ptr, size, PROT_READ | PROT_WRITE);
  return true;
}

void release_memory(void* ptr, U64 size) {
  munmap(ptr, size);
}

U64 get_large_page_size() {
  FILE* fp = fopen("/proc/meminfo", "r");
  if (!fp) {
    std::abort();
  }

  char label[64]{};
  U64  value{};
  char unit[16]{};
  bool found = false;

  while (fscanf(fp, "%63s %" SCNu64 " %15s", label, &value, unit) != EOF) {
    if (strcmp(label, "Hugepagesize:") == 0) {
      found = true;
      break;
    }
  }
  fclose(fp);

  if (!found) {
    std::abort();
  }
  return value * 1024uz;
}

U64 get_page_size() {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    std::abort();
  }
  return static_cast<U64>(page_size);
}

Arena* arena_alloc(const ArenaParams params) {
  U64 reserve_size = params.reserve_size;
  U64 commit_size  = params.commit_size;

  LOCAL_PERSIST U64 large_page_size = get_large_page_size();
  LOCAL_PERSIST U64 page_size       = get_page_size();

  void* base = params.optional_backing_buffer;
  if (base == nullptr) {
    if (params.flags & ArenaFlags::LARGE_PAGES) {
      reserve_size = align_pow2(reserve_size, large_page_size);
      commit_size  = align_pow2(commit_size, large_page_size);
    } else {
      reserve_size = align_pow2(reserve_size, page_size);
      commit_size  = align_pow2(commit_size, page_size);
    }

    if (params.flags & ArenaFlags::LARGE_PAGES) {
      base = reserve_memory_large(reserve_size);
      commit_memory_large(base, commit_size);
    } else {
      base = reserve_memory(reserve_size);
      commit_memory(base, commit_size);
    }

    AsanPoisonMemoryRegion(base, commit_size);
  } else {
    AsanPoisonMemoryRegion(base, params.reserve_size);
  }

  if (base == nullptr) [[unlikely]] {
    std::abort();
  }

  AsanUnpoisonMemoryRegion(base, sizeof(Arena));
  Arena* arena         = static_cast<Arena*>(base);
  arena->current       = arena;
  arena->flags         = params.flags;
  arena->commit_size   = params.commit_size;
  arena->reserve_size  = params.reserve_size;
  arena->base_position = 0;
  arena->position      = sizeof(Arena);
  arena->committed     = commit_size;
  arena->reserved      = reserve_size;
  arena->location      = params.location;
  arena->name          = params.name;

  // TODO: arenatable debug
  return arena;
}

void arena_release(Arena* arena) {
  for (Arena *n{arena->current}, *prev = nullptr; n != nullptr; n = prev) {
    prev = n->prev;
    AsanUnpoisonMemoryRegion(n, n->committed);
    release_memory(n, n->reserved);
  }
}

void* Arena::push(this Arena& arena, U64 size, U64 align, bool zero) {
  Arena* current  = arena.current;
  U64    pos_pre  = align_pow2(current->position, align);
  U64    pos_post = pos_pre + size;

  U64 size_to_zero = 0;
  if (zero) {
    size_to_zero = min(current->committed, pos_post) - pos_pre;
  }

  if (current->reserved < pos_post && !(arena.flags & ArenaFlags::NO_CHAIN)) {
    Arena* new_block = nullptr;
    // TODO: FREE_LIST OPTIONAL THINGY

    if (new_block == nullptr) {
      U64 res_size    = current->reserve_size;
      U64 commit_size = current->commit_size;
      if (size + sizeof(Arena) > res_size) {
        res_size    = align_pow2(size + sizeof(Arena), align);
        commit_size = align_pow2(size + sizeof(Arena), align);
      }
      new_block    = arena_alloc({.flags        = current->flags,
                                  .reserve_size = res_size,
                                  .commit_size  = commit_size,
                                  .location     = current->location});
      size_to_zero = 0;
    } else {
      size_to_zero = size;
    }

    new_block->base_position = current->base_position + current->reserved;
    SLLStackPush_N(arena.current, new_block, prev);

    current  = new_block;
    pos_pre  = align_pow2(current->position, align);
    pos_post = pos_pre + size;
  }

  // commit new page
  if (current->committed < pos_post) {
    U64 commit_post_aligned = pos_post + current->commit_size - 1;
    commit_post_aligned -= commit_post_aligned % current->commit_size;
    U64 commit_post_clamped = clamp_top(commit_post_aligned, current->reserved);
    U64 commit_size         = commit_post_aligned - current->committed;
    U8* commit_ptr          = reinterpret_cast<U8*>(current) + current->committed;
    if (current->flags & ArenaFlags::LARGE_PAGES) {
      commit_memory_large(commit_ptr, commit_size);
    } else {
      commit_memory(commit_ptr, commit_size);
    }
    AsanPoisonMemoryRegion(commit_ptr, commit_size);
    current->committed = commit_post_clamped;
  }

  void* result = nullptr;
  if (current->committed >= pos_post) {
    result            = reinterpret_cast<U8*>(current) + pos_pre;
    current->position = pos_post;
    AsanUnpoisonMemoryRegion(result, size);
    MemoryZero(result, size_to_zero);
  }

  if (result == nullptr) [[unlikely]] {
    std::abort();
  }
  return result;
}

U64 Arena::pos(this Arena& arena) {
  Arena* current = arena.current;
  U64    pos     = current->base_position + current->position;
  return pos;
}
void Arena::pop_to(this Arena& arena, U64 pos) {
  U64    big_pos = clamp_bot(sizeof(Arena), pos);
  Arena* current = arena.current;

#if ARENA_FREE_LIST
#else
  for (Arena* prev = 0; current->base_position >= big_pos; current = prev) {
    prev = current->prev;
    AsanUnpoisonMemoryRegion(current, current->committed);
    release_memory(current, current->reserved);
  }
#endif
  arena.current = current;
  U64 new_pos   = big_pos - current->base_position;
  ASSERT_ALWAYS(new_pos <= current->position);
  AsanPoisonMemoryRegion(reinterpret_cast<U8*>(current) + new_pos, (current->position - new_pos));
  current->position = new_pos;
}

void Arena::clear(this Arena& arena) {
  arena.pop_to(0);
}

void Arena::pop(this Arena& arena, U64 amount) {
  U64 pos_old = arena.pos();
  U64 pos_new = pos_old;
  if (amount < pos_old) {
    pos_new = pos_old - amount;
  }
  arena.pop_to(pos_new);
}

Temp Arena::temp_begin(this Arena& arena) {
  Temp temp = {&arena, arena.pos()};
  return temp;
}

void Temp::end(this Temp temp) {
  temp.arena->pop_to(temp.pos);
}
