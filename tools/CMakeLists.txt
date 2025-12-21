FetchContent_Declare(
  cargs
  GIT_REPOSITORY https://github.com/likle/cargs
  GIT_TAG        v1.2.0
)

FetchContent_MakeAvailable(cargs)

find_package(PNG)


find_package(Git REQUIRED)

execute_process(
  COMMAND git log -1 --format=%H
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE VPIC_GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE VPIC_GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE VPIC_GIT_SHORT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

execute_process(
  COMMAND git log -1 --format=%aD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE VPIC_BUILD_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

set(VPIC_BUILD_TOOLCHAIN "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

add_executable(vpic vpic.c)

target_link_libraries(vpic
  PRIVATE
    voxelpic::voxelpic
    cargs
)

target_include_directories(vpic
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

if( PNG_FOUND )
  message("libpng found: include_dirs=[${PNG_INCLUDE_DIRS}] libraries=${PNG_LIBRARIES}")
  target_include_directories(vpic PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(vpic PRIVATE ${PNG_LIBRARIES})
  target_compile_definitions(vpic PRIVATE VOXELPIC_PNG)
else()
  message("libpng not found: vpic PNG I/O disabled")
endif()

find_package(Python REQUIRED)

add_custom_command(TARGET vpic POST_BUILD
                  COMMAND echo Generating sample cloud file
                  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cloud_gen.py $<TARGET_FILE_DIR:vpic>/sample_cloud.dat)
