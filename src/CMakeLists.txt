# 查找当前目录下的所有源文件
file(GLOB_RECURSE SOURCE_FILES "*.cc" "*.cpp" "*.c")

# 查找当前目录下的所有头文件
file(GLOB_RECURSE HEADER_FILES "*.h" "*.hpp")

# 添加一个可执行文件目标（你也可以根据需要改为动态库或静态库）
add_executable(FlyGameServer
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# 设置luaplus库路径
set(LIB_LUAPLUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

target_link_libraries(FlyGameServer
    ${LIB_LUAPLUS_DIR}/libluaplus.a
)



target_link_libraries(FlyGameServer uv protobuf mysqlclient hiredis pthread)

