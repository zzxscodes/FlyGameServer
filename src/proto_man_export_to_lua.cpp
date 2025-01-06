#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "proto_man.h"
#include "lua_wrapper.h"


extern "C"
{

#include "tolua++.h"

}

#include "tolua_fix.h"
#include "proto_man_export_to_lua.h"
using namespace google::protobuf;

#define my_malloc malloc
#define my_free free

// 假设这里有结构体用于保存消息类型与描述符的映射关系，和前面proto_man修改部分对应

// 全局变量保存映射关系（实际使用中可能需要更好的初始化和管理方式）
static std::vector<ProtoMsgMapping> g_pb_msg_mappings; 

// 从Lua表中读取并构建消息类型映射关系
static int lua_register_protobuf_cmd_map(lua_State* toLua_S)
{
    std::vector<ProtoMsgMapping> cmd_mappings;
    int n = luaL_len(toLua_S, 1);
    if (n <= 0)
    {
        return 0;
    }
    for (int i = 1; i <= n; i++)
    {
        lua_pushnumber(toLua_S, i);
        lua_gettable(toLua_S, 1);
        const char* name = luaL_checkstring(toLua_S, -1);
        if (name!= NULL)
        {
            const google::protobuf::Descriptor* descriptor =
                google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(name);
            if (descriptor)
            {
                ProtoMsgMapping mapping;
                mapping.ctype = i;
                mapping.descriptor = descriptor;
                cmd_mappings.push_back(mapping);
            }
        }
        lua_pop(toLua_S, 1);
    }
    proto_man::register_protobuf_cmd_map(cmd_mappings);
    g_pb_msg_mappings = cmd_mappings;  // 保存全局映射关系
    return 0;
}

static int lua_proto_type(lua_State* toLua_S)
{
    lua_pushinteger(toLua_S, proto_man::proto_type());
    return 1;
}

static int lua_proto_man_init(lua_State* toLua_S)
{
    int argc = lua_gettop(toLua_S);
    if (argc!= 1)
    {
        return 0;
    }
    int proto_type = lua_tointeger(toLua_S, 1);
    if (proto_type!= PROTO_JSON && proto_type!= PROTO_BUF)
    {
        return 0;
    }
    proto_man::init(proto_type, g_pb_msg_mappings);  // 传入映射关系初始化proto_man
    return 0;
}

int register_proto_man_export(lua_State* toLua_S)
{
    lua_getglobal(toLua_S, "_G");
    if (lua_istable(toLua_S, -1)) {
        tolua_open(toLua_S);
        tolua_module(toLua_S, "ProtoMan", 0);
        tolua_beginmodule(toLua_S, "ProtoMan");

        tolua_function(toLua_S, "init", lua_proto_man_init);
        tolua_function(toLua_S, "proto_type", lua_proto_type);
        tolua_function(toLua_S, "register_protobuf_cmd_map", lua_register_protobuf_cmd_map);

        tolua_endmodule(toLua_S);
    }
    lua_pop(toLua_S, 1);

    return 0;
}

// RawCmd.read_header(raw_cmd)
static int lua_raw_read_header(lua_State* toLua_S)
{
    int argc = lua_gettop(toLua_S);
    if (argc!= 1)
    {
        return 0;
    }

    raw_cmd* raw = (raw_cmd*)tolua_touserdata(toLua_S, 1, NULL);
    if (raw == NULL)
    {
        return 0;
    }
    lua_pushinteger(toLua_S, raw->stype);
    lua_pushinteger(toLua_S, raw->ctype);
    lua_pushinteger(toLua_S, raw->utag);
    return 3;

}

// 辅助函数，将Protobuf消息推送到Lua栈（示例，可能需要根据实际完善处理不同类型字段等情况）
void push_proto_message_tolua(const Message* message)
{
    // 这里简单示例，如果消息有更多复杂结构，需要更深入的处理
    // 比如遍历消息的字段等，此处假设消息转字符串表示推送到Lua
    std::string message_str;
    message->SerializeToString(&message_str);
    lua_pushstring(lua_wrapper::lua_state(), message_str.c_str());
}

static int lua_raw_read_body(lua_State* toLua_S)
{
    int argc = lua_gettop(toLua_S);
    if (argc!= 1)
    {
        return 0;
    }
    raw_cmd* raw = (raw_cmd*)tolua_touserdata(toLua_S, 1, NULL);
    if (raw == NULL)
    {
        return 0;
    }
    cmd_msg* msg;
    if (proto_man::decode_cmd_msg(raw->raw_data, raw->raw_len, &msg))
    {
        if (msg->body == NULL)
        {
            lua_pushnil(toLua_S);
        }
        else if (proto_man::proto_type() == PROTO_JSON)
        {
            lua_pushstring(toLua_S, (const char*)msg->body);
        }
        else
        {
            push_proto_message_tolua((Message*)msg->body);
        }
        proto_man::cmd_msg_free(msg);
    }
    return 1;

}

static int lua_raw_set_utag(lua_State* toLua_S)
{
    int argc = lua_gettop(toLua_S);
    if (argc!= 2)
    {
        return 0;
    }

    raw_cmd* raw = (raw_cmd*)tolua_touserdata(toLua_S, 1, NULL);
    if (raw == NULL)
    {
        return 0;
    }
    unsigned int utag = (unsigned int)luaL_checkinteger(toLua_S, 2);
    raw->utag = utag;

    unsigned char* utag_ptr = raw->raw_data + 4;
    utag_ptr[0] = (utag & 0xff);
    utag_ptr[1] = ((utag & 0xff00) >> 8);
    utag_ptr[2] = ((utag & 0xff0000) >> 16);
    utag_ptr[3] = ((utag & 0xff000000) >> 24);
    return 0;

}

int register_raw_cmd_export(lua_State* toLua_S)
{
    lua_getglobal(toLua_S, "_G");
    if (lua_istable(toLua_S, -1)) {
        tolua_open(toLua_S);
        tolua_module(toLua_S, "RawCmd", 0);
        tolua_beginmodule(toLua_S, "RawCmd");

        tolua_function(toLua_S, "read_header", lua_raw_read_header);
        tolua_function(toLua_S, "set_utag", lua_raw_set_utag);
        tolua_function(toLua_S, "read_body", lua_raw_read_body);
        tolua_endmodule(toLua_S);
    }
    lua_pop(toLua_S, 1);

    return 0;
}