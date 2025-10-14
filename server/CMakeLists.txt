add_executable(veh_server
    src/veh_server_main.cpp
    src/veh_aeb_server.cpp
    src/veh_can.cpp
)

target_include_directories(veh_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veh_server PRIVATE common)
