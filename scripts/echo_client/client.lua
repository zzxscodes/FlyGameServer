Logger.init("ClientLogs","client",true);

--初始化协议模块
local proto_type={
    PROTO_JSON=0,
    PROTO_BUF=1,
}
ProtoMan.init(proto_type.PROTO_BUF);

--如果是protobuf协议，还要注册映射表
local cmd_name_map = {
    "USERSEND",
    "USERRECV"
}

if ProtoMan.proto_type() == proto_type.PROTO_BUF then
    if cmd_name_map then
        ProtoMan.register_protobuf_cmd_map(cmd_name_map);
    end
end

Netbus.tcp_connect("127.0.0.1",6080,
	--链接成功的回调
    function(error,session)
        if error ~=0 then
            Logger.error("connect error to [127.0.0.1:6080]")
            return;
        end
            print("success to connect to [127.0.0.1:6080]");
    end);


