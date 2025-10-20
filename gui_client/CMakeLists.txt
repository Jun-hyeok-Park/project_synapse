cmake_minimum_required(VERSION 3.14)

project(gui_client LANGUAGES CXX)

# ✅ Qt 자동 처리 활성화
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ Qt6 찾기
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ✅ include
include_directories(
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/server
)

# ✅ vsomeip 라이브러리 찾기
find_library(VSOMEIP_LIB vsomeip3 PATHS /usr/local/lib /usr/lib)
find_library(VSOMEIP_CFG_LIB vsomeip3-cfg PATHS /usr/local/lib /usr/lib)

# ✅ 소스파일 지정
set(SOURCES
    main.cpp
    mainwindow.cpp
    vsclientthread.cpp
)

set(HEADERS
    mainwindow.h
    vsclientthread.h
)

# ✅ 실행 파일 생성
add_executable(gui_client ${SOURCES} ${HEADERS})

# ✅ 링크
target_link_libraries(gui_client
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${VSOMEIP_LIB}
    ${VSOMEIP_CFG_LIB}
    pthread
)
