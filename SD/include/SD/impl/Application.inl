namespace sd {
template<typename T, typename... Args>
  requires std::is_base_of_v<Layer, T>
T& Application::push_window_layer(WindowId id, Args&&... args) {
  auto& windows = window_manager->get_windows();
  if (!windows.contains(id)) {
    log::engine::critical("Attempted to push layer to invalid window ID: {}",
                          static_cast<uint32_t>(id));
  }
  auto& layer = windows[id].view_layers.push_layer<T>(std::forward<Args>(args)...);
  layer.app   = this;
  return layer;
}
template<typename T, typename... Args>
  requires std::is_base_of_v<Layer, T>
std::expected<std::reference_wrapper<T>, ViewError>
Application::push_layer_to_view(ViewId id, Args&&... args) {
  return view_manager->push_layer<T>(id, std::forward<Args>(args)...);
}
template<typename T, typename... Args>
  requires std::is_base_of_v<Layer, T>
std::expected<std::reference_wrapper<T>, ViewError>
Application::push_layer_to_view(const std::string& name, Args&&... args) {
  auto id_res = view_manager->get_id(name);
  if (!id_res)
    return std::unexpected(id_res.error());
  return view_manager->push_layer<T>(*id_res, std::forward<Args>(args)...);
}
} // namespace sd
