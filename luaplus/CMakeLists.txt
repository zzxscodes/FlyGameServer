# CMake 最低版本要求
cmake_minimum_required(VERSION 3.0)

# 项目名称
project(luaplus)

# 查找当前目录下的所有源文件
file(GLOB_RECURSE SOURCE_FILES "*.cc" "*.cpp" "*.c")

# 创建静态库目标
add_library(luaplus STATIC ${SOURCE_FILES})

# 设置头文件路径（假设头文件在luaplus目录下）
target_include_directories(luaplus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
