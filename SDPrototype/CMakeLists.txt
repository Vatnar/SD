add_executable(SDPrototype main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(SDPrototype PRIVATE
        ../vendor/NVRHI/thirdparty/Vulkan-Headers/include
        ../vendor/NVRHI/include
        ../vendor/glfw/include
)
target_link_libraries(SDPrototype PRIVATE nvrhi_vk nvrhi Vulkan::Vulkan glfw)