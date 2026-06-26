#include "SD/core/layers/EngineDebugLayer.hpp"

#include <array>
#include <imgui.h>

#include "SD/Application.hpp"
#include "SD/core/LayoutManager.hpp"
#include "SD/core/SceneManager.hpp"
#include "SD/core/View.hpp"
#include "SD/core/ViewManager.hpp"
#include "SD/core/ecs/components.hpp"
#include "SD/core/events/window/keyboard_events.hpp"
#include "SD/core/events/window/mouse_events.hpp"
#include "SD/core/logging.hpp"
#include "SD/core/vulkan/VulkanRenderer.hpp"

namespace sd {

EngineDebugLayer::EngineDebugLayer(ApplicationRuntime runtime,
                                   EngineServices     services,
                                   Scene*             scene) :
  m_views(runtime.views), m_scenes(runtime.scenes), m_layout(runtime.layout),
  m_events(runtime.events), m_frame_timer(runtime.timer),
  m_hot_reload_enabled(runtime.hot_reload_enabled), m_global_layers(runtime.global_layers),
  m_renderer(services.renderer) {
  debug_name  = "EngineDebug";
  this->scene = scene;
}

void EngineDebugLayer::on_update(float dt) {
  m_update_count++;
  m_timer += dt;

  if (m_timer >= 1.0f) {
    m_prev_fixed         = m_fixed_update_count;
    m_prev_update        = m_update_count;
    m_fixed_update_count = 0;
    m_update_count       = 0;
    m_timer              = 0.0f;
  }

  m_views.for_each([&](View& view) {
    if (m_log_view_resizes && view.consume_extent_changed()) {
      log::debug_layer::tagged("view",
                               "View '{}' resized to {}x{}",
                               view.get_name(),
                               view.get_extent().width,
                               view.get_extent().height);
    }
  });

  // Track entity lifecycle changes if logging is enabled
  if (m_selected_scene && m_log_entity_lifecycle) {
    if (m_selected_scene != m_prev_scene_for_entity_count) {
      m_prev_entity_count           = m_selected_scene->em.get_alive_entity_count();
      m_prev_scene_for_entity_count = m_selected_scene;
    } else {
      int alive = m_selected_scene->em.get_alive_entity_count();
      if (alive != m_prev_entity_count) {
        if (alive > m_prev_entity_count)
          log::debug_layer::tagged("entity",
                                   "Entity created (count: {} -> {})",
                                   m_prev_entity_count,
                                   alive);
        else
          log::debug_layer::tagged("entity",
                                   "Entity destroyed (count: {} -> {})",
                                   m_prev_entity_count,
                                   alive);
        m_prev_entity_count = alive;
      }
    }
  }
}

void EngineDebugLayer::on_fixed_update(double /*dt*/) {
  m_fixed_update_count++;
}

void EngineDebugLayer::on_event(EventVariant& e) {
  if (m_log_events) {
    std::string detail;
    std::visit(
        overloaded{
            [&](const KeyPressedEvent& ke) {
              detail = fmt::format("Key: {} (mods: {}, repeat: {})", ke.key, ke.mods, ke.repeat);
            },
            [&](const KeyReleasedEvent& ke) {
              detail = fmt::format("Key: {} (mods: {})", ke.key, ke.mods);
            },
            [&](const MousePressedEvent& me) {
              detail =
                  fmt::format("Button: {} (mods: {}, repeat: {})", me.button, me.mods, me.repeat);
            },
            [&](const MouseReleasedEvent& me) {
              detail = fmt::format("Button: {} (mods: {})", me.button, me.mods);
            },
            [&](const MouseMovedEvent& me) {
              detail = fmt::format("Pos: {:.1f}, {:.1f}", me.x_pos, me.y_pos);
            },
            [&](const MouseScrolledEvent& me) {
              detail = fmt::format("Offset: {:.1f}, {:.1f}", me.x_offset, me.y_offset);
            },
            [&](const auto&) { detail = "Generic Event"; },
        },
        e.event);
    log::debug_layer::tagged("event", "{} [{}]", e.event.index(), detail);
  }
}

void EngineDebugLayer::on_im_gui_menu_bar() {
  // Apply pending layout once dockspace is ready
  m_layout.apply_pending_layout(make_runtime());

  if (ImGui::BeginMenu("Debug")) {
    ImGui::MenuItem("View Inspector", nullptr, &m_show_view_inspector);
    ImGui::MenuItem("Scene Inspector", nullptr, &m_show_scene_inspector);
    ImGui::MenuItem("Renderer Info", nullptr, &m_show_renderer_info);
    ImGui::MenuItem("Context Overlay", nullptr, &m_show_context_overlay);

    ImGui::Separator();
    display_layout_menu();

    ImGui::Separator();
    if (ImGui::MenuItem("Reload Shaders")) {
      app->request_shader_reload();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Restart Game")) {
      app->request_restart();
    }

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Log")) {
    if (ImGui::BeginMenu("Categories")) {
      if (!m_category_tree_built)
        build_category_tree();
      for (auto& child : m_category_root.children) {
        render_category_menu(child);
      }
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Clear Log")) {
      log::clear_history();
    }
    if (ImGui::MenuItem("Copy Log")) {
      auto        history = log::get_log_history();
      std::string clip;
      for (auto& entry : history) {
        if (!is_log_entry_visible(entry))
          continue;
        clip +=
            fmt::format("[+{:.3f}s] [{}] {}\n", entry.uptime_sec, entry.category, entry.message);
      }
      if (!clip.empty())
        ImGui::SetClipboardText(clip.c_str());
    }

    if (ImGui::BeginMenu("Levels")) {
      ImGui::MenuItem("Trace", nullptr, &m_log_show_trace);
      ImGui::MenuItem("Debug", nullptr, &m_log_show_debug);
      ImGui::MenuItem("Info", nullptr, &m_log_show_info);
      ImGui::MenuItem("Warn", nullptr, &m_log_show_warn);
      ImGui::MenuItem("Error", nullptr, &m_log_show_error);
      ImGui::MenuItem("Critical", nullptr, &m_log_show_critical);
      ImGui::MenuItem("General", nullptr, &m_log_show_general);
      ImGui::EndMenu();
    }

    ImGui::MenuItem("Show Timestamps", nullptr, &m_log_show_timestamps);

    ImGui::EndMenu();
  }
}

void EngineDebugLayer::on_gui_render() {
  if (m_show_view_inspector) {
    if (ImGui::Begin("View Inspector", &m_show_view_inspector)) {
      View* selected_view = nullptr;
      if (m_selected_view_id.has_value()) {
        auto view_result = m_views.get(m_selected_view_id.value());
        if (view_result) {
          selected_view = &view_result->get();
        } else {
          m_selected_view_id.reset();
        }
      }

      if (ImGui::BeginCombo("View Selector",
                            selected_view ? selected_view->get_name().c_str() : "None")) {
        m_views.for_each([&](View& view) {
          if (ImGui::Selectable(view.get_name().c_str(),
                                m_selected_view_id == view.get_view_id())) {
            m_selected_view_id = view.get_view_id();
          }
        });
        ImGui::EndCombo();
      }
      if (selected_view)
        display_view_info(selected_view);
    }
    ImGui::End();
  }

  if (m_show_scene_inspector) {
    if (ImGui::Begin("Scene Inspector", &m_show_scene_inspector)) {
      display_scene_selector();
      ImGui::Separator();
      display_ecs_inspector();
    }
    ImGui::End();
  }

  if (m_show_event_log) {
    if (ImGui::Begin("Engine Log", &m_show_event_log)) {
      display_event_log();
    }
    ImGui::End();
  }

  if (m_show_renderer_info) {
    if (ImGui::Begin("Renderer Info", &m_show_renderer_info)) {
      ImGui::Text("App Performance: %.1f FPS", ImGui::GetIO().Framerate);
      ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
      ImGui::Separator();
      if (ImGui::BeginTable("Timings",
                            2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Fixed Updates/s");
        ImGui::TableNextColumn();
        ImGui::Text("%4d (60 Hz expected)", m_prev_fixed);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Render Frames/s");
        ImGui::TableNextColumn();
        ImGui::Text("%4d FPS", m_prev_update);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Frame Work Time");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", m_frame_timer.get_frame_work_time() * 1000.0f);

        ImGui::EndTable();
      }

      ImGui::Separator();
      if (ImGui::Checkbox("Pause Hot-Reload", &app->game_code_reload_paused)) {
        app->game_code_reload_paused = !app->game_code_reload_paused;
      }
      if (ImGui::Button("Force Reload Shaders")) {
        app->request_shader_reload();
      }
    }
    ImGui::End();
  }

  if (m_show_context_overlay) {
    auto mouse_pos = ImGui::GetMousePos();
    m_views.for_each([&](View& view) {
      ImVec2 region_pos    = view.get_content_region_pos();
      ImVec2 region_extent = view.get_content_region_extent();
      if (mouse_pos.x >= region_pos.x && mouse_pos.x <= region_pos.x + region_extent.x &&
          mouse_pos.y >= region_pos.y && mouse_pos.y <= region_pos.y + region_extent.y) {
        ImGui::SetNextWindowPos(ImVec2(mouse_pos.x + 15, mouse_pos.y + 15));
        if (ImGui::Begin("##ContextOverlay",
                         nullptr,
                         ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings)) {
          float ndc_x = ((mouse_pos.x - region_pos.x) / region_extent.x) * 2.0f - 1.0f;
          float ndc_y = ((mouse_pos.y - region_pos.y) / region_extent.y) * 2.0f - 1.0f;
          ImGui::Text("View: %s", view.get_name().c_str());
          ImGui::Text("NDC: %.3f, %.3f", ndc_x, ndc_y);
          ImGui::End();
        }
      }
    });
  }

  // Display dialogs
  display_save_layout_dialog();
  display_delete_layout_dialog();
  display_overwrite_confirmation_dialog();

  // Handle shortcuts
  handle_layout_shortcuts();
  handle_debug_shortcuts();
}

void EngineDebugLayer::display_view_info(View* selected_view) {
  if (!selected_view)
    return; // Defensive null check

  if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Log Window Events", &m_log_events);
    ImGui::Checkbox("Log View Resizes", &m_log_view_resizes);
    ImGui::Checkbox("Show Context Overlay", &m_show_context_overlay);
    ImGui::TreePop();
  }
  ImGui::Separator();
  ImGui::Text("ID: %u | Extent: %ux%u",
              static_cast<uint32_t>(selected_view->get_view_id()),
              selected_view->get_extent().width,
              selected_view->get_extent().height);

