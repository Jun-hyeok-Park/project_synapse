cmake_minimum_required(VERSION 3.10)
project(veh_server)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vsomeip 라이브러리 경로 설정 (필요 시 수정)
include_directories(include)
link_directories(/usr/local/lib)

# 소스 파일 추가
add_executable(veh_server
    src/veh_server_main.cpp
    src/veh_drive_server.cpp
    src/veh_aeb_server.cpp
    src/veh_fcw_server.cpp
    src/veh_autopark_server.cpp
    src/veh_auth_server.cpp
    src/veh_tof_server.cpp
    src/veh_fault_server.cpp
    src/veh_can.cpp
)

# 라이브러리 링크
target_link_libraries(veh_server
    vsomeip3
    pthread
)
