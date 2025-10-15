cmake_minimum_required(VERSION 3.14)
project(project_synapse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ────────────────────────────────
# 1️. 기본 설정
# ────────────────────────────────
message(STATUS "Configuring Project SYNAPSE")

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROJECT_ROOT}/common)

# ────────────────────────────────
# 2️. vsomeip3 (라즈베리파이 경로 기준)
# ────────────────────────────────
find_path(VSOMEIP_INCLUDE_DIR vsomeip/vsomeip.hpp
          PATHS /usr/local/include /usr/include)

find_library(VSOMEIP_LIB vsomeip3 PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_CFG_LIB vsomeip3-cfg PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_E2E_LIB vsomeip3-e2e PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_SD_LIB vsomeip3-sd PATHS /usr/local/lib /usr/lib)

if(NOT VSOMEIP_LIB)
  message(FATAL_ERROR "vsomeip3 not found. Please check /usr/local/lib")
else()
  message(STATUS "Found vsomeip3 libs: ${VSOMEIP_LIB}")
endif()

# ────────────────────────────────
# 3️. Boost.System (vsomeip 종속)
# ────────────────────────────────
find_library(BOOST_SYSTEM_LIB boost_system PATHS /usr/lib /usr/local/lib)
if(NOT BOOST_SYSTEM_LIB)
  message(FATAL_ERROR "boost_system not found. Run: sudo apt-get install -y libboost-system-dev")
else()
  message(STATUS "Found Boost.System: ${BOOST_SYSTEM_LIB}")
endif()

# ────────────────────────────────
# 4️. pybind11
# ────────────────────────────────
find_package(pybind11 QUIET)
if(NOT pybind11_FOUND)
  message(STATUS "pybind11 not found, checking /usr/include")
  include_directories(/usr/include)
endif()

# ────────────────────────────────
# 5️. 공통 헤더-only 라이브러리
# ────────────────────────────────
add_library(common INTERFACE)
target_include_directories(common INTERFACE ${PROJECT_ROOT}/common)
target_link_libraries(common INTERFACE pthread)

# ────────────────────────────────
# 6️. 하위 모듈 (Server / Client)
# ────────────────────────────────
add_subdirectory(server)
add_subdirectory(client)

# ────────────────────────────────
# 7️. Python Binding (pybind11)
# ────────────────────────────────
if(pybind11_FOUND)
  message(STATUS "Building Python binding (synapse_vsomeip)")
  pybind11_add_module(synapse_vsomeip bindings/synapse_vsomeip.cpp)
  target_include_directories(synapse_vsomeip PRIVATE ${PROJECT_ROOT} ${PROJECT_ROOT}/common)
  target_link_libraries(synapse_vsomeip
    PRIVATE
    ${VSOMEIP_LIB}
    ${VSOMEIP_CFG_LIB}
    ${VSOMEIP_SD_LIB}
    ${VSOMEIP_E2E_LIB}
    ${BOOST_SYSTEM_LIB}
    pthread dl)
else()
  message(WARNING "pybind11 not available; skipping Python module build.")
endif()

# ────────────────────────────────
# 8️. 빌드 후 안내 메시지
# ────────────────────────────────
message(STATUS "")
message(STATUS "Build complete!")
message(STATUS "To use in Python:")
message(STATUS "  export PYTHONPATH=${CMAKE_BINARY_DIR}/bindings:\$PYTHONPATH")
message(STATUS "")
