cmake_minimum_required(VERSION 3.14)

# ────────────────────────────────
# Client Executables
# ────────────────────────────────
add_executable(veh_control_client veh_control_client.cpp)
add_executable(veh_status_subscriber veh_status_subscriber.cpp)
add_executable(veh_cli veh_cli.cpp)

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
target_link_libraries(veh_control_client PRIVATE common ${VSOMEIP_LIBS})
target_link_libraries(veh_status_subscriber PRIVATE common ${VSOMEIP_LIBS})
target_link_libraries(veh_cli PRIVATE common ${VSOMEIP_LIBS})

set_target_properties(
    veh_control_client
    veh_status_subscriber
    veh_cli
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
