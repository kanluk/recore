add_recore_executable(volume_pathtracer)

target_sources(volume_pathtracer PRIVATE
    volume_pathtracer.cpp
)

target_link_libraries(volume_pathtracer PUBLIC
    recore
)
 