add_executable(veh_control_server
    veh_control_server.cpp
    veh_can_tx.cpp
)
add_executable(veh_status_publisher
    veh_status_publisher.cpp
)

target_link_libraries(veh_control_server
    PRIVATE common
    ${VSOMEIP_LIB}
    ${VSOMEIP_CFG_LIB}
    ${VSOMEIP_SD_LIB}
    ${VSOMEIP_E2E_LIB}
    ${BOOST_SYSTEM_LIB}
    pthread dl
)

target_link_libraries(veh_status_publisher
    PRIVATE common
    ${VSOMEIP_LIB}
    ${VSOMEIP_CFG_LIB}
    ${VSOMEIP_SD_LIB}
    ${VSOMEIP_E2E_LIB}
    ${BOOST_SYSTEM_LIB}
    pthread dl
)
