#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Core/Systems/Renderer/Shader.hpp"
#include "Core/Systems/Renderer/Texture.hpp"
#include "Utils/Utils.hpp"

// TODO: This is a simple material system. It could be expanded to support more complex properties
// like uniform buffer data (e.g., color, roughness, metallic values) that could be updated
// per-material.

struct Material {
public:
  // A material is defined by its shaders.
  std::shared_ptr<Shader> shader;

  // A map of texture names (or binding points) to texture objects.
  // This allows a material to have multiple textures (e.g., diffuse, normal, specular).
  std::map<std::string, std::shared_ptr<Texture>> textures;

  // Example of how a layer would use this:
  //
  // auto myMaterial = std::make_shared<Material>();
  // myMaterial->shader = resourceManager.GetShader("DefaultShader");
  // myMaterial->textures["diffuse"] = resourceManager.GetTexture("assets/textures/my_texture.png");
  //
  // myRenderable.material = myMaterial;
};
