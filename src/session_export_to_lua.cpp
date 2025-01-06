#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "service.h"
#include "session.h"
#include "proto_man.h"
#include "service_man.h"
#include "netbus.h"
#include "logger.h"
#include "lua_wrapper.h"
using namespace google::protobuf;

extern "C"
{
#include "tolua++.h"
}
#include "session_export_to_lua.h"

// 全局变量保存映射关系（实际使用中可能需要更好的初始化和管理方式）
static std::vector<ProtoMsgMapping> g_pb_msg_mappings; 

static int lua_session_close(lua_State* toLua_S)
{
    session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
    if (s == NULL)
    {
        return 0;
    }
    s->close();
    return 0;
}

// 根据 Lua 表和消息类型标识动态创建 Protobuf 消息
static google::protobuf::Message* lua_table_to_protobuf(lua_State* toLua_S,
                                                        int stack_index,
                                                        int ctype)
{
    if (!lua_istable(toLua_S, stack_index))
    {
        // log_error("lua_table_to_protobuf")
        return NULL;
    }
    const ProtoMsgMapping* mapping = nullptr;
    for (const auto& m : g_pb_msg_mappings) {
        if (m.ctype == ctype) {
            mapping = &m;
            break;
        }
    }
    if (!mapping) {
        log_error("Could not find mapping for ctype %d in lua_table_to_protobuf", ctype);
        return NULL;
    }
    const google::protobuf::Message* prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(mapping->descriptor);
    if (!prototype) {
        log_error("Can't find prototype for message with ctype %d", ctype);
        return NULL;
    }
    Message* message = prototype->New();
    if (!message) {
        log_error("Failed to create new message for ctype %d", ctype);
        return NULL;
    }

    const Reflection* reflection = message->GetReflection();
    const Descriptor* descriptor = message->GetDescriptor();

    for (int32_t index = 0; index < descriptor->field_count(); ++index) {
        const FieldDescriptor* fd = descriptor->field(index);
        const std::string name = fd->name();

        bool isRequired = fd->is_required();
        bool bReapeted = fd->is_repeated();
        lua_pushstring(toLua_S, name.c_str());
        lua_rawget(toLua_S, stack_index);

        bool isNil = lua_isnil(toLua_S, -1);

        if (bReapeted) {
            if (isNil) {
                lua_pop(toLua_S, 1);
                continue;
            }
            else {
                bool isTable = lua_istable(toLua_S, -1);
                if (!isTable) {
                    log_error("cant find required repeated field %s\n", name.c_str());
                    proto_man::release_message(message);
                    return NULL;
                }
            }
            lua_pushnil(toLua_S);
            for (; lua_next(toLua_S, -2)!= 0;) {
                switch (fd->cpp_type()) {

                case FieldDescriptor::CPPTYPE_DOUBLE:
                {
                    double value = luaL_checknumber(toLua_S, -1);
                    reflection->AddDouble(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                {
                    float value = luaL_checknumber(toLua_S, -1);
                    reflection->AddFloat(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_INT64:
                {
                    int64_t value = luaL_checknumber(toLua_S, -1);
                    reflection->AddInt64(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_UINT64:
                {
                    uint64_t value = luaL_checknumber(toLua_S, -1);
                    reflection->AddUInt64(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_ENUM:
                {
                    int32_t value = luaL_checknumber(toLua_S, -1);
                    const EnumDescriptor* enumDescriptor = fd->enum_type();
                    const EnumValueDescriptor* valueDescriptor = enumDescriptor->FindValueByNumber(value);
                    reflection->AddEnum(message, fd, valueDescriptor);
                }
                break;
                case FieldDescriptor::CPPTYPE_INT32:
                {
                    int32_t value = luaL_checknumber(toLua_S, -1);
                    reflection->AddInt32(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_UINT32:
                {
                    uint32_t value = luaL_checknumber(toLua_S, -1);
                    reflection->AddUInt32(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_STRING:
                {
                    size_t size = 0;
                    const char* value = luaL_checklstring(toLua_S, -1, &size);
                    reflection->AddString(message, fd, std::string(value, size));
                }
                break;
                case FieldDescriptor::CPPTYPE_BOOL:
                {
                    bool value = lua_toboolean(toLua_S, -1);
                    reflection->AddBool(message, fd, value);
                }
                break;
                case FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    int sub_ctype = fd->message_type()->index();  // 获取子消息的类型标识（示例，可能需根据实际调整获取方式）
                    Message* value = lua_table_to_protobuf(toLua_S, lua_gettop(toLua_S), sub_ctype);
                    if (!value) {
                        log_error("convert to message %s failed whith value %s\n", fd->message_type()->name().c_str(), name.c_str());
                        proto_man::release_message(value);
                        return NULL;
                    }
                    Message* msg = reflection->AddMessage(message, fd);
                    msg->CopyFrom(*value);
                    proto_man::release_message(value);
                }
                break;
                default:
                    break;
                }

                lua_pop(toLua_S, 1);
            }
        }
        else {

            if (isRequired) {
                if (isNil) {
                    log_error("cant find required field %s\n", name.c_str());
                    proto_man::release_message(message);
                    return NULL;
                }
            }
            else {
                if (isNil) {
                    lua_pop(toLua_S, 1);
                    continue;
                }
            }

            switch (fd->cpp_type()) {
            case FieldDescriptor::CPPTYPE_DOUBLE:
            {
                double value = luaL_checknumber(toLua_S, -1);
                reflection->SetDouble(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_FLOAT:
            {
                float value = luaL_checknumber(toLua_S, -1);
                reflection->SetFloat(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_INT64:
            {
                int64_t value = luaL_checknumber(toLua_S, -1);
                reflection->SetInt64(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_UINT64:
            {
                uint64_t value = luaL_checknumber(toLua_S, -1);
                reflection->SetUInt64(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_ENUM:
            {
                int32_t value = luaL_checknumber(toLua_S, -1);
                const EnumDescriptor* enumDescriptor = fd->enum_type();
                const EnumValueDescriptor* valueDescriptor = enumDescriptor->FindValueByNumber(value);
                reflection->SetEnum(message, fd, valueDescriptor);
            }
            break;
            case FieldDescriptor::CPPTYPE_INT32:
            {
                int32_t value = luaL_checknumber(toLua_S, -1);
                reflection->SetInt32(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_UINT32:
            {
                uint32_t value = luaL_checknumber(toLua_S, -1);
                reflection->SetUInt32(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_STRING:
            {
                size_t size = 0;
                const char* value = luaL_checklstring(toLua_S, -1, &size);
                reflection->SetString(message, fd, std::string(value, size));
            }
            break;
            case FieldDescriptor::CPPTYPE_BOOL:
            {
                bool value = lua_toboolean(toLua_S, -1);
                reflection->SetBool(message, fd, value);
            }
            break;
            case FieldDescriptor::CPPTYPE_MESSAGE:
            {
                int sub_ctype = fd->message_type()->index();  // 获取子消息的类型标识（示例，可能需根据实际调整获取方式）
                Message* value = lua_table_to_protobuf(toLua_S, lua_gettop(toLua_S), sub_ctype);
                if (!value) {
                    log_error("convert to message %s failed whith value %s \n", fd->message_type()->name().c_str(), name.c_str());
                    proto_man::release_message(message);
                    return NULL;
                }
                Message* msg = reflection->MutableMessage(message, fd);
                msg->CopyFrom(*value);
                proto_man::release_message(value);
            }
            break;
            default:
                break;
            }
        }

        // pop value
        lua_pop(toLua_S, 1);
    }

    return message;
}

// 通过 UDP 发送消息，逻辑基本不变，依赖于其他相关函数的正确实现
void udp_send_msg(char* ip, int port, cmd_msg* msg)
{
    unsigned char* encode_pkg = NULL;
    int encode_len = 0;
    encode_pkg = proto_man::encode_msg_to_raw(msg, &encode_len);
    if (encode_pkg)
    {
        // this->send_data(encode_pkg, encode_len);
        netbus::instance()->udp_send_to(ip, port, encode_pkg, encode_len);
        proto_man::msg_raw_free(encode_pkg);
    }
}

// 通过 UDP 发送消息，适配动态消息创建逻辑
// ip,port,{1,stype 2,ctype 3,utag 4,body(jsonStr or table)}
static int lua_udp_send_msg(lua_State* toLua_S)
{
    char* ip = (char*)tolua_tostring(toLua_S, 1, NULL);
    if (ip == NULL) {
        return 0;
    }

    int port = (int)tolua_tonumber(toLua_S, 2, 0);
    if (port == 0)
    {
        return 0;
    }
    // stack  table
    if (!lua_istable(toLua_S, 3)) {
        return 0;
    }

    struct cmd_msg msg;

    int n = luaL_len(toLua_S, 3);
    if (n!= 4 && n!= 3) {
        return 0;
    }

    lua_pushnumber(toLua_S, 1);
    lua_gettable(toLua_S, 3);
    msg.stype = luaL_checkinteger(toLua_S, -1);

    lua_pushnumber(toLua_S, 2);
    lua_gettable(toLua_S, 3);
    msg.ctype = luaL_checkinteger(toLua_S, -1);

    lua_pushnumber(toLua_S, 3);
    lua_gettable(toLua_S, 3);
    msg.utag = luaL_checkinteger(toLua_S, -1);

    if (n == 3)
    {
        msg.body = NULL;
        // s->send_msg(&msg);
        udp_send_msg(ip, port, &msg);
        return 0;
    }
    lua_pushnumber(toLua_S, 4);
    lua_gettable(toLua_S, 3);

    if (proto_man::proto_type() == PROTO_JSON)
    {
        msg.body = (char*)lua_tostring(toLua_S, -1);
        // s->send_msg(&msg);
        udp_send_msg(ip, port, &msg);
    }
    else
    {
        if (!lua_istable(toLua_S, -1))
        {
            msg.body = NULL;
            // s->send_msg(NULL);
            udp_send_msg(ip, port, &msg);
        }
        else
        {
            // protobuf message table
            msg.body = lua_table_to_protobuf(toLua_S, lua_gettop(toLua_S), msg.ctype);
            // s->send_msg(&msg);
            udp_send_msg(ip, port, &msg);
            proto_man::release_message((google::protobuf::Message*)msg.body);
        }
    }

    return 0;
}

// 发送消息，适配动态消息创建逻辑
// {1,stype 2,ctype 3,utag 4,body(jsonStr or table)}
static int lua_send_msg(lua_State* toLua_S)
{
    session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
    if (s == NULL)
    {
        return 0;
    }
    // stack  1,session 2,table
    if (!lua_istable(toLua_S, 2))
    {
        return 0;
    }

    cmd_msg msg;
    int n = luaL_len(toLua_S, 2);
    if (n!= 4 && n!= 3)
    {
        return 0;
    }
    lua_pushnumber(toLua_S, 1);
    lua_gettable(toLua_S, 2);
    msg.stype = luaL_checkinteger(toLua_S, -1);

    lua_pushnumber(toLua_S, 2);
    lua_gettable(toLua_S, 2);
    msg.ctype = luaL_checkinteger(toLua_S, -1);

    lua_pushnumber(toLua_S, 3);
    lua_gettable(toLua_S, 2);
    msg.utag = luaL_checkinteger(toLua_S, -1);

    if (n == 3)
    {
        msg.body = NULL;
        s->send_msg(&msg);
        return 0;
    }
    lua_pushnumber(toLua_S, 4);
    lua_gettable(toLua_S, 2);

    if (proto_man::proto_type() == PROTO_JSON)
    {
        msg.body = (char*)lua_tostring(toLua_S, -1);
        s->send_msg(&msg);
    }
    else
    {
        if (!lua_istable(toLua_S, -1))
        {
            msg.body = NULL;
            s->send_msg(NULL);
        }
        else
        {
            // protobuf message table
            msg.body = lua_table_to_protobuf(toLua_S, lua_gettop(toLua_S), msg.ctype);
            s->send_msg(&msg);
            proto_man::release_message((google::protobuf::Message*)msg.body);
        }
    }

    return 0;
}

static int lua_send_raw_cmd(lua_State* toLua_S)
{
    session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);  // 栈中获取值
    if (s == NULL)
    {
        return 0;
    }

    raw_cmd* cmd = (raw_cmd*)tolua_touserdata(toLua_S, 2, NULL);
    if (cmd == NULL)
    {
        return 0;
    }
    s->send_raw_cmd(cmd);
    return 0;
}

static int lua_get_addr(lua_State* toLua_S)
{
    session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
    if (s == NULL)
    {
        return 0;
    }
    int client_port = 0;
    const char* ip = s->get_address(&client_port);
    lua_pushstring(toLua_S, ip);
    lua_pushinteger(toLua_S, client_port);
    return 2;
}

static int lua_set_utag(lua_State* toLua_S)
{
	session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
	if (s == NULL)
	{
		return 0;
	}
	
	unsigned int utag = lua_tointeger(toLua_S, 2);
	s->utag = utag;

	return 2;
	
}

static int lua_get_utag(lua_State* toLua_S)
{
	session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
	if (s == NULL)
	{
		return 0;
	}
	lua_pushinteger(toLua_S, s->utag);
	return 1;
	
}

static int lua_set_uid(lua_State* toLua_S)
{
	session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
	if (s == NULL)
	{
		return 0;
	}

	unsigned int uid = lua_tointeger(toLua_S, 2);
	s->uid = uid;

	return 2;
	
}

static int lua_get_uid(lua_State* toLua_S)
{
	session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
	if (s == NULL)
	{
		return 0;
	}
	lua_pushinteger(toLua_S, s->uid);
	return 1;
	
}

static int lua_as_client(lua_State* toLua_S)
{
	session* s = (session*)tolua_touserdata(toLua_S, 1, NULL);
	if (s == NULL)
	{
		return 0;
	}
	lua_pushinteger(toLua_S, s->as_client);
	return 1;
	
}

int register_session_export(lua_State* toLua_S)
{
	lua_getglobal(toLua_S, "_G");
	if (lua_istable(toLua_S, -1)) {
		tolua_open(toLua_S);
		tolua_module(toLua_S, "Session", 0);
		tolua_beginmodule(toLua_S, "Session");

		tolua_function(toLua_S, "close", lua_session_close);
		tolua_function(toLua_S, "send_msg", lua_send_msg);
		tolua_function(toLua_S, "send_raw_cmd", lua_send_raw_cmd);
		tolua_function(toLua_S, "get_address", lua_get_addr);
		tolua_function(toLua_S, "set_utag", lua_set_utag);
		tolua_function(toLua_S, "get_utag", lua_get_utag);
		tolua_function(toLua_S, "set_uid", lua_set_uid);
		tolua_function(toLua_S, "get_uid", lua_get_uid);
		tolua_function(toLua_S, "asclient", lua_as_client);
		tolua_function(toLua_S, "udp_send_msg", lua_udp_send_msg);

		tolua_endmodule(toLua_S);
	}
	lua_pop(toLua_S, 1);

	return 0;
}
