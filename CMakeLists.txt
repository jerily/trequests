cmake_minimum_required(VERSION 3.22.1)
project(trequests VERSION 1.0.0 LANGUAGES C)

set(TARGET trequests)
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

if(WIN32)
    # Prefix all shared libraries with 'lib'.
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    # Prefix all static libraries with 'lib'.
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(CURL REQUIRED)
message(STATUS "CURL_LIBRARIES: ${CURL_LIBRARIES}")
message(STATUS "CURL_INCLUDE_DIRS: ${CURL_INCLUDE_DIRS}")

find_package(TCL 8.6.13 REQUIRED)  # TCL_INCLUDE_PATH TCL_LIBRARY
find_program(TCL_TCLSH
  NAMES
      tclsh86 tclsh8.6
      tclsh90 tclsh9.0
      tclsh
  PATHS
      ${TCL_INCLUDE_PATH}/../bin
      ${TCL_INCLUDE_PATH}/../../bin
  NO_DEFAULT_PATH
)

message(STATUS "TCL_INCLUDE_PATH: ${TCL_INCLUDE_PATH}")
message(STATUS "TCL_LIBRARY: ${TCL_LIBRARY}")
message(STATUS "TCL_TCLSH: ${TCL_TCLSH}")

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(TCL_THREADS VERSION=${PROJECT_VERSION})

if ("${ADDRESS_SANITIZER}" STREQUAL "ON")
    add_compile_options(-fPIC -g -fsanitize=undefined -fsanitize=address)
    add_link_options(-fsanitize=undefined -fsanitize=address)
    add_compile_definitions(TCL_MEM_DEBUG=1)
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_compile_definitions(TCL_MEM_DEBUG=1)
    set(CMAKE_C_FLAGS "-g ${CMAKE_C_FLAGS}")
else()
    # NDEBUG to not generate code for assert
    add_compile_definitions(NDEBUG)
endif ()

#set(CMAKE_VERBOSE_MAKEFILE ON)

#set(CMAKE_CTEST_ARGUMENTS "--verbose")
set(CMAKE_CTEST_ARGUMENTS "--verbose" "--output-on-failure")
enable_testing()
add_test(NAME AllUnitTests
    COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/all.tcl -verbose "body error start"
)
set_tests_properties(AllUnitTests
    PROPERTIES ENVIRONMENT TCLLIBPATH=${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(shell ${CMAKE_COMMAND} -E env TCLLIBPATH=${CMAKE_CURRENT_BINARY_DIR} ${TCL_TCLSH}
    USES_TERMINAL
    DEPENDS ${TARGET})

add_library(trequests SHARED
    src/testing.h
    src/library.c
    src/library.h
    src/common.c
    src/common.h
    src/treqSession.c
    src/treqSession.h
    src/treqRequest.c
    src/treqRequest.h
    src/treqRequestAuth.c
    src/treqRequestAuth.h
    src/treqPool.c
    src/treqPool.h
)

if ("${TREQUESTS_TESTING_MODE}" STREQUAL "ON")
    add_compile_definitions(TREQUESTS_TESTING_MODE=1)
    target_sources(trequests PRIVATE src/testing.c)
endif()

set_target_properties(trequests PROPERTIES POSITION_INDEPENDENT_CODE ON)

include_directories(${TCL_INCLUDE_PATH} ${CURL_INCLUDE_DIRS})
target_link_libraries(trequests PRIVATE ${TCL_LIBRARY} ${CURL_LIBRARIES})
get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)
