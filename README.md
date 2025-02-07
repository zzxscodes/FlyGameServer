### 依赖
Linux（Ubuntu）平台下，
需要配置mysql、redis、protobuf、libuv的开发环境

### 编译构建
进入build目录内后
cmake .. 
make
编译构建后在bin目录下有可执行文件FlyGameServer

###业务逻辑开发
scripts下进行脚本开发，
示例参考与执行方式：
./FlyGameServer ../scripts/server.lua，

### 测试
测试代码在echo_client和echo_server下

测试日志输出如下：

<img width="434" alt="331d1cd2a3014f85700001fa6c7a60c" src="https://github.com/user-attachments/assets/17c5e661-8910-4af8-8cce-fc52ee888a39" /><br>

<img width="441" alt="bfc57c109c3e7ba5c9ddee455647432" src="https://github.com/user-attachments/assets/0d6a0cb0-96e5-429a-8b47-bde3a7fafc3e" /><br>

<img width="760" alt="ec70f81c0d7eb1fb5c9da3932e3de2c" src="https://github.com/user-attachments/assets/d644f479-e52e-413a-89fa-a3bc0fd7d882" /><br>

