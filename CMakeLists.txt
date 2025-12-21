# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.15)

include(FetchContent)

file( STRINGS "VERSION" VOXELPIC_VERSION_FILE )

string( REPLACE "." ";" VOXELPIC_VERSION_LIST ${VOXELPIC_VERSION_FILE} )

list( GET VOXELPIC_VERSION_LIST 0 VOXELPIC_VERSION_MAJOR )

list( GET VOXELPIC_VERSION_LIST 1 VOXELPIC_VERSION_MINOR )

list( GET VOXELPIC_VERSION_LIST 2 VOXELPIC_VERSION_REVISION )

set( VOXELPIC_VERSION ${VOXELPIC_VERSION_MAJOR}.${VOXELPIC_VERSION_MINOR}.${VOXELPIC_VERSION_REVISION} )

message("Configure VOXELPIC_VERSION at ${VOXELPIC_VERSION}")

project( voxelpic VERSION ${VOXELPIC_VERSION} LANGUAGES C)

option(VOXELPIC_BUILD_TOOLS "Specifies whether to build the tools" OFF)
option(VOXELPIC_BUILD_TESTS "Specifies whether to build the tests" OFF)
option(VOXELPIC_BUILD_SHARED "Whether to build the voxelpic_shared library" OFF)
set(VOXELPIC_SANITIZE "" CACHE STRING "Argument to pass to sanitize (disabled by default)")

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_FORMAT NAMES clang-format-18 )

string(COMPARE EQUAL ${CLANG_FORMAT} "CLANG_FORMAT-NOTFOUND" CLANG_FORMAT_NOT_FOUND)
if(CLANG_FORMAT_NOT_FOUND)
  message("voxelpic_format target not defined: no clang-format tool found")
else()
  file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS
        src/*.c
        src/*.h
        test/*.c
        test/*.h
        include/voxelpic/*.h
  )

  add_custom_target(voxelpic_format
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    COMMAND ${CLANG_FORMAT}
                    -i
                    ${ALL_SOURCE_FILES})
endif()

add_subdirectory(src)

if(VOXELPIC_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(VOXELPIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

set(INSTALL_CONFIGDIR cmake)
set(INSTALL_LIBDIR lib)
set(INSTALL_INCLUDEDIR include)
set(VOXELPIC_INSTALL_TARGETS voxelpic)

if(VOXELPIC_BUILD_SHARED)
  list(APPEND VOXELPIC_INSTALL_TARGETS voxelpic_shared)
endif()

install(TARGETS ${VOXELPIC_INSTALL_TARGETS}
  EXPORT ${PROJECT_NAME}_Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION
  ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(EXPORT ${PROJECT_NAME}_Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

export(EXPORT ${PROJECT_NAME}_Targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::)

export(PACKAGE ${PROJECT_NAME})

install(DIRECTORY include/ DESTINATION ${INSTALL_INCLUDEDIR})
