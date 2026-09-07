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

add_executable(api_test api_test.c)
target_link_libraries(api_test
  PRIVATE
  voxelpic::voxelpic)


add_test(NAME voxel_test_1 COMMAND voxel_test 1 voxel_1_input.dat voxel_1_expected.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_2 COMMAND voxel_test 2 voxel_2_input.dat voxel_2_expected.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_3 COMMAND voxel_test 3 voxel_3_input.dat voxel_3_expected.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_4 COMMAND voxel_test 4 voxel_4_input.dat voxel_4_expected.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME voxel_test_5 COMMAND voxel_test 5 voxel_5_input.dat voxel_5_expected.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:voxel_test>)
add_test(NAME color_test   COMMAND color_test              WORKING_DIRECTORY $<TARGET_FILE_DIR:color_test>)
add_test(NAME image_test_7 COMMAND image_test 7 image_7_cloud.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)
add_test(NAME image_test_8 COMMAND image_test 8 image_8_cloud.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)
add_test(NAME image_test_9 COMMAND image_test 9 image_9_cloud.dat WORKING_DIRECTORY $<TARGET_FILE_DIR:image_test>)
add_test(NAME api_test     COMMAND api_test                WORKING_DIRECTORY $<TARGET_FILE_DIR:api_test>)


find_package(Python REQUIRED)

add_custom_command(TARGET voxel_test POST_BUILD
                  COMMAND echo Generating voxel test files
                  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/voxel_test_gen.py $<TARGET_FILE_DIR:voxel_test>)
add_custom_command(TARGET image_test POST_BUILD
                  COMMAND echo Generating image test files
                  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/image_test_gen.py $<TARGET_FILE_DIR:image_test>)
