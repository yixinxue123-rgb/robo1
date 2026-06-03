cmake_minimum_required(VERSION 3.10)
project(armor_detector)

# 显式指定 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 关闭编译器特定扩展，确保代码平台通用性

# 构建类型配置（如果是 Debug 模式可以方便用 VS Code 打断点调试）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 编译器警告设置
if(MSVC)
  add_compile_options(/W4)
else()
  # 提示：MSYS2 环境下走这里，-pthread 确保多线程正常（OpenCV 依赖多线程）
  add_compile_options(-Wall -Wextra -pthread)
endif()

# 寻找 OpenCV 库
find_package(OpenCV REQUIRED)

# 打印 OpenCV 状态信息（用于在 VS Code 控制台排查问题）
if(OpenCV_FOUND)
  message(STATUS "OpenCV found successfully!")
  message(STATUS "OpenCV version: ${OpenCV_VERSION}")
  message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
else()
  message(FATAL_ERROR "OpenCV not found! Please check MSYS2 installation.")
endif()

# 核心修改点 1：生成可执行文件（保持你的主文件名为 main.cpp）
add_executable(armor_detector main.cpp)

# 核心修改点 2：标准的现代 CMake 链接方式（target_ 开头的指令更安全）
target_include_directories(armor_detector PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(armor_detector PRIVATE ${OpenCV_LIBS})

# 安装配置 (保持原样，通常非必选，但保留无害)
install(TARGETS armor_detector
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

# 核心修改点 3：修正 Windows 下跨平台复制文件的指令（针对你的 armor.jpg）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/armor.jpg")
  add_custom_command(TARGET armor_detector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/armor.jpg"
      "${CMAKE_CURRENT_BINARY_DIR}/armor.jpg"
    COMMENT "Copying test image to build directory..."
  )
endif()