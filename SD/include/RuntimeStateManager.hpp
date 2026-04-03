// TODO(docs): Add file-level Doxygen header
//   - @file RuntimeStateManager.hpp
//   - @brief State persistence for hot reload support
//   - Explain the hot reload flow: Serialize -> DLL reload -> Restore
//   - What state is preserved vs lost during hot reload
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace SD {

class Application;
class Scene;

// TODO(docs): Document RuntimeStateManager
//   - Purpose: Enables game state survival across DLL hot reloads
//   - Usage pattern (when to call Serialize/Restore)
//   - Limitations (what cannot be serialized)
//   - Example integration with hot reload system
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
