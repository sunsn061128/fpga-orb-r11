cmake_minimum_required(VERSION 3.16)
project(fpga_orb_fastv2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ORB_REFERENCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../FPGA_ORB_SHARED_v1.0"
    CACHE PATH "Reference package root")
if(NOT EXISTS "${ORB_REFERENCE_ROOT}/golden.cpp")
  message(FATAL_ERROR "Reference package not found: ${ORB_REFERENCE_ROOT}")
endif()

add_library(fastv2_model src/fastv2_hls.cpp)
target_include_directories(fastv2_model PUBLIC include)
if(MSVC)
  target_compile_options(fastv2_model PRIVATE /W4)
else()
  target_compile_options(fastv2_model PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unknown-pragmas -Wno-unused-label)
endif()

add_executable(fastv2_contract_test
  hls/tb_fastv2.cpp
  "${ORB_REFERENCE_ROOT}/golden.cpp")
target_include_directories(fastv2_contract_test PRIVATE
  include "${ORB_REFERENCE_ROOT}")
target_link_libraries(fastv2_contract_test PRIVATE fastv2_model)

add_executable(fastv2_resize_contract_test hls/tb_resize.cpp)
target_include_directories(fastv2_resize_contract_test PRIVATE include)
target_link_libraries(fastv2_resize_contract_test PRIVATE fastv2_model)

add_executable(fastv2_export_ps_reference host/export_ps_reference.cpp
  "${ORB_REFERENCE_ROOT}/golden.cpp")
target_include_directories(fastv2_export_ps_reference PRIVATE "${ORB_REFERENCE_ROOT}")

set(FASTV2_BOARD_INPUT "${ORB_REFERENCE_ROOT}/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm")
add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/ps/fastv2_reference.inc"
                          "${CMAKE_CURRENT_SOURCE_DIR}/ps/reference_pyramid.bin"
  COMMAND fastv2_export_ps_reference "${FASTV2_BOARD_INPUT}"
          "${CMAKE_CURRENT_SOURCE_DIR}/ps/fastv2_reference.inc"
          "${CMAKE_CURRENT_SOURCE_DIR}/ps/reference_pyramid.bin"
  DEPENDS fastv2_export_ps_reference "${FASTV2_BOARD_INPUT}"
  VERBATIM)
add_custom_target(fastv2_ps_reference ALL
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ps/fastv2_reference.inc"
          "${CMAKE_CURRENT_SOURCE_DIR}/ps/reference_pyramid.bin")

enable_testing()
if(WIN32)
  add_executable(fastv2_ps_contract_test host/test_ps_benchmark.cpp)
  add_dependencies(fastv2_ps_contract_test fastv2_ps_reference)
  target_include_directories(fastv2_ps_contract_test PRIVATE host)
  target_link_libraries(fastv2_ps_contract_test PRIVATE fastv2_model)
  if(NOT MSVC)
    target_compile_options(fastv2_ps_contract_test PRIVATE -fno-strict-aliasing -Wall -Wextra)
  endif()
  add_test(NAME fastv2_ps_mailbox_and_faults COMMAND fastv2_ps_contract_test
    "${FASTV2_BOARD_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/ps/reference_pyramid.bin")
endif()
add_test(NAME fastv2_exact_front_contract COMMAND fastv2_contract_test)
add_test(NAME fastv2_exact_resize_contract COMMAND fastv2_resize_contract_test)
add_test(NAME fastv2_fullframe_contract
  COMMAND fastv2_contract_test
    "${ORB_REFERENCE_ROOT}/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm")
