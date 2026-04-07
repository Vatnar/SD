---
date: 2026-04-07
---
Systems query the ECS via views, not by directly iterating over entities. A view represents a query for entities that have a specific set of components.

## Current Implementation
A view selects the *smallest* component pool as its iteration base, which minimises the number of candidates the view needs to check. For each candidate entity in that smallest pool, it validates that the entity also has all other requested components using the entity's component mask.

```cpp
template<typename... Components>
std::tuple<Components&...> GetComponentGroup(Entity e);

bool Iterator::IsValid() const {
  return (manager.HasComponent<Components>(currentEntity) && ...);
}
```

The iteration loop advances until it finds a valid entity or reaches the end:
```cpp
void Next() {
  do {
    index++;
  } while (index < entities->size() && !IsValid());
}
```

**Performance:**
- Iterates over the smallest pool's dense entity list
- Per-candidate validation: k bitset tests (one per requested component type)
- Sequential memory access for component data — cache-friendly
- No allocations, no virtual dispatch

**Bottleneck:** When the smallest pool is large but few entities actually match (e.g., querying a rare component combination), the view does O(n × k) validity checks before finding valid entities.

## Archetypes
If the per-entity validity check becomes a bottleneck, we could fix it with **archetypes**. Which is to pre-group entities by their full component mask.

**How it works:**
- Each unique component mask becomes an *archetype*
- A separate storage maps archetype mask -> dense entity list
- When an entity's components change (add/remove), it moves between archetype groups
- View iteration: select archetype group(s) matching the required components → iterate entities directly

```cpp
// Archetype storage
std::unordered_map<ComponentMask, std::vector<Entity>> archetypeGroups;

// View becomes: direct iteration over matching archetype(s)
// No per-entity HasComponent() check needed
```

**Tradeoffs:**
- Component add/remove: O(1) membership update + entity move between groups
- Memory overhead: archetype group mappings
- Complexity: tracking entity moves on component changes

If rendering or physics systems shows hitching from view iteration, then archetypes are worth pursuing. We could still use the same View interface, so this is purely a implementation change.
