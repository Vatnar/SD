---
date: 2026-04-07
---
Systems query the ECS to get all entities with a specific set of components and iterate over them sequentially.

```cpp
auto view = em.View<Transform, Velocity>();
for (auto [entity, transform, velocity] : view) {
    transform.position += velocity.velocity * dt;
}
```

Views are lightweight, no allocations, no heap storage. The iterator dereferences directly into the component pools, so iteration is cache-friendly.

**Example: Physics system applying gravity**
```cpp
void PhysicsSystem::Update(float dt) {
    auto view = entityManager.View<Transform, RigidBody>();
    for (auto [entity, transform, body] : view) {
        body.velocity += Vec3{0, -9.81f, 0} * dt;
        transform.position += body.velocity * dt;
    }
}
```

**Key properties:**
- Views have no ownership of entities or components.
- Systems act on component data, not on entities directly
- Each component pool is independent, so adding a new component type doesn't affect existing systems
- Iteration is single-threaded and sequential

> [!note] No entity dependency
> Systems care about component data, not entity identity. The `Entity` handle is available in the loop if needed (e.g., for destruction), but the primary concern is the component group.
