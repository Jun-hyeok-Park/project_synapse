cmake_minimum_required(VERSION 3.14)

# ────────────────────────────────
# Server Executables
# ────────────────────────────────
add_executable(veh_control_server veh_control_server.cpp veh_can_tx.cpp)
add_executable(veh_status_publisher veh_status_publisher.cpp)

# ────────────────────────────────
# 공통 링크 대상
# ────────────────────────────────
set(VSOMEIP_LIBS
    ${VSOMEIP_LIB}
    ${VSOMEIP_CFG_LIB}
    ${VSOMEIP_SD_LIB}
    ${VSOMEIP_E2E_LIB}
    ${BOOST_SYSTEM_LIB}
    pthread
    dl
    stdc++fs
)

# ────────────────────────────────
# Link
# ────────────────────────────────
target_link_libraries(veh_control_server PRIVATE common ${VSOMEIP_LIBS})
target_link_libraries(veh_status_publisher PRIVATE common ${VSOMEIP_LIBS})

set_target_properties(
    veh_control_server
    veh_status_publisher
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
