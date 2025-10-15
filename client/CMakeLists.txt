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
)

# ────────────────────────────────
# 각 실행파일에 라이브러리 연결
# ────────────────────────────────
target_link_libraries(veh_control_client PRIVATE common ${VSOMEIP_LIBS})
target_link_libraries(veh_status_subscriber PRIVATE common ${VSOMEIP_LIBS})
target_link_libraries(veh_cli PRIVATE common ${VSOMEIP_LIBS})
