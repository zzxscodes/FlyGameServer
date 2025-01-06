--初始化日志模块
Logger.init("logger/echo_server/","echo_server",true);
--end

--初始化协议模块
local proto_type={
  PROTO_JSON=0,
  PROTO_BUF=1,
}
ProtoMan.init(proto_type.PROTO_BUF);
--如果是protobuf协议，还要注册映射表
if ProtoMan.proto_type() == proto_type.PROTO_BUF then
  local cmd_name_map=require("cmd_name_map");
  if cmd_name_map then
    ProtoMan.register_protobuf_cmd_map(cmd_name_map);
  end
end

--end

--开启网络服务
Netbus.tcp_listen(6080);
Netbus.udp_listen(8002);
--end

print("start service success!!!");

local echo_server=require("echo_server/echo_server");
local ret= Service.register(echo_server.stype,echo_server.service);

if ret then
  print("register echo_server service success!!!")
else
  print("register echo_server service failed!!!")
end 
