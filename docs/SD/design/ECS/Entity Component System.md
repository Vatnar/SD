---
date: 2026-04-07
---
The entity component system in SD consists of the [EntityManager](https://sd.vatnar.dev/docs/classSD_1_1EntityManager.html) class, the [Entity](https://sd.vatnar.dev/docs/structSD_1_1Entity.html) type, and collections of [SparseEntitySet](https://sd.vatnar.dev/docs/classSD_1_1SparseEntitySet.html) for tracking entities and components. Each registered component has its own SparseEntitySet pool for storing component registrations.

Multiple systems, such as the render systems or physics system act on the ECS, querying it for components or groups of components, which it can sequentially access quickly.

## Entity
The entity is a simple struct consisting of just an index and generation. Using two 32 bit integers as index and generation is more useful than a single 64 bit integer since we can then easily track destroyed and recycled entities. By checking the generation we also prevent use after frees on entities.
```cpp
struct Entity {
  u32 index;
  u32 generation;
};
```
### Entity Lifecycle
Created -> alive -> destroyed -> recycled

To create an entity we use:
```
EntityManager em; 
Entity e = em.Create();
```
Or using the command system:
```cpp
CommandQueue queue;
EntityHandle h(0); // Handles resolve within the queue on Apply()
queue.Add<CreateEntityCmd>(h); // Add a CreateEntity Cmd
queue.Apply(em); // Execute the queue on the ECS in sequential order
```
To read more about [[CommandQueue]].

## SparseEntitySet
See [[SparseEntitySet]] for how sparse pages map to dense arrays for flexible yet sequential component storage.

## Queries and Views
See [[Queries and Views]] for how systems query entities with specific component combinations.

## Components
See [[Components]] for how components are defined, registered via macros, and serialized.

## Systems
See [[Systems]] for how systems efficiently query and iterate over component data.
