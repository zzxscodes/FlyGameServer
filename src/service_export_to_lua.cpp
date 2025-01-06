#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lua_wrapper.h"
#include "service.h"
#include "session.h"
#include "proto_man.h"
#include "service_man.h"
#include "logger.h"
using namespace google::protobuf;

extern "C"
{

#include "tolua++.h"

}

#include "service_export_to_lua.h"

#define SERVICE_FUNCTION_MAPPING "service_function_mapping"

// 全局变量保存映射关系（实际使用中可能需要更好的初始化和管理方式）
static std::vector<ProtoMsgMapping> g_pb_msg_mappings; 

// 初始化服务函数映射表
static void init_service_function_map(lua_State* toLua_S)
{
    lua_pushstring(toLua_S, SERVICE_FUNCTION_MAPPING);
    lua_newtable(toLua_S);
    lua_rawset(toLua_S, LUA_REGISTRYINDEX);
}

static unsigned int s_function_ref_id = 0;
// 保存服务函数引用，逻辑基本不变，只是在当前代码结构下的位置调整
static unsigned int save_service_function(lua_State* L, int lo, int def)
{
    // function at lo
    if (!lua_isfunction(L, lo)) return 0;

    s_function_ref_id++;

    lua_pushstring(L, SERVICE_FUNCTION_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: fun... refid_fun */
    lua_pushinteger(L, s_function_ref_id);                      /* stack: fun... refid_fun refid */
    lua_pushvalue(L, lo);                                       /* stack: fun... refid_fun refid fun */

    lua_rawset(L, -3);                  /* refid_fun[refid] = fun, stack: fun... refid_ptr */
    lua_pop(L, 1);                                              /* stack: fun... */

    return s_function_ref_id;

    // lua_pushvalue(L, lo);                                           /* stack:... func */
    // return luaL_ref(L, LUA_REGISTRYINDEX);
}

// 获取服务函数引用，逻辑基本不变
static void get_service_function(lua_State* L, int refid)
{
    lua_pushstring(L, SERVICE_FUNCTION_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack:... refid_fun */
    lua_pushinteger(L, refid);                                  /* stack:... refid_fun refid */
    lua_rawget(L, -2);                                          /* stack:... refid_fun fun */
    lua_remove(L, -2);                                          /* stack:... fun */
}

// 推送服务函数到 Lua 栈，增加错误处理打印详细信息
static bool push_service_function(int nHandler)
{
    get_service_function(lua_wrapper::lua_state(), nHandler);     /* L:... func */
    if (!lua_isfunction(lua_wrapper::lua_state(), -1))
    {
        log_error("[LUA ERROR] refid %d does not reference any function. Stack trace:\n %s", 
                  nHandler, lua_tostring(lua_wrapper::lua_state(), -1));
        lua_pop(lua_wrapper::lua_state(), 1);
        return false;
    }
    return true;
}

// 执行函数，增加更多错误处理相关的日志输出，便于排查问题
static int exe_function(int numArgs)
{
    int functionIndex = -(numArgs + 1);
    if (!lua_isfunction(lua_wrapper::lua_state(), functionIndex))
    {
        log_error("value at stack [%d] is not a function. Stack trace:\n %s", 
                  functionIndex, lua_tostring(lua_wrapper::lua_state(), -1));
        lua_pop(lua_wrapper::lua_state(), numArgs + 1);
        return 0;
    }

    int traceback = 0;
    lua_getglobal(lua_wrapper::lua_state(), "__G__TRACKBACK__"); /* L:... func arg1 arg2... G */
    if (!lua_isfunction(lua_wrapper::lua_state(), -1))
    {
        lua_pop(lua_wrapper::lua_state(), 1);      /* L:... func arg1 arg2... */
    }
    else
    {
        lua_insert(lua_wrapper::lua_state(), functionIndex - 1);   /* L:... G func arg1 arg2... */
        traceback = functionIndex - 1;
    }

    int error = 0;
    error = lua_pcall(lua_wrapper::lua_state(), numArgs, 1, traceback);  /* L:... [G] ret */
    if (error)
    {
        if (traceback == 0)
        {
            log_error("[LUA ERROR] %s. Stack trace:\n %s", 
                      lua_tostring(lua_wrapper::lua_state(), -1), lua_tostring(lua_wrapper::lua_state(), -2));
            lua_pop(lua_wrapper::lua_state(), 1); 
        }
        else  /* L:... G error */
        {
            log_error("[LUA ERROR] %s. Stack trace:\n %s", 
                      lua_tostring(lua_wrapper::lua_state(), -2), lua_tostring(lua_wrapper::lua_state(), -3));
            lua_pop(lua_wrapper::lua_state(), 2);
        }
        return 0;
    }

    int ret = 0;
    if (lua_isnumber(lua_wrapper::lua_state(), -1))
    {
        ret = (int)lua_tointeger(lua_wrapper::lua_state(), -1);
    }
    else if (lua_isboolean(lua_wrapper::lua_state(), -1))
    {
        ret = (int)lua_toboolean(lua_wrapper::lua_state(), -1);
    }

    lua_pop(lua_wrapper::lua_state(), 1);  /* L:... [G] */

    if (traceback)
    {
        lua_pop(lua_wrapper::lua_state(), 1);      /* L:... */
    }
    return ret;
}

// 执行服务函数，逻辑基本不变
static int execute_service_function(int nHandler, int numArgs)
{
    int ret = 0;

    if (push_service_function(nHandler))   /* L:... arg1 arg2... func */
    {
        if (numArgs > 0)
        {
            lua_insert(lua_wrapper::lua_state(), -(numArgs + 1));/* L:... func arg1 arg2... */
        }

        ret = exe_function(numArgs);
    }
    lua_settop(lua_wrapper::lua_state(), 0);
    return ret;
}

class lua_service :public service
{
public:
    unsigned int lua_recv_cmd_handler;
    unsigned int lua_disconnect_handler;
    unsigned int lua_recv_raw_handler;
    unsigned int lua_connect_handler;
public:
    virtual bool on_session_recv_raw_cmd(session* s, struct raw_cmd* raw);
    virtual bool on_session_recv_cmd(session* s, struct cmd_msg* msg);
    virtual void on_session_disconnect(session* s, int stype);
    virtual void on_session_connect(session* s, int stype);
};

// 将 Protobuf 消息推送到 Lua 栈的函数，适配动态消息机制
static void push_proto_message_tolua(const Message* message)
{
    lua_State* state = lua_wrapper::lua_state();
    if (!message) {
        // printf("PushProtobuf2LuaTable failed, message is NULL");
        return;
    }
    const Reflection* reflection = message->GetReflection();

    lua_newtable(state);

    const Descriptor* descriptor = message->GetDescriptor();
    for (int32_t index = 0; index < descriptor->field_count(); ++index) {
        const FieldDescriptor* fd = descriptor->field(index);
        const std::string& name = fd->lowercase_name();

        // key
        lua_pushstring(state, name.c_str());

        bool bReapeted = fd->is_repeated();

        if (bReapeted) {
            lua_newtable(state);
            int size = reflection->FieldSize(*message, fd);
            for (int i = 0; i < size; ++i) {
                char str[32] = { 0 };
                switch (fd->cpp_type()) {
                case FieldDescriptor::CPPTYPE_DOUBLE:
                    lua_pushnumber(state, reflection->GetRepeatedDouble(*message, fd, i));
                    break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                    lua_pushnumber(state, (double)reflection->GetRepeatedFloat(*message, fd, i));
                    break;
                case FieldDescriptor::CPPTYPE_INT64:
                    sprintf(str, "%lld", (long long)reflection->GetRepeatedInt64(*message, fd, i));
                    lua_pushstring(state, str);
                    break;
                case FieldDescriptor::CPPTYPE_UINT64:
                    sprintf(str, "%llu", (unsigned long long)reflection->GetRepeatedUInt64(*message, fd, i));
                    lua_pushstring(state, str);
                    break;
                case FieldDescriptor::CPPTYPE_ENUM:
                    lua_pushinteger(state, reflection->GetRepeatedEnum(*message, fd, i)->number());
                    break;
                case FieldDescriptor::CPPTYPE_INT32:
                    lua_pushinteger(state, reflection->GetRepeatedInt32(*message, fd, i));
                    break;
                case FieldDescriptor::CPPTYPE_UINT32:
                    lua_pushinteger(state, reflection->GetRepeatedUInt32(*message, fd, i));
                    break;
                case FieldDescriptor::CPPTYPE_STRING:
                {
                    std::string value = reflection->GetRepeatedString(*message, fd, i);
                    lua_pushlstring(state, value.c_str(), value.size());
                }
                break;
                case FieldDescriptor::CPPTYPE_BOOL:
                    lua_pushboolean(state, reflection->GetRepeatedBool(*message, fd, i));
                    break;
                case FieldDescriptor::CPPTYPE_MESSAGE:
                    push_proto_message_tolua(&(reflection->GetRepeatedMessage(*message, fd, i)));
                    break;
                default:
                    break;
                }
                lua_rawseti(state, -2, i + 1); // lua's index start at 1
            }
        }
        else {
            char str[32] = { 0 };
            switch (fd->cpp_type()) {

            case FieldDescriptor::CPPTYPE_DOUBLE:
                lua_pushnumber(state, reflection->GetDouble(*message, fd));
                break;
            case FieldDescriptor::CPPTYPE_FLOAT:
                lua_pushnumber(state, (double)reflection->GetFloat(*message, fd));
                break;
            case FieldDescriptor::CPPTYPE_INT64:
                sprintf(str, "%lld", (long long)reflection->GetInt64(*message, fd));
                lua_pushstring(state, str);
                break;
            case FieldDescriptor::CPPTYPE_UINT64:
                sprintf(str, "%llu", (unsigned long long)reflection->GetUInt64(*message, fd));
                lua_pushstring(state, str);
                break;
            case FieldDescriptor::CPPTYPE_ENUM: // 按int32一样处理
                lua_pushinteger(state, (int)reflection->GetEnum(*message, fd)->number());
                break;
            case FieldDescriptor::CPPTYPE_INT32:
                lua_pushinteger(state, reflection->GetInt32(*message, fd));
                break;
            case FieldDescriptor::CPPTYPE_UINT32:
                lua_pushinteger(state, reflection->GetUInt32(*message, fd));
                break;
            case FieldDescriptor::CPPTYPE_STRING:
            {
                std::string value = reflection->GetString(*message, fd);
                lua_pushlstring(state, value.c_str(), value.size());
            }
            break;
            case FieldDescriptor::CPPTYPE_BOOL:
                lua_pushboolean(state, reflection->GetBool(*message, fd));
                break;
            case FieldDescriptor::CPPTYPE_MESSAGE:
                push_proto_message_tolua(&(reflection->GetMessage(*message, fd)));
                break;
            default:
                break;
            }
        }
        lua_rawset(state, -3);
    }
}

// 处理接收到的命令消息，适配动态消息创建机制来处理消息体部分
bool lua_service::on_session_recv_cmd(session* s, cmd_msg* msg)
{
    // call lua func
    tolua_pushuserdata(lua_wrapper::lua_state(), (void*)s);
    int index = 1;

    lua_newtable(lua_wrapper::lua_state());
    lua_pushinteger(lua_wrapper::lua_state(), msg->stype);
    lua_rawseti(lua_wrapper::lua_state(), -2, index);
    ++index;

    lua_pushinteger(lua_wrapper::lua_state(), msg->ctype);
    lua_rawseti(lua_wrapper::lua_state(), -2, index);
    ++index;

    lua_pushinteger(lua_wrapper::lua_state(), msg->utag);
    lua_rawseti(lua_wrapper::lua_state(), -2, index);
    ++index;
    if (msg->body == NULL)
    {
        lua_pushnil(lua_wrapper::lua_state());
        lua_rawseti(lua_wrapper::lua_state(), -2, index);
        ++index;
    }
    else
    {
        if (proto_man::proto_type() == PROTO_JSON)
        {
            lua_pushstring(lua_wrapper::lua_state(), (char*)msg->body);
        }
        else
        {
            // proto_buf，使用动态创建消息机制来处理
            ProtoMsgMapping* mapping = nullptr;
            for (auto& m : g_pb_msg_mappings) {
                if (m.ctype == msg->ctype) {
                    mapping = &m;
                    break;
                }
            }
            if (mapping) {
                const google::protobuf::Message* prototype =
                    google::protobuf::MessageFactory::generated_factory()->GetPrototype(mapping->descriptor);
                if (prototype) {
                    google::protobuf::Message* message = prototype->New();
                    if (message && message->ParseFromArray((const char *)msg->body, message->ByteSizeLong()))
                    {
                        push_proto_message_tolua(message);
                    }
                    delete message;
                }
            }
            else {
                log_error("Could not find mapping for ctype %d in on_session_recv_cmd", msg->ctype);
            }
        }
        lua_rawseti(lua_wrapper::lua_state(), -2, index);
        ++index;
    }
    execute_service_function(this->lua_recv_cmd_handler, 2);
    return true;
}

// 处理接收到的原始命令消息，逻辑基本不变
bool lua_service::on_session_recv_raw_cmd(session* s, raw_cmd* raw)
{
    // call lua func
    tolua_pushuserdata(lua_wrapper::lua_state(), (void*)s);
    tolua_pushuserdata(lua_wrapper::lua_state(), (void*)raw);

    execute_service_function(this->lua_recv_raw_handler, 2);
    return true;
}

// 处理会话断开连接事件，逻辑基本不变
void lua_service::on_session_disconnect(session* s, int stype)
{
    tolua_pushuserdata(lua_wrapper::lua_state(), (void*)s);
    lua_pushinteger(lua_wrapper::lua_state(), stype);
    // call lua func
    execute_service_function(this->lua_disconnect_handler, 2);
}

// 处理会话连接事件，逻辑基本不变
void lua_service::on_session_connect(session* s, int stype)
{
    tolua_pushuserdata(lua_wrapper::lua_state(), (void*)s);
    lua_pushinteger(lua_wrapper::lua_state(), stype);
    if (this->lua_connect_handler)
    {
        execute_service_function(this->lua_connect_handler, 2);
    }
}

static int lua_register_service(lua_State* toLua_S)
{
	int stype = (int)tolua_tonumber(toLua_S, 1, 0);
	bool ret = false;
	//table
	if (!lua_istable(toLua_S, 2))
	{
		lua_pushboolean(toLua_S, ret ? 1 : 0);
		return 1;
	}
	
	unsigned int lua_recv_cmd_handler;
	unsigned int lua_disconnect_handler;
	unsigned int lua_connect_handler;


	lua_getfield(toLua_S, 2, "on_session_recv_cmd");
	lua_getfield(toLua_S, 2, "on_session_disconnect");
	lua_getfield(toLua_S, 2, "on_session_connect");
	//stack 3  on_session_recv_cmd,4  on_session_disconnect
	lua_recv_cmd_handler=save_service_function(toLua_S, 3, 0);
	lua_disconnect_handler =save_service_function(toLua_S, 4, 0);
	lua_connect_handler = save_service_function(toLua_S, 5, 0);
	if (lua_recv_cmd_handler == 0 || lua_disconnect_handler == 0)
	{
		lua_pushboolean(toLua_S, ret ? 1 : 0);
		return 1;
	}

	lua_service* s = new lua_service();
	s->using_raw_cmd = false;

	s->lua_recv_cmd_handler = lua_recv_cmd_handler;
	s->lua_disconnect_handler = lua_disconnect_handler;
	s->lua_recv_raw_handler = 0;
	s->lua_connect_handler = lua_connect_handler;
	ret = service_man::register_service(stype, s);
    lua_pushboolean(toLua_S, ret ? 1 : 0);
	return 1;
}

// 注册服务（非原始命令形式），添加对消息类型映射关系的构建和传递
static int lua_register_raw_service(lua_State* toLua_S)
{
	int stype = (int)tolua_tonumber(toLua_S, 1, 0);
	bool ret = false;
	//table
	if (!lua_istable(toLua_S, 2))
	{
		lua_pushboolean(toLua_S, ret ? 1 : 0);
		return 1;
	}

	unsigned int lua_recv_raw_handler;
	unsigned int lua_disconnect_handler;
	unsigned int lua_connect_handler;

	lua_getfield(toLua_S, 2, "on_session_recv_raw_cmd");
	lua_getfield(toLua_S, 2, "on_session_disconnect");
	lua_getfield(toLua_S, 2, "on_session_connect");
	//stack 3  on_session_recv_cmd,4  on_session_disconnect
	lua_recv_raw_handler = save_service_function(toLua_S, 3, 0);
	lua_disconnect_handler = save_service_function(toLua_S, 4, 0);
	lua_connect_handler = save_service_function(toLua_S, 5, 0);
	if (lua_recv_raw_handler == 0 || lua_disconnect_handler == 0)
	{
		lua_pushboolean(toLua_S, ret ? 1 : 0);
		return 1;
	}

	lua_service* s = new lua_service();
	s->using_raw_cmd = true;
	s->lua_disconnect_handler = lua_disconnect_handler;
	s->lua_recv_cmd_handler = 0;
	s->lua_recv_raw_handler = lua_recv_raw_handler;
	s->lua_connect_handler = lua_connect_handler;
	ret = service_man::register_service(stype, s);
    lua_pushboolean(toLua_S, ret ? 1 : 0);
	return 1;
}

int register_service_export(lua_State* toLua_S)
{
	init_service_function_map(toLua_S);
	lua_getglobal(toLua_S, "_G");
	if (lua_istable(toLua_S, -1)) {
		tolua_open(toLua_S);
		tolua_module(toLua_S, "Service", 0);
		tolua_beginmodule(toLua_S, "Service");

		tolua_function(toLua_S, "register", lua_register_service);
		tolua_function(toLua_S, "register_with_raw", lua_register_raw_service);

		tolua_endmodule(toLua_S);
	}
	lua_pop(toLua_S, 1);

	return 0;
}
