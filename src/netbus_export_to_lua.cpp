#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "netbus.h"
#include "lua_wrapper.h"
#include <cstdint>
#include "uv.h"

extern "C"
{

#include "tolua++.h"

}

#include "tolua_fix.h"
#include "netbus_export_to_lua.h"

#define my_malloc malloc
#define my_free free


static int lua_udp_listen(lua_State* toLua_S)
{
	int argc = lua_gettop(toLua_S);
	if (argc!=1)
	{
		return 0;
	}
	int port = lua_tointeger(toLua_S, 1);
	netbus::instance()->udp_listen(port);
	return 0;
}

static void on_tcp_listen(session* s, void* udata)
{
	if (udata == NULL)
		return;

	tolua_pushuserdata(lua_wrapper::lua_state(), s);
	lua_wrapper::execute_script_handler((intptr_t)udata, 1);
	//lua_wrapper::remove_script_handler((intptr_t)udata);
}

static int lua_tcp_listen(lua_State* toLua_S)
{
	int cout = lua_gettop(toLua_S);
	int port = luaL_checkinteger(toLua_S, 1);
	if(cout == 1)
	{
		netbus::instance()->tcp_listen(port);
	}
	int handler = toluafix_ref_function(toLua_S, 2, 0);
	if (handler==0)
	{
		return 0;
	}
	netbus::instance()->tcp_listen(port,on_tcp_listen,(void*)handler);
	return 0;
}

static void on_tcp_connected(int err, session* s, void* udata)
{
	if (err)
	{
		lua_pushinteger(lua_wrapper::lua_state(), err);
		lua_pushnil(lua_wrapper::lua_state());
	}
	else
	{
		lua_pushinteger(lua_wrapper::lua_state(), err);
		tolua_pushuserdata(lua_wrapper::lua_state(), s);
	}
	lua_wrapper::execute_script_handler((intptr_t)udata, 2);
	lua_wrapper::remove_script_handler((intptr_t)udata);
}

//ip,port,lua_func(error,session)
static int lua_tcp_connect(lua_State* toLua_S)
{
	const char* ip = luaL_checkstring(toLua_S, 1);
	if (ip==NULL)
	{
		return 0;
	}
	int port = luaL_checkinteger(toLua_S, 2);
	int handler = toluafix_ref_function(toLua_S, 3, 0);
	if (handler==0)
	{
		return 0;
	}
	netbus::instance()->tcp_connect(ip, port, on_tcp_connected, (void*)handler);
	return 0;
}

int register_netbus_export(lua_State* toLua_S)
{
	lua_getglobal(toLua_S, "_G");
	if (lua_istable(toLua_S, -1)) {
		tolua_open(toLua_S);
		tolua_module(toLua_S, "Netbus", 0);
		tolua_beginmodule(toLua_S, "Netbus");

		tolua_function(toLua_S, "udp_listen", lua_udp_listen);
		tolua_function(toLua_S, "tcp_listen", lua_tcp_listen);
		tolua_function(toLua_S, "tcp_connect", lua_tcp_connect);

		tolua_endmodule(toLua_S);
	}
	lua_pop(toLua_S, 1);
	return 0;
}
