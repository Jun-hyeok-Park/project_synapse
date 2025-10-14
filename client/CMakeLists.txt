add_executable(veh_client
    src/main.cpp
)

target_include_directories(veh_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veh_client PRIVATE common)
