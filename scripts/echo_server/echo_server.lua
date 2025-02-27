function echo_recv_cmd( s,msg )
    print(msg[1])--stype
    print(msg[2])--ctype
    print(msg[3])--utag
    local body=msg[4];
    print(body.name);
    print(body.email);
    print(body.age);

    --send to client
    local to_client={1,2,0,{status=200}}
    Session.send_msg(s,to_client)

end

function echo_session_connect( s,stype )
    local ip,port=Session.get_address(s);
    print("echo_session_connect"..ip..":"..port);
end

function echo_session_disconnect( s,stype )
    local ip,port=Session.get_address(s);
    print("echo_session_disconnect"..ip..":"..port);
end

local echo_service={
    on_session_recv_cmd=echo_recv_cmd,
    on_session_connect=echo_session_connect,
    on_session_disconnect=echo_session_disconnect,
};

local echo_server={
    stype=1,
    service=echo_service,
}

return echo_server;