#include <Core/Entity.hpp>

#include "Renderer/Material.hpp"
#include "Renderer/Mesh.hpp"
#include "VLA/Matrix.hpp"

struct Renderable {
  std::shared_ptr<Mesh> mesh;
  std::shared_ptr<Material> material;
  VLA::Matrix4x4f transform;
};

class RenderSystem {
  // Do some render stuff based on Renderable data
  void Update(EntityManager& em) {
    for (auto [entity, renderable]) : em.View<Renderable>()) {
      }
  }
};

int main() {
  EntityManager entityManager;

  // Hook up systems, maybe make them autoregister. Maybe not, incase we want to have multiple
  // instances of the ECS

  Entity e = entityManager.Create();

  entityManager.AddComponent<Renderable>(e, nullptr, nullptr, VLA::Matrix4x4f::Identity);
  auto cmp = entityManager.TryGetComponent<Renderable>(e);
  cmp->mesh = nullptr;
}
