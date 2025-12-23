# cmake version
cmake_minimum_required(VERSION 3.10)

# copy shaders to build dir
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()
if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

project(FinalArmLab)

aux_source_directory(src PROJECT_SOURCES)
add_executable(FinalArmLab ${PROJECT_SOURCES})
target_include_directories(FinalArmLab PRIVATE include)

if(APPLE)
  find_package(GLUT REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(glm CONFIG REQUIRED)
  target_link_libraries(FinalArmLab PRIVATE ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})
  target_link_libraries(FinalArmLab PRIVATE glfw)
  target_link_libraries(FinalArmLab PRIVATE glm::glm)
  target_compile_definitions(FinalArmLab PRIVATE GL_SILENCE_DEPRECATION)
else()
  find_package(glad CONFIG REQUIRED)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(glm CONFIG REQUIRED)
  target_link_libraries(FinalArmLab PRIVATE glad::glad)
  target_link_libraries(FinalArmLab PRIVATE glfw)
  target_link_libraries(FinalArmLab PRIVATE glm::glm)
endif(APPLE)
