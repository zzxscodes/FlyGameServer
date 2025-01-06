#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstdint>
#include "lua_wrapper.h"
#include "redis_wrapper.h"

extern "C" {
#include "tolua++.h"
}
#include "tolua_fix.h"
#include "redis_export_to_lua.h"

static void on_open_cb(const char* err, void* context, void* udata)
{
    if(err)
    {
        lua_pushstring(lua_wrapper::lua_state(), err);
        lua_pushnil(lua_wrapper::lua_state());
    }
    else
    {
        lua_pushnil(lua_wrapper::lua_state());
        tolua_pushuserdata(lua_wrapper::lua_state(), context);
    }

    lua_wrapper::execute_script_handler((intptr_t)udata, 2);
    lua_wrapper::remove_script_handler((intptr_t)udata);
}

static int lua_redis_connect(lua_State* tolua_S)
{
    char* ip = (char*)tolua_tostring(tolua_S, 1, 0);
    if(!ip)
    {
        return 0;
    }

    int port = (int)tolua_tonumber(tolua_S, 2, 0);
    int handler = toluafix_ref_function(tolua_S, 3, 0);
    redis_wrapper::connect(ip, port, on_open_cb, (void*)handler);

    return 0;
}

static int lua_redis_close(lua_State* tolua_S)
{
    void* context = tolua_touserdata(tolua_S, 1, 0);
    if(context)
    {
        redis_wrapper::close_redis(context);
    }
    return 0;
}

static void push_result_to_lua(redisReply* result)
{
    int index = 1;
    switch (result->type)
    {
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_STATUS:
        lua_pushstring(lua_wrapper::lua_state(), result->str);
        break;
    case REDIS_REPLY_INTEGER:
        lua_pushinteger(lua_wrapper::lua_state(), result->integer);
        break;
    case REDIS_REPLY_NIL:
        lua_pushnil(lua_wrapper::lua_state());
        break;
    case REDIS_REPLY_ARRAY:
        lua_newtable(lua_wrapper::lua_state());
        for(int i = 0; i < result->elements; i++)
        {
            push_result_to_lua(result->element[i]);
            lua_rawseti(lua_wrapper::lua_state(), -2, index);
            ++index;
        }
        break;
    default:
        break;
    }
}

static void on_lua_query_cb(const char* err, redisReply* result, void* udata)
{
    if(err)
    {
        lua_pushstring(lua_wrapper::lua_state(), err);
        lua_pushnil(lua_wrapper::lua_state());
    }
    else
    {
        lua_pushnil(lua_wrapper::lua_state());
        if(result)
        {
            push_result_to_lua(result);
        }
        else
        {
            lua_pushnil(lua_wrapper::lua_state());
        }
    }

    lua_wrapper::execute_script_handler((intptr_t)udata, 2);
    lua_wrapper::remove_script_handler((intptr_t)udata);
}

static int lua_redis_query(lua_State* tolua_S)
{
    void* context = tolua_touserdata(tolua_S, 1, 0);
    if(!context)
    {
        return 0;
    }
    char* cmd = (char*)tolua_tostring(tolua_S, 2, 0);
    if(!cmd)
    {
        return 0;
    }
    int handler = toluafix_ref_function(tolua_S, 3, 0);
    if(handler == 0)
    {
        return 0;
    }
    redis_wrapper::query(context, cmd, on_lua_query_cb, (void*)handler);

    return 0;
}

int register_redis_export(lua_State* tolua_S)
{
    lua_getglobal(tolua_S, "_G");
    if(lua_istable(tolua_S, -1))
    {
        tolua_open(tolua_S);
        tolua_module(tolua_S, "Redis", 0);
        tolua_beginmodule(tolua_S, "Redis");
        tolua_function(tolua_S, "connect", lua_redis_connect);
        tolua_function(tolua_S, "close_redis", lua_redis_close);
        tolua_function(tolua_S, "query", lua_redis_query);
        tolua_endmodule(tolua_S);
    }
    lua_pop(tolua_S, 1);
    return 0;
}