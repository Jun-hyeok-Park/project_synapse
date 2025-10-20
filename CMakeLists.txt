cmake_minimum_required(VERSION 3.14)
project(project_synapse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ────────────────────────────────
# 1️⃣ 기본 설정
# ────────────────────────────────
message(STATUS "🔧 Configuring Project SYNAPSE")

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROJECT_ROOT}/common)

# ────────────────────────────────
# 2️⃣ vsomeip3 (라즈베리파이 기준)
# ────────────────────────────────
find_path(VSOMEIP_INCLUDE_DIR vsomeip/vsomeip.hpp
          PATHS /usr/local/include /usr/include)

find_library(VSOMEIP_LIB vsomeip3 PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_CFG_LIB vsomeip3-cfg PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_E2E_LIB vsomeip3-e2e PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_SD_LIB vsomeip3-sd PATHS /usr/local/lib /usr/lib)

if(NOT VSOMEIP_LIB)
  message(FATAL_ERROR "❌ vsomeip3 not found. Please check /usr/local/lib")
else()
  message(STATUS "✅ Found vsomeip3 libs: ${VSOMEIP_LIB}")
endif()

# ────────────────────────────────
# 3️⃣ Boost.System (vsomeip 종속)
# ────────────────────────────────
find_library(BOOST_SYSTEM_LIB boost_system PATHS /usr/lib /usr/local/lib)
if(NOT BOOST_SYSTEM_LIB)
  message(FATAL_ERROR "❌ boost_system not found. Run: sudo apt-get install -y libboost-system-dev")
else()
  message(STATUS "✅ Found Boost.System: ${BOOST_SYSTEM_LIB}")
endif()

# ────────────────────────────────
# 4️⃣ pybind11 (선택)
# ────────────────────────────────
find_package(pybind11 QUIET)
if(NOT pybind11_FOUND)
  message(STATUS "ℹ️ pybind11 not found, checking /usr/include")
  include_directories(/usr/include)
endif()

# ────────────────────────────────
# 5️⃣ 공통 헤더-only 라이브러리
# ────────────────────────────────
add_library(common INTERFACE)
target_include_directories(common INTERFACE ${PROJECT_ROOT}/common)
target_link_libraries(common INTERFACE pthread)

# ────────────────────────────────
# 6️⃣ 하위 모듈 (Server / Client / Bindings)
# ────────────────────────────────
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(bindings)

# ────────────────────────────────
# 7️⃣ GUI 클라이언트 (Qt + vSomeIP)
# ────────────────────────────────
option(BUILD_GUI_CLIENT "Build Qt-based GUI Client" ON)

if(BUILD_GUI_CLIENT)
  message(STATUS "🖥️  Building Qt GUI Client")

  # Qt Widgets 찾기
  find_package(Qt6 REQUIRED COMPONENTS Widgets)

  add_subdirectory(gui_client)

  message(STATUS "✅ GUI Client build enabled")
else()
  message(STATUS "🚫 Skipping GUI client build (BUILD_GUI_CLIENT=OFF)")
endif()

# ────────────────────────────────
# 8️⃣ 빌드 후 안내 메시지
# ────────────────────────────────
message(STATUS "")
message(STATUS "✅ Build complete!")
message(STATUS "To use Python binding:")
message(STATUS "  export PYTHONPATH=${CMAKE_BINARY_DIR}/bindings:\$PYTHONPATH")
message(STATUS "")
