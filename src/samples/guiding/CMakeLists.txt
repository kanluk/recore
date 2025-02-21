add_recore_executable(guiding)

target_sources(guiding PRIVATE
  guiding.cpp
)

target_link_libraries(guiding PUBLIC
    recore
)
