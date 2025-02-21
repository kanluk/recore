add_subdirectory(vulkan)

add_library(recore STATIC)

target_sources(recore PRIVATE
    core/application.cpp
    core/window.cpp

    scene/scene.cpp
    scene/gpu_scene.cpp
    scene/camera.cpp
    scene/ecs.cpp

    passes/blit/blit.cpp
    passes/gbuffer/gbuffer.cpp
    passes/rsm/rsm.cpp
    passes/lpv/lpv.cpp
    passes/diffuse_pathtracer/diffuse_pathtracer.cpp
    passes/debug/distribution.cpp
    passes/prefixsum/prefixsum.cpp
    passes/gui/gui.cpp
    passes/accumulator/accumulator.cpp
    passes/tone_mapping/tone_mapping.cpp
    passes/svgf/svgf.cpp
    passes/pm_pathtracer/pm_pathtracer.cpp
    passes/photontracer/photontracer.cpp
    passes/taa/taa.cpp
    passes/guided_pathtracer/guided_pathtracer.cpp
    passes/volume_pathtracer/volume_pathtracer.cpp

)

target_link_libraries(recore PUBLIC
    glfw
    imgui
    glm
    tinygltf
    EnTT
    recore-vulkan
)

target_include_directories(recore PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

target_compile_definitions(recore PUBLIC
    RECORE_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
    RECORE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../assets/"
    RECORE_EVAL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../eval/"
)
