#include <Core/Entity.hpp>

#include "Renderer/Material.hpp"
#include "Renderer/Mesh.hpp"
#include "VLA/Matrix.hpp"


struct Renderable {
  std::shared_ptr<Material> material;
};
REGISTER_SD_COMPONENT(Renderable);

struct StaticMesh {
  std::shared_ptr<Mesh> mesh;
};
REGISTER_SD_COMPONENT(StaticMesh);

struct Transform {
  VLA::Matrix4x4f transform;
};
REGISTER_SD_COMPONENT(Transform);

struct RigidBody {
  VLA::Vector3f velocity;
  VLA::Vector3f acceleration;
  VLA::Vector3f force;
  VLA::Vector3f moment;
  VLA::Matrix4x4f transform;
};

class RenderSystem {
  // Do some render stuff based on Renderable data
  void Update(EntityManager& em) {
    // for (auto [entity, renderable]) : em.View<Renderable>()) {
    // }
  }
};

int main() {
  EntityManager entityManager;

  // Hook up systems, maybe make them autoregister. Maybe not, incase we want to have multiple
  // instances of the ECS

  Entity e = entityManager.Create();

  entityManager.AddComponent<Renderable>(e, nullptr);
  entityManager.AddComponent<StaticMesh>(e, nullptr);
  entityManager.AddComponent<Transform>(e, VLA::Matrix4x4f::Identity);


  auto cmps = entityManager.GetAllComponentNames(e);
  for (auto cmp : cmps) {
    std::print("Component: {}\t\t Location: {}\n", cmp.name, cmp.data);
  }
}
