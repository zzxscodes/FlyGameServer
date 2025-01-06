#!/bin/bash

# 设置proto文件所在的目录
PROTO_DIR="./"  # 当前目录
OUT_DIR="./"    # 生成的C++代码文件放在当前目录

# 通过find命令获取所有的proto文件
PROTO_FILES=$(find "$PROTO_DIR" -name "*.proto")

# 遍历每个proto文件并编译
for PROTO_FILE in $PROTO_FILES; do
    echo "正在编译: $PROTO_FILE"

    # 使用protoc命令生成C++代码，生成文件放在当前目录
    protoc --cpp_out="$OUT_DIR" "$PROTO_FILE"
    
    if [ $? -eq 0 ]; then
        echo "$PROTO_FILE 编译成功!"
    else
        echo "$PROTO_FILE 编译失败!"
    fi
done

echo "所有proto文件编译完成!"
 