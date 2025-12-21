add_library(voxelpic STATIC voxelpic.c)

add_library(voxelpic::voxelpic ALIAS voxelpic)


if(NOT MSVC)
  target_link_libraries(voxelpic
    PUBLIC
      m
  )
  target_compile_options(voxelpic
    PRIVATE
      -Wall
  )
endif()

target_include_directories( voxelpic
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/voxelpic>
)

if (VOXELPIC_SANITIZE)
  target_compile_options(voxelpic PUBLIC -g -fsanitize=${VOXELPIC_SANITIZE} -fno-omit-frame-pointer)
  target_link_libraries(voxelpic PUBLIC -fsanitize=${VOXELPIC_SANITIZE})
endif()


if ( VOXELPIC_SHARED )
  add_library( voxelpic_shared SHARED voxelpic.c)

  if(MSVC)
    target_compile_definitions(voxelpic_shared PRIVATE "VOXELPIC_SHARED")
  else()
    target_link_libraries(voxelpic_shared
      PUBLIC
        m
    )
  endif()

  target_include_directories( voxelpic_shared
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/voxelpic>
  )
endif()