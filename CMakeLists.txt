# 루트 CMakeLists.txt (발췌)
cmake_minimum_required(VERSION 3.14)
project(project_synapse)

set(CMAKE_CXX_STANDARD 17)

# pybind11 찾기 (패키지 or subdirectory)
find_package(pybind11 QUIET)
if(NOT pybind11_FOUND)
  message(STATUS "pybind11 not found via find_package, using subdirectory")
  # 외부에 pybind11 소스가 있다면:
  # add_subdirectory(external/pybind11)
endif()

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

# 바인딩 모듈
if(pybind11_FOUND)
  pybind11_add_module(synapse_vsomeip bindings/synapse_vsomeip.cpp)
  target_include_directories(synapse_vsomeip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(synapse_vsomeip PRIVATE vsomeip pthread)
else()
  message(WARNING "pybind11 not available; Python module will not be built.")
endif()
