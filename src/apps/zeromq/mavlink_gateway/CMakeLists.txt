add_executable(goby_mavlink_gateway mavlink_gateway.cpp)
target_link_libraries(goby_mavlink_gateway goby goby_zeromq)
generate_middleware_interfaces(goby_mavlink_gateway)
