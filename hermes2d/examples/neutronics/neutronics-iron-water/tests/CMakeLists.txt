if(NOT H2D_REAL)
    return()
endif(NOT H2D_REAL)

if (WITH_EXODUSII)

project(test-example-iron-water)

add_executable(${PROJECT_NAME} main.cpp)
include (${hermes2d_SOURCE_DIR}/CMake.common)
set_common_target_properties(${PROJECT_NAME})
set(BIN ${PROJECT_BINARY_DIR}/${PROJECT_NAME})
add_test(test-example-iron-water ${BIN})

endif (WITH_EXODUSII)