  AspectMode current_mode = selected_view->get_aspect_mode();
  int        modeInt      = static_cast<int>(current_mode);
  ImGui::Text("Aspect Mode:");
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Height", &modeInt, 0))
    selected_view->set_aspect_mode(AspectMode::FIXED_HEIGHT);
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Width", &modeInt, 1))
    selected_view->set_aspect_mode(AspectMode::FIXED_WIDTH);
  ImGui::SameLine();
  if (ImGui::RadioButton("Best Fit", &modeInt, 2))
    selected_view->set_aspect_mode(AspectMode::BEST_FIT);

  ImGui::Separator();
  RenderMode  current_r_mode = selected_view->get_render_mode();
  const char* r_modes[]      = {"Shaded", "Wireframe"};
  int         r_mode_int     = static_cast<int>(current_r_mode);
  if (ImGui::Combo("Render Mode", &r_mode_int, r_modes, IM_ARRAYSIZE(r_modes))) {
    selected_view->set_render_mode((RenderMode)r_mode_int);
  }

  ImVec2 mouse_pos     = ImGui::GetMousePos();
  ImVec2 region_pos    = selected_view->get_content_region_pos();
  ImVec2 region_extent = selected_view->get_content_region_extent();

  bool is_hovered = (mouse_pos.x >= region_pos.x && mouse_pos.x <= region_pos.x + region_extent.x &&
                     mouse_pos.y >= region_pos.y && mouse_pos.y <= region_pos.y + region_extent.y);

