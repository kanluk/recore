message(STATUS "Loading external dependencies...")

message(STATUS "Loading Vulkan-Headers...")
add_subdirectory(Vulkan-Headers)
target_compile_definitions(Vulkan-Headers INTERFACE VK_NO_PROTOTYPES)

message(STATUS "Loading volk...")
# set VOLK_INCLUDES manually so it finds the Vulkan Headers
set(VOLK_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers/include")
add_subdirectory(volk)
set_target_properties(volk PROPERTIES POSITION_INDEPENDENT_CODE ON)


message(STATUS "Loading VulkanMemoryAllocator...")
add_subdirectory(VulkanMemoryAllocator SYSTEM)


message(STATUS "Loading GLFW...")
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_INSTALL OFF)
option(BUILD_SHARED_LIBS ON)
# set(GLFW_BUILD_X11 OFF)
# set(GLFW_BUILD_WAYLAND ON)
set(GLFW_BUILD_X11 ON)
set(GLFW_BUILD_WAYLAND OFF)
add_subdirectory(glfw)


message(STATUS "Loading imgui...")
add_library(imgui STATIC)
target_sources(imgui PRIVATE
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imconfig.h
    imgui/imgui.h
    imgui/imgui_internal.h
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h

    imgui/backends/imgui_impl_vulkan.cpp
    imgui/backends/imgui_impl_glfw.cpp
)
target_link_libraries(imgui PUBLIC
    glfw
    Vulkan-Headers
)
target_include_directories(imgui PUBLIC imgui)
target_compile_definitions(imgui INTERFACE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)


message(STATUS "Loading shaderc...")
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)

option(ENABLE_SPVREMAPPER OFF)
option(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL ON)
option(ENABLE_OPT OFF)
option(BUILD_TESTING OFF)
option(BUILD_EXTERNAL OFF)
add_subdirectory(glslang)

set(SHADERC_SKIP_TESTS ON)
set(SHADERC_ENABLE_TESTS OFF)
add_subdirectory(shaderc)

message(STATUS "Loading spirv-cross...")
add_subdirectory(SPIRV-Cross)


message(STATUS "Loading glm...")
add_subdirectory(glm)
target_compile_definitions(glm INTERFACE
    GLM_FORCE_SWIZZLE
    GLM_FORCE_RADIANS
    GLM_FORCE_CTOR_INIT
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

message(STATUS "Loading tinygltf...")
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(tinygltf)


message(STATUS "Loading argparse...")
add_subdirectory(argparse)



message(STATUS "Loading EnTT...")
add_subdirectory(entt)
