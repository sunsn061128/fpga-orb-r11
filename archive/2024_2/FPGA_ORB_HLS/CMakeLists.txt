cmake_minimum_required(VERSION 3.16)
project(fpga_orb_hls LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ORB_REFERENCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../FPGA_ORB_SHARED_v1.0" CACHE PATH "Reference package root")
if(NOT EXISTS "${ORB_REFERENCE_ROOT}/golden.cpp")
  message(FATAL_ERROR "Reference package not found: ${ORB_REFERENCE_ROOT}")
endif()

add_library(orb_hls_model src/orb_hls.cpp)
target_include_directories(orb_hls_model PUBLIC include "${ORB_REFERENCE_ROOT}")
if(MSVC)
  target_compile_options(orb_hls_model PRIVATE /W4)
else()
  target_compile_options(orb_hls_model PRIVATE -Wall -Wextra -Wpedantic -Wno-unknown-pragmas)
endif()

add_executable(orb_hls_cli host/orb_hls_cli.cpp "${ORB_REFERENCE_ROOT}/golden.cpp")
target_include_directories(orb_hls_cli PRIVATE include "${ORB_REFERENCE_ROOT}")
target_link_libraries(orb_hls_cli PRIVATE orb_hls_model)

add_executable(orb_level_contract_test hls/tb_level.cpp)
target_compile_definitions(orb_level_contract_test PRIVATE ORB_HLS_COSIM_SMALL_DEPTH)
target_link_libraries(orb_level_contract_test PRIVATE orb_hls_model)

add_executable(orb_fast_stream_test
  hls/tb_fast_stream.cpp
  "${ORB_REFERENCE_ROOT}/golden.cpp")
target_include_directories(orb_fast_stream_test PRIVATE include "${ORB_REFERENCE_ROOT}")
target_link_libraries(orb_fast_stream_test PRIVATE orb_hls_model)

add_executable(orb_fullframe_pipeline_test hls/tb_fullframe_pipeline.cpp)
target_link_libraries(orb_fullframe_pipeline_test PRIVATE orb_hls_model)

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME level_t2_capability_contract COMMAND orb_level_contract_test)
add_test(NAME fast_stream_v1_candidate_contract COMMAND orb_fast_stream_test)
add_test(NAME fullframe_pipeline_contract
  COMMAND orb_fullframe_pipeline_test
          "${ORB_REFERENCE_ROOT}/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm"
          "${ORB_REFERENCE_ROOT}/vectors/flight_20261004T184929_998838_6bb93c56_f0690/input.pgm")
add_test(NAME prepare_vector_random_L4_T2_normal
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_BINARY_DIR}/replay_random_L4_T2_normal")
set_tests_properties(prepare_vector_random_L4_T2_normal PROPERTIES FIXTURES_SETUP vector_random_fixture)
add_test(NAME vector_random_L4_T2_normal
  COMMAND "${Python3_EXECUTABLE}" "${ORB_REFERENCE_ROOT}/scripts/replay_vectors.py"
          --binary $<TARGET_FILE:orb_hls_cli>
          --output "${CMAKE_BINARY_DIR}/replay_random_L4_T2_normal"
          --case random_seed_73 --profile L4_T2 --role normal)
set_tests_properties(vector_random_L4_T2_normal PROPERTIES FIXTURES_REQUIRED vector_random_fixture)

add_test(NAME reject_unsupported_grid_top_k
  COMMAND $<TARGET_FILE:orb_hls_cli>
          --input "${ORB_REFERENCE_ROOT}/vectors/random_seed_73/input.pgm"
          --output "${CMAKE_BINARY_DIR}/unsupported_grid_top_k.orb"
          --grid-top-k 2)
set_tests_properties(reject_unsupported_grid_top_k PROPERTIES WILL_FAIL TRUE)
