add_recore_executable(photon_mapping)

target_sources(photon_mapping PRIVATE
  photon_mapping.cpp
)

target_link_libraries(photon_mapping PUBLIC
    recore
)
