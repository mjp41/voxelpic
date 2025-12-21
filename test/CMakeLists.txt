add_executable(voxel_test voxel_test.c)
target_link_libraries(voxel_test
  PRIVATE 
  voxelpic::voxelpic)

add_executable(color_test color_test.c)
target_link_libraries(color_test
  PRIVATE
  voxelpic::voxelpic)

add_executable(image_test image_test.c)
target_link_libraries(image_test
  PRIVATE
  voxelpic::voxelpic)


add_test(NAME voxel_test_1 COMMAND voxel_test voxel_1.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_2 COMMAND voxel_test voxel_2.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_3 COMMAND voxel_test voxel_3.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_4 COMMAND voxel_test voxel_4.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_5 COMMAND voxel_test voxel_5.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME color_test   COMMAND color_test              WORKING_DIRECTORY $<TARGET_FILE_DIR:color_test>)
add_test(NAME image_test_7 COMMAND image_test image_7.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)
add_test(NAME image_test_8 COMMAND image_test image_8.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)
add_test(NAME image_test_9 COMMAND image_test image_9.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)


find_package(Python REQUIRED)

add_custom_command(TARGET voxel_test POST_BUILD
                  COMMAND echo Generating voxel test files
                  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/voxel_test_gen.py $<TARGET_FILE_DIR:voxel_test>)
add_custom_command(TARGET image_test POST_BUILD
                  COMMAND echo Generating image test files
                  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/image_test_gen.py $<TARGET_FILE_DIR:image_test>)
