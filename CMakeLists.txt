cmake_minimum_required(VERSION 3.10)
project(FlyGameServer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 配置最终的可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/luaplus)

link_directories(/usr/lib)
link_directories(/usr/local/lib)

# 添加子目录
add_subdirectory(luaplus)
add_subdirectory(src)