  ImGui::Text("Hovered: %s", is_hovered ? "YES" : "NO");

  // Calculate cursor NDC relative to the specific view bounds
  float ndc_x = ((mouse_pos.x - region_pos.x) / region_extent.x) * 2.0f - 1.0f;
  float ndc_y = ((mouse_pos.y - region_pos.y) / region_extent.y) * 2.0f - 1.0f;

  ImGui::Text("Cursor NDC: %.3f, %.3f", ndc_x, ndc_y);

  if (ImGui::TreeNode("Matrix Data")) {
    const auto& m = selected_view->get_projection();
    for (int i = 0; i < 4; i++) {
      ImGui::Text("%.3f %.3f %.3f %.3f", m(i, 0), m(i, 1), m(i, 2), m(i, 3));
    }
    ImGui::TreePop();
  }
}

void EngineDebugLayer::display_scene_selector() {
  if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Log Scene Activity", &m_log_scene_changes);
    ImGui::Checkbox("Log Entity Lifecycle", &m_log_entity_lifecycle);
    ImGui::TreePop();
  }
  ImGui::Separator();

  Scene* initial_scene = nullptr;
  m_scenes.for_each([&](Scene& scene) {
    if (!initial_scene)
      initial_scene = &scene;
  });
  if (!m_selected_scene && initial_scene)
    m_selected_scene = initial_scene;

  std::string current_label = m_selected_scene ? m_selected_scene->get_name() : "None";
  if (ImGui::BeginCombo("Inspect Scene", current_label.c_str())) {
    m_scenes.for_each([&](Scene& scene) {
      if (ImGui::Selectable(scene.get_name().c_str(), m_selected_scene == &scene)) {
        m_selected_scene = &scene;
      }
    });
    ImGui::EndCombo();
  }
}

void EngineDebugLayer::display_ecs_inspector() {
  if (!m_selected_scene) {
    ImGui::Text("No Scene selected.");
    return;
  }

  for (auto [entity, transform] : m_selected_scene->em.view<sd::components::Transform>()) {
    std::string label = "Entity " + std::to_string(entity.index);
    if (auto* name = m_selected_scene->em.try_get_component<sd::components::DebugName>(entity)) {
      label = name->name + " (ID: " + std::to_string(entity.index) + ")";
    }

    if (ImGui::TreeNode(label.c_str())) {
      if (auto* transform_ptr =
              m_selected_scene->em.try_get_component<sd::components::Transform>(entity)) {
        if (ImGui::TreeNode("Transform")) {
          for (int i = 0; i < 4; i++) {
            if (ImGui::DragFloat4(("col " + std::to_string(i)).c_str(),
                                  &transform_ptr->world_matrix(0, i),
                                  0.01f)) {
              if (m_log_scene_changes) {
                log::debug_layer::tagged("transform", "Entity {} transform changed", entity.index);
              }
            }
          }
          ImGui::TreePop();
        }
      }
      if (auto* renderable =
              m_selected_scene->em.try_get_component<sd::components::Renderable>(entity)) {
        if (ImGui::TreeNode("Renderable")) {
          if (ImGui::ColorEdit4("Color", renderable->color)) {
            if (m_log_scene_changes) {
              log::debug_layer::tagged("color", "Entity {} color changed", entity.index);
            }
          }
          if (ImGui::DragInt("Stage", (int*)&renderable->render_stage, 1, 0, 10)) {
            if (m_log_scene_changes) {
              log::debug_layer::tagged("render",
                                       "Entity {} render stage changed to {}",
                                       entity.index,
                                       renderable->render_stage);
            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }
  }
}

void EngineDebugLayer::build_category_tree() {
  auto& categories = log::get_category_registry();
  m_category_root  = CategoryNode{"", "", true, {}};

  for (auto& cat : categories) {
    CategoryNode* node  = &m_category_root;
    size_t        start = 0;
    while (start < cat.name.size()) {
      size_t end = cat.name.find('/', start);
      if (end == std::string::npos)
        end = cat.name.size();
      std::string segment = cat.name.substr(start, end - start);

      CategoryNode* child = nullptr;
      for (auto& c : node->children) {
        if (c.name == segment) {
          child = &c;
          break;
        }
      }
      if (!child) {
        node->children.push_back(CategoryNode{segment, cat.name.substr(0, end), cat.visible, {}});
        child = &node->children.back();
      }
      node  = child;
      start = end + 1;
    }
  }
  m_category_tree_built = true;
}

void EngineDebugLayer::render_category_node(CategoryNode& node) {
  if (node.name.empty()) {
    // Root node — just render children
    for (auto& child : node.children) {
      render_category_node(child);
    }
    return;
  }

  if (node.children.empty()) {
    // Leaf node
    bool visible = node.visible;
    if (ImGui::Checkbox(node.name.c_str(), &visible)) {
      set_category_visible(node, visible);
    }
  } else {
    // Parent node — TreeNode + Checkbox
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    ImGui::SameLine();
    bool visible = node.visible;
    if (ImGui::Checkbox("##vis", &visible)) {
      set_category_visible(node, visible);
    }
    if (open) {
      for (auto& child : node.children) {
        render_category_node(child);
      }
      ImGui::TreePop();
    }
  }
}

bool EngineDebugLayer::is_log_visible(const std::string& category) {
  // Walk the tree to find the category node
  CategoryNode* node  = &m_category_root;
  size_t        start = 0;
  while (start < category.size()) {
    size_t end = category.find('/', start);
    if (end == std::string::npos)
      end = category.size();
    std::string segment = category.substr(start, end - start);

    CategoryNode* child = nullptr;
    for (auto& c : node->children) {
      if (c.name == segment) {
        child = &c;
        break;
      }
    }
    if (!child)
      return false;
    node  = child;
    start = end + 1;
  }
  return node->visible;
}

void EngineDebugLayer::render_category_menu(CategoryNode& node) {
  if (node.children.empty()) {
    bool vis = node.visible;
    if (ImGui::MenuItem(node.name.c_str(), nullptr, &vis)) {
      set_category_visible(node, vis);
    }
  } else {
    if (ImGui::BeginMenu(node.name.c_str())) {
      bool vis = node.visible;
      if (ImGui::MenuItem(node.name.c_str(), nullptr, &vis)) {
        set_category_visible(node, vis);
      }
      ImGui::Separator();
      for (auto& child : node.children) {
        render_category_menu(child);
      }
      ImGui::EndMenu();
    }
  }
}

void EngineDebugLayer::set_category_visible(CategoryNode& node, bool visible) {
  node.visible = visible;
  for (auto& child : node.children) {
    set_category_visible(child, visible);
  }
  // Sync back to the flat registry
  auto& categories = log::get_category_registry();
  for (auto& cat : categories) {
    if (cat.name == node.full_path || log::is_category_under(cat.name, node.full_path)) {
      cat.visible = visible;
    }
  }
}

bool EngineDebugLayer::is_log_entry_visible(const log::LogEntry& entry) {
  switch (entry.level) {
    case log::LogLevel::TRACE:
      return m_log_show_trace;
    case log::LogLevel::DEBUG:
      return m_log_show_debug;
    case log::LogLevel::INFO:
      return m_log_show_info;
    case log::LogLevel::WARN:
      return m_log_show_warn;
    case log::LogLevel::ERROR:
      return m_log_show_error;
    case log::LogLevel::CRITICAL:
      return m_log_show_critical;
    case log::LogLevel::GENERAL:
      return m_log_show_general;
    default:
      return false;
  }
}

void EngineDebugLayer::display_event_log() {
  size_t total_entries = log::get_total_entry_count();
  auto&  categories    = log::get_category_registry();

  // Ensure category tree is built before is_log_visible() is called below
  if (!m_category_tree_built || categories.size() != m_category_root.children.size())
    build_category_tree();

  // Search bar
  ImGui::InputText("Search", m_log_search_buffer, sizeof(m_log_search_buffer));

  ImGui::Separator();

  std::string search = m_log_search_buffer;
  std::ranges::transform(search, search.begin(), tolower);

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(total_entries));
  while (clipper.Step()) {
    int  display_count = clipper.DisplayEnd - clipper.DisplayStart;
    auto batch         = log::get_entries(clipper.DisplayStart, display_count);

    for (auto& log : batch) {
      if (!is_log_entry_visible(log) || !is_log_visible(log.category))
        continue;

      if (!search.empty()) {
        std::string msg_lower = log.message;
        std::ranges::transform(msg_lower, msg_lower.begin(), tolower);
        if (msg_lower.find(search) == std::string::npos)
          continue;
      }

      // Category color
      ImVec4 cat_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      for (auto& cat : categories) {
        if (cat.name == log.category) {
          cat_color = cat.color;
          break;
        }
      }

      // Level color for the tag
      ImVec4 level_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      switch (log.level) {
        case log::LogLevel::TRACE:
          level_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
          break;
        case log::LogLevel::DEBUG:
          level_color = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);
          break;
        case log::LogLevel::INFO:
          level_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
          break;
        case log::LogLevel::WARN:
          level_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
          break;
        case log::LogLevel::ERROR:
          level_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
          break;
        case log::LogLevel::CRITICAL:
          level_color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
          break;
        default:
          break;
      }

      // Format: [+sec] [Category] [level] message
      if (m_log_show_timestamps) {
        ImGui::Text("[+%.3fs] ", log.uptime_sec);
        ImGui::SameLine();
      }
      ImGui::TextColored(cat_color, "[%s]", log.category.c_str());

      if (log.level != log::LogLevel::GENERAL) {
        ImGui::SameLine();

        const char* level_str = "?";
        switch (log.level) {
          case log::LogLevel::TRACE:
            level_str = "trace";
            break;
          case log::LogLevel::DEBUG:
            level_str = "debug";
            break;
          case log::LogLevel::INFO:
            level_str = "info";
            break;
          case log::LogLevel::WARN:
            level_str = "warn";
            break;
          case log::LogLevel::ERROR:
            level_str = "error";
            break;
          case log::LogLevel::CRITICAL:
            level_str = "critical";
            break;
          default:
            break;
        }
        ImGui::TextColored(level_color, "[%s]", level_str);
      }

      ImGui::SameLine();
      if (log.level == log::LogLevel::GENERAL && !log.message.empty() && log.message[0] == '[') {
        auto close = log.message.find(']');
        if (close != std::string::npos) {
          ImGui::TextColored(cat_color, "%.*s", static_cast<int>(close + 1), log.message.c_str());
          ImGui::SameLine();
          ImGui::Text("%s", log.message.c_str() + close + 1);
        } else {
          ImGui::Text("%s", log.message.c_str());
        }
      } else {
        ImGui::Text("%s", log.message.c_str());
      }
    }
  }

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
}

void EngineDebugLayer::display_layout_menu() {
  if (ImGui::BeginMenu("Layout")) {
    auto&              layout_manager = m_layout;
    const std::string& current_layout = layout_manager.get_current_layout();

    // Presets
    bool is_minimal = current_layout == "Minimal";
    if (ImGui::MenuItem("Minimal", "Alt+1", is_minimal)) {
      layout_manager.apply_preset(LayoutManager::Preset::MINIMAL, make_runtime());
    }

    bool isDefault = current_layout == "Default";
    if (ImGui::MenuItem("Default", "Alt+2", isDefault)) {
      layout_manager.apply_preset(LayoutManager::Preset::DEFAULT, make_runtime());
    }

    // Custom layouts
    auto layouts = layout_manager.get_user_layout_names();
    if (!layouts.empty()) {
      ImGui::Separator();
      int shortcut_num = 3;
      for (const auto& name : layouts) {
        bool        is_current = name == current_layout;
        std::string shortcut   = (shortcut_num <= 9) ? "Alt+" + std::to_string(shortcut_num) : "";
        if (ImGui::MenuItem(name.c_str(), shortcut.c_str(), is_current)) {
          layout_manager.load_layout(name, make_runtime());
        }
        shortcut_num++;
      }
    }

    ImGui::Separator();

    // Save and Delete options
    if (ImGui::MenuItem("Save Layout...", "Alt+0")) {
      m_show_save_layout_dialog = true;
    }
    if (ImGui::MenuItem("Delete Layout...")) {
      m_show_delete_layout_dialog = true;
    }

    ImGui::EndMenu();
  }
}

void EngineDebugLayer::display_save_layout_dialog() {
  if (!m_show_save_layout_dialog) {
    m_save_dialog_initialized = false;
    return;
  }

  // Clear buffer only once when dialog first opens
  if (!m_save_dialog_initialized) {
    m_layout_name_buffer.fill('\0');
    m_save_dialog_initialized = true;
  }
  ImGui::OpenPopup("Save Layout");

  if (ImGui::BeginPopupModal("Save Layout", &m_show_save_layout_dialog)) {
    ImGui::InputText("Layout Name", m_layout_name_buffer.data(), m_layout_name_buffer.size());

    ImGui::Spacing();

    bool save_clicked = ImGui::Button("Save");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_show_save_layout_dialog = false;
    }

    if (save_clicked && m_layout_name_buffer[0] != '\0') {
      auto&       layout_manager = m_layout;
      std::string layout_name(m_layout_name_buffer.data());

      if (layout_manager.has_layout(layout_name)) {
        // Show overwrite confirmation
        m_pending_layout_name         = layout_name;
        m_show_overwrite_confirmation = true;
        m_show_save_layout_dialog     = false;
      } else {
        layout_manager.save_layout(layout_name);
        m_show_save_layout_dialog = false;
      }
    }

    ImGui::EndPopup();
  }
}

void EngineDebugLayer::display_delete_layout_dialog() {
  if (!m_show_delete_layout_dialog)
    return;

  ImGui::OpenPopup("Delete Layout");

  if (ImGui::BeginPopupModal("Delete Layout", &m_show_delete_layout_dialog)) {
    auto& layout_manager = m_layout;
    auto  layouts        = layout_manager.get_user_layout_names();

    ImGui::Text("Select layout to delete:");
    ImGui::Spacing();

    for (const auto& name : layouts) {
      ImGui::PushID(name.c_str());
      if (ImGui::Button("Delete", ImVec2(200, 0))) {
        layout_manager.delete_layout(name, make_runtime());
      }
      ImGui::SameLine();
      ImGui::Text("\"%s\"", name.c_str());
      ImGui::PopID();
    }

    if (layouts.empty()) {
      ImGui::TextDisabled("No custom layouts to delete");
    }

    ImGui::Spacing();
    if (ImGui::Button("Close")) {
      m_show_delete_layout_dialog = false;
    }

    ImGui::EndPopup();
  }
}

void EngineDebugLayer::display_overwrite_confirmation_dialog() {
  if (!m_show_overwrite_confirmation)
    return;

  ImGui::OpenPopup("Confirm Overwrite");

  if (ImGui::BeginPopupModal("Confirm Overwrite", &m_show_overwrite_confirmation)) {
    ImGui::Text("Layout \"%s\" already exists.", m_pending_layout_name.c_str());
    ImGui::Text("Do you want to overwrite it?");
    ImGui::Spacing();

    if (ImGui::Button("Yes")) {
      m_layout.save_layout(m_pending_layout_name);
      m_show_overwrite_confirmation = false;
      m_pending_layout_name.clear();
    }

    ImGui::SameLine();
    if (ImGui::Button("No")) {
      m_show_overwrite_confirmation = false;
      m_pending_layout_name.clear();
    }

    ImGui::EndPopup();
  }
}

void EngineDebugLayer::handle_layout_shortcuts() {
  // Alt+Number shortcuts for layouts
  ImGuiIO& io = ImGui::GetIO();

  if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift) {
    auto& layout_manager = m_layout;

    // Check each number key individually
    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
      layout_manager.apply_preset(LayoutManager::Preset::MINIMAL, make_runtime());
    } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
      layout_manager.apply_preset(LayoutManager::Preset::DEFAULT, make_runtime());
    } else if (ImGui::IsKeyPressed(ImGuiKey_0)) {
      m_show_save_layout_dialog = true;
    } else {
      // Alt+3 through Alt+9 for custom layouts
      int custom_index = -1;
      if (ImGui::IsKeyPressed(ImGuiKey_3))
        custom_index = 0;
      else if (ImGui::IsKeyPressed(ImGuiKey_4))
        custom_index = 1;
      else if (ImGui::IsKeyPressed(ImGuiKey_5))
        custom_index = 2;
      else if (ImGui::IsKeyPressed(ImGuiKey_6))
        custom_index = 3;
      else if (ImGui::IsKeyPressed(ImGuiKey_7))
        custom_index = 4;
      else if (ImGui::IsKeyPressed(ImGuiKey_8))
        custom_index = 5;
      else if (ImGui::IsKeyPressed(ImGuiKey_9))
        custom_index = 6;

      if (custom_index >= 0) {
        auto layouts = layout_manager.get_user_layout_names();
        if (custom_index < static_cast<int>(layouts.size())) {
          layout_manager.load_layout(layouts[custom_index], make_runtime());
        }
      }
    }
  }
}

void EngineDebugLayer::handle_debug_shortcuts() {
  ImGuiIO& io = ImGui::GetIO();

  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P)) {
    log::debug_layer::tagged("debug", "Command palette placeholder - coming soon!");
  }
}

void EngineDebugLayer::apply_preset_configuration(bool inspectors_visible,
                                                  bool log_visible,
                                                  bool renderer_visible) {
  m_show_view_inspector  = inspectors_visible;
  m_show_scene_inspector = inspectors_visible;
  m_show_event_log       = log_visible;
  m_show_renderer_info   = renderer_visible;
}

} // namespace sd
