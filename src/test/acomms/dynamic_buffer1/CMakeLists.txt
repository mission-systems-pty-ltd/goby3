#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS test.proto)

add_executable(goby_test_dynamic_buffer1 test.cpp ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(goby_test_dynamic_buffer1 goby)

add_test(goby_test_dynamic_buffer1 ${goby_BIN_DIR}/goby_test_dynamic_buffer1)
