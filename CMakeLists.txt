cmake_minimum_required(VERSION 3.28.2)

project(recore VERSION 0.0.1 DESCRIPTION "Rendering & Compute Research Framework")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()


function(add_recore_executable target)
  add_executable(${target})
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endfunction()

add_subdirectory(external)
add_subdirectory(src)
