# project information
project(TxParser
        VERSION 1.0
        DESCRIPTION "Transaction parser of Boilerplate app"
        LANGUAGES C)

# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# set one specific SANITIZER for workflow
if(DEFINED ENV{SANITIZER} AND (ENV{SANITIZER} STREQUAL "address" OR ENV{SANITIZER} STREQUAL "memory" OR ENV{SANITIZER} STREQUAL "undefined"))
    set(CMAKE_C_FLAGS_DEBUG
        "${CMAKE_C_FLAGS_DEBUG} -Werror -Wall -Wextra -Wno-unused-function -DFUZZ -pedantic -g -O0 -fsanitize=fuzzer,$ENV{SANITIZER}"
    )
else()
    set(CMAKE_C_FLAGS_DEBUG
        "${CMAKE_C_FLAGS_DEBUG} -Werror -Wall -Wextra -Wno-unused-function -DFUZZ -pedantic -g -O0"
    )
endif()


add_library(txparser
    ${BOLOS_SDK}/lib_standard_app/format.c
    ${BOLOS_SDK}/lib_standard_app/buffer.c
    ${BOLOS_SDK}/lib_standard_app/read.c
    ${BOLOS_SDK}/lib_standard_app/varint.c
    ${BOLOS_SDK}/lib_standard_app/bip32.c
    ${BOLOS_SDK}/lib_standard_app/write.c
    ${BOLOS_SDK}/lib_cxng/src/cx_hash.c
    ${BOLOS_SDK}/lib_cxng/src/cx_ram.c
    ${BOLOS_SDK}/lib_cxng/src/cx_sha256.c
    ${BOLOS_SDK}/lib_cxng/src/cx_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/base64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/bits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/cell.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/crc16.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/encoding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/format_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/format_bigint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/hints.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/int256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/mybuffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/myformat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/myread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/transaction_hints.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/deserialize.c
)

set_target_properties(txparser PROPERTIES SOVERSION 1)

target_include_directories(txparser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${BOLOS_SDK}/lib_standard_app
    ${BOLOS_SDK}/include
    ${BOLOS_SDK}/lib_cxng/include
    ${BOLOS_SDK}/lib_cxng/src
    ${BOLOS_SDK}/target/${TARGET}/include
    #${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction
)