#include "Core/ECS/CommandQueue.hpp"

#include "Core/ECS/Commands.hpp"
#include "Utils/Utils.hpp"

namespace SD {

u32 CreateEntityCmd::sTypeId = CommandRegistry::Register("CreateEntityCmd");
u32 DestroyEntityCmd::sTypeId = CommandRegistry::Register("DestroyEntityCmd");

template<>
u32 AddComponentCmd<Transform>::sTypeId =
    CommandRegistry::Register("AddComponentCmd", ComponentTraits<Transform>::Id);
template<>
u32 RemoveComponentCmd<Transform>::sTypeId =
    CommandRegistry::Register("RemoveComponentCmd", ComponentTraits<Transform>::Id);

template<>
u32 AddComponentCmd<Camera>::sTypeId =
    CommandRegistry::Register("AddComponentCmd", ComponentTraits<Camera>::Id);
template<>
u32 RemoveComponentCmd<Camera>::sTypeId =
    CommandRegistry::Register("RemoveComponentCmd", ComponentTraits<Camera>::Id);

template<>
u32 AddComponentCmd<Renderable>::sTypeId =
    CommandRegistry::Register("AddComponentCmd", ComponentTraits<Renderable>::Id);
template<>
u32 RemoveComponentCmd<Renderable>::sTypeId =
    CommandRegistry::Register("RemoveComponentCmd", ComponentTraits<Renderable>::Id);

template<>
u32 AddComponentCmd<DebugName>::sTypeId =
    CommandRegistry::Register("AddComponentCmd", ComponentTraits<DebugName>::Id);
template<>
u32 RemoveComponentCmd<DebugName>::sTypeId =
    CommandRegistry::Register("RemoveComponentCmd", ComponentTraits<DebugName>::Id);

namespace {
struct CommandRegistrar {
  CommandRegistrar() {
    CommandFactory::Register(CreateEntityCmd::sTypeId,
                             [] { return std::make_unique<CreateEntityCmd>(); });
    CommandFactory::Register(DestroyEntityCmd::sTypeId,
                             [] { return std::make_unique<DestroyEntityCmd>(); });
    CommandFactory::Register(AddComponentCmd<Transform>::sTypeId,
                             [] { return std::make_unique<AddComponentCmd<Transform>>(); });
    CommandFactory::Register(RemoveComponentCmd<Transform>::sTypeId,
                             [] { return std::make_unique<RemoveComponentCmd<Transform>>(); });
    CommandFactory::Register(AddComponentCmd<Camera>::sTypeId,
                             [] { return std::make_unique<AddComponentCmd<Camera>>(); });
    CommandFactory::Register(RemoveComponentCmd<Camera>::sTypeId,
                             [] { return std::make_unique<RemoveComponentCmd<Camera>>(); });
    CommandFactory::Register(AddComponentCmd<Renderable>::sTypeId,
                             [] { return std::make_unique<AddComponentCmd<Renderable>>(); });
    CommandFactory::Register(RemoveComponentCmd<Renderable>::sTypeId,
                             [] { return std::make_unique<RemoveComponentCmd<Renderable>>(); });
    CommandFactory::Register(AddComponentCmd<DebugName>::sTypeId,
                             [] { return std::make_unique<AddComponentCmd<DebugName>>(); });
    CommandFactory::Register(RemoveComponentCmd<DebugName>::sTypeId,
                             [] { return std::make_unique<RemoveComponentCmd<DebugName>>(); });
  }
};
static CommandRegistrar registrar;
} // namespace

u32 CommandRegistry::Register(const char* name, u32 componentId) {
  std::string key = std::string(name) + "_" + std::to_string(componentId);
  auto it = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  u32 id = static_cast<u32>(names.size());
  names.push_back(name);
  map[key] = id;
  return id;
}

u32 CommandRegistry::GetId(const char* name, u32 componentId) {
  std::string key = std::string(name) + "_" + std::to_string(componentId);
  auto it = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  return type_max<u32>;
}

const char* CommandRegistry::GetName(u32 id) {
  if (id < names.size()) {
    return names[id];
  }
  return "Unknown";
}

void CommandFactory::Register(u32 typeId, CreatorFunc creator) {
  if (typeId >= creators.size()) {
    creators.resize(typeId + 1);
  }
  creators[typeId] = std::move(creator);
}

std::unique_ptr<Command> CommandFactory::Create(u32 typeId) {
  if (typeId < creators.size() && creators[typeId]) {
    return creators[typeId]();
  }
  return nullptr;
}

void CommandQueue::Apply(EntityManager& em) {
  for (const auto& cmd : mCommands) {
    cmd->Execute(em, *this);
  }
  mCommands.clear();
}
void CommandQueue::SetEntityForHandle(EntityHandle entityHandle, Entity entity) {
  SD_ALWAYS_ASSERT(entityHandle.IsValid(), "Cannot set entity for invalid (sentinel) handle");
  if (entityHandle.id >= mHandleToEntity.size()) {
    mHandleToEntity.resize(entityHandle.id + 1);
  }
  mHandleToEntity[entityHandle.id] = entity;
}
bool CommandQueue::IsHandleResolved(EntityHandle handle) const {
  return handle.IsValid() && handle.id < mHandleToEntity.size();
}
void CommandQueue::Clear() {
  mCommands.clear();
  mHandleToEntity.clear();
}
usize CommandQueue::GetCount() const {
  return mCommands.size();
}
Entity CommandQueue::GetEntity(EntityHandle handle) const {
  assert(handle.id < mHandleToEntity.size() && "Entity has not been resolved");
  return mHandleToEntity[handle.id];
}

void CommandQueue::Serialize(Serializer& serializer) const {
  serializer.Write(static_cast<u32>(mCommands.size()));
  for (const auto& cmd : mCommands) {
    serializer.Write(cmd->GetTypeId());
    // Serialize command to temp buffer to know its size
    std::vector<std::byte> payload;
    Serializer payloadSerializer(payload);
    cmd->Serialize(payloadSerializer);
    serializer.Write(static_cast<u32>(payload.size()));
    serializer.Write(payload.data(), payload.size());
  }
}

void CommandQueue::Deserialize(Serializer& serializer) {
  u32 count = serializer.Read<u32>();
  mCommands.clear();
  mCommands.reserve(count);

  for (u32 i = 0; i < count; ++i) {
    u32 typeId = serializer.Read<u32>();
    u32 payloadSize = serializer.Read<u32>();
    usize payloadStart = serializer.GetOffset();
    if (auto cmd = CommandFactory::Create(typeId)) {
      cmd->Deserialize(serializer);
      mCommands.push_back(std::move(cmd));
    } else {
      SD::Log::Engine::Error("Unknown command type ID {} during deserialization, skipping {} bytes", typeId, payloadSize);
      serializer.SetOffset(payloadStart + payloadSize);
    }
  }
}

} // namespace SD
