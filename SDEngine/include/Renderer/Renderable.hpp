#pragma once

#include "Renderer/Mesh.hpp"
#include "Renderer/Material.hpp"
#include <VLA/Matrix.hpp> // Assuming this is your matrix library

// TODO: This is a basic renderable object. It could be extended to be part of a scene graph or entity-component system (ECS).
// An ECS would be a more flexible approach, where a "Renderable" is just a component you add to an entity.

struct Renderable {
    // A shared pointer is used here to allow multiple renderables to share the same mesh data (e.g., instancing).
    std::shared_ptr<Mesh> mesh;

    // A shared pointer for the material, allowing multiple objects to share the same look.
    std::shared_ptr<Material> material;

    // The object's transformation in the world.
    VLA::Matrix4x4f transform;

    // How does this support 2D and 3D?
    // - For 2D: The 'mesh' would be a simple quad, and the renderer's camera would use an orthographic projection.
    // - For 3D: The 'mesh' would be a 3D model, and the camera would use a perspective projection.
    // The Renderable struct itself is generic and works for both.
};
