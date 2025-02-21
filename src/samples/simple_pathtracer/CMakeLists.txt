add_recore_executable(simple_pathtracer)

target_sources(simple_pathtracer PRIVATE
    simple_pathtracer.cpp
)

target_link_libraries(simple_pathtracer PUBLIC
    recore
)
