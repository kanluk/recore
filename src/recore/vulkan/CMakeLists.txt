add_library(recore-vulkan STATIC)

target_sources(recore-vulkan PRIVATE
    api/instance.cpp
    api/device.cpp
    api/command.cpp
    api/image.cpp
    api/swapchain.cpp
    api/synchronization.cpp
    api/renderpass.cpp
    api/pipeline.cpp
    api/descriptor.cpp
    api/buffer.cpp
    api/acceleration.cpp
    api/queries.cpp

    context.cpp
    shader_library.cpp
    debug_messenger.cpp
)

target_link_libraries(recore-vulkan PUBLIC
    VulkanMemoryAllocator
    volk
    shaderc
    shaderc_util
    spirv-cross-glsl
)

target_include_directories(recore-vulkan PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)
