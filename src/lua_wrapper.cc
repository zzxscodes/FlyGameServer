#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "logger.h"
#include "tolua_fix.h"
#include "lua_wrapper.h"
#include "mysql_export_to_lua.h"
#include "redis_export_to_lua.h"
#include "service_export_to_lua.h"
#include "session_export_to_lua.h"
#include "scheduler_export_to_lua.h"
#include "netbus_export_to_lua.h"
#include "proto_man_export_to_lua.h"
#include "utils_export_to_lua.h"

lua_State* g_lua_State = NULL;


static void print_error(const char* file_name, 
												int line_num,
									const char* msg)
{
	logger::log(file_name, line_num, ERROR, msg);
}
static void print_warning(const char* file_name,
	int line_num,
	const char* msg)
{
	logger::log(file_name, line_num, WARNING, msg);
}
static void print_debug(const char* file_name,
	int line_num,
	const char* msg)
{
	logger::log(file_name, line_num, DEBUG, msg);
}

static void
do_log_message(void(*log)(const char* file_name, int line_num, const char* msg),
				const char* msg) 
{
	lua_Debug info;
	int depth = 0;
	while (lua_getstack(g_lua_State, depth, &info)) {

		lua_getinfo(g_lua_State, "S", &info);
		lua_getinfo(g_lua_State, "n", &info);
		lua_getinfo(g_lua_State, "l", &info);

		if (info.source[0] == '@') {
			log(&info.source[1], info.currentline, msg);
			return;
		}
		++depth;
	}
	if (depth == 0) {
		log("trunk", 0, msg);
	}
}



static int lua_log_debug(lua_State* toLua_S)
{
	int nargs = lua_gettop(toLua_S);
	std::string t;
	for (int i = 1; i <= nargs; i++)
	{
		if (lua_istable(toLua_S, i))
			t += "table";
		else if (lua_isnone(toLua_S, i))
			t += "none";
		else if (lua_isnil(toLua_S, i))
			t += "nil";
		else if (lua_isboolean(toLua_S, i))
		{
			if (lua_toboolean(toLua_S, i) != 0)
				t += "true";
			else
				t += "false";
		}
		else if (lua_isfunction(toLua_S, i))
			t += "function";
		else if (lua_islightuserdata(toLua_S, i))
			t += "lightuserdata";
		else if (lua_isthread(toLua_S, i))
			t += "thread";
		else
		{
			const char* str = lua_tostring(toLua_S, i);
			if (str)
				t += lua_tostring(toLua_S, i);
			else
				t += lua_typename(toLua_S, lua_type(toLua_S, i));
		}
		if (i != nargs)
			t += "\t";
	}
	do_log_message(print_debug, t.c_str());
	return 0;
}
static int lua_log_warning(lua_State* toLua_S)
{
	int nargs = lua_gettop(toLua_S);
	std::string t;
	for (int i = 1; i <= nargs; i++)
	{
		if (lua_istable(toLua_S, i))
			t += "table";
		else if (lua_isnone(toLua_S, i))
			t += "none";
		else if (lua_isnil(toLua_S, i))
			t += "nil";
		else if (lua_isboolean(toLua_S, i))
		{
			if (lua_toboolean(toLua_S, i) != 0)
				t += "true";
			else
				t += "false";
		}
		else if (lua_isfunction(toLua_S, i))
			t += "function";
		else if (lua_islightuserdata(toLua_S, i))
			t += "lightuserdata";
		else if (lua_isthread(toLua_S, i))
			t += "thread";
		else
		{
			const char* str = lua_tostring(toLua_S, i);
			if (str)
				t += lua_tostring(toLua_S, i);
			else
				t += lua_typename(toLua_S, lua_type(toLua_S, i));
		}
		if (i != nargs)
			t += "\t";
	}
	do_log_message(print_warning, t.c_str());
	return 0;
}
static int lua_log_error(lua_State* toLua_S)
{
	int nargs = lua_gettop(toLua_S);
	std::string t;
	for (int i = 1; i <= nargs; i++)
	{
		if (lua_istable(toLua_S, i))
			t += "table";
		else if (lua_isnone(toLua_S, i))
			t += "none";
		else if (lua_isnil(toLua_S, i))
			t += "nil";
		else if (lua_isboolean(toLua_S, i))
		{
			if (lua_toboolean(toLua_S, i) != 0)
				t += "true";
			else
				t += "false";
		}
		else if (lua_isfunction(toLua_S, i))
			t += "function";
		else if (lua_islightuserdata(toLua_S, i))
			t += "lightuserdata";
		else if (lua_isthread(toLua_S, i))
			t += "thread";
		else
		{
			const char* str = lua_tostring(toLua_S, i);
			if (str)
				t += lua_tostring(toLua_S, i);
			else
				t += lua_typename(toLua_S, lua_type(toLua_S, i));
		}
		if (i != nargs)
			t += "\t";
	}
	do_log_message(print_error, t.c_str());
	return 0;
}


static int lua_panic(lua_State* L)
{
	const char* msg = luaL_checkstring(L, -1);
	if (msg)
	{
		do_log_message(print_error, msg);
	}
	return 0;
}

static int lua_logger_init(lua_State* toLua_S)
{
	const char* path = (const char*)lua_tostring(toLua_S, 1);
	if (path==NULL)
	{
		return 0;
	}

	const char* prefix = (const char*)lua_tostring(toLua_S, 2);
	if (prefix == NULL)
	{
		return 0;
	}
	bool std_out = lua_toboolean(toLua_S, 3);
	logger::init(path, prefix, std_out);
	return 0;
}

static int register_logger_export(lua_State* toLua_S)
{
	lua_wrapper::reg_func2lua("print", lua_log_debug);

	lua_getglobal(toLua_S, "_G");
	if (lua_istable(toLua_S, -1)) {
		tolua_open(toLua_S);
		tolua_module(toLua_S, "Logger", 0);
		tolua_beginmodule(toLua_S, "Logger");

		tolua_function(toLua_S, "debug", lua_log_debug);
		tolua_function(toLua_S, "warning", lua_log_warning);
		tolua_function(toLua_S, "error", lua_log_error);
		tolua_function(toLua_S, "init", lua_logger_init);

		tolua_endmodule(toLua_S);
	}
	lua_pop(toLua_S, 1);
	return 0;
}

static int lua_add_search_path(lua_State* toLua_S)
{
	const char* path = luaL_checkstring(toLua_S, 1);
	if (path!=NULL)
	{
		std::string str_path = path;
		lua_wrapper::add_search_path(str_path);
	}
	return 0;
}

void lua_wrapper::init()
{
	g_lua_State = luaL_newstate();
	lua_atpanic(g_lua_State, lua_panic);
	luaL_openlibs(g_lua_State);
	toluafix_open(g_lua_State);

	lua_wrapper::reg_func2lua("add_search_path", lua_add_search_path);

	register_logger_export(g_lua_State);
	register_mysql_export(g_lua_State);
	register_redis_export(g_lua_State);
	register_service_export(g_lua_State);
	register_session_export(g_lua_State);
	register_scheduler_export(g_lua_State);
	register_netbus_export(g_lua_State);
	register_proto_man_export(g_lua_State);
	register_raw_cmd_export(g_lua_State);
	register_utils_export(g_lua_State);


}

void lua_wrapper::exit()
{
	if (g_lua_State != NULL)
	{
		lua_close(g_lua_State);
		g_lua_State = NULL;
	}
}

bool lua_wrapper::do_file(std::string &lua_file)
{
	if (luaL_dofile(g_lua_State, lua_file.c_str()))
	{
		lua_log_error(g_lua_State);
		return false;
	}
	return true;
}

lua_State* lua_wrapper::lua_state()
{
	return g_lua_State;
}

void lua_wrapper::reg_func2lua(const char* name, 
											int(*c_func)(lua_State* L))
{
	lua_pushcfunction(g_lua_State, c_func);
	lua_setglobal(g_lua_State, name);

}

static bool pushFunctionByHandler(int nHandler)
{
	
	toluafix_get_function_by_refid(g_lua_State, nHandler);     /* L: ... func */
	if (!lua_isfunction(g_lua_State, -1))
	{
		log_error("[LUA_ERROR] function refid '%d' does not reference a Lua function", nHandler);
		lua_pop(g_lua_State, 1);
		return false;
	}
	return true;
}

static int
executeFunction(int numArgs)
{
	int functionIndex = -(numArgs + 1);
	if (!lua_isfunction(g_lua_State, functionIndex))
	{
		log_error("value at stack [%d] is not function", functionIndex);
		lua_pop(g_lua_State, numArgs + 1);
		return 0;
	}

	int traceback = 0;
	lua_getglobal(g_lua_State, "__G__TRACKBACK__"); /* L: ... func arg1 arg2 ... G */
	if (!lua_isfunction(g_lua_State, -1))
	{
		lua_pop(g_lua_State, 1);      /* L: ... func arg1 arg2 ... */
	}
	else
	{
		lua_insert(g_lua_State, functionIndex - 1);   /* L: ... G func arg1 arg2 ... */
		traceback = functionIndex - 1;
	}

	int error = 0;
	error = lua_pcall(g_lua_State, numArgs, 1, traceback);  /* L: ... [G] ret */
	if (error)
	{
		if (traceback == 0)
		{
			log_error("[LUA ERROR] %s", lua_tostring(g_lua_State, -1));   /* L: ... error */
			lua_pop(g_lua_State, 1); 
		}
		else  /* L: ... G error */
		{
			lua_pop(g_lua_State, 2); 
		}
		return 0;
	}


	int ret = 0;
	if (lua_isnumber(g_lua_State, -1))
	{
		ret = (int)lua_tointeger(g_lua_State, -1);
	}
	else if (lua_isboolean(g_lua_State, -1))
	{
		ret = (int)lua_toboolean(g_lua_State, -1);
	}

	lua_pop(g_lua_State, 1);  /* L: ... [G] */

	if (traceback)
	{
		lua_pop(g_lua_State, 1); 
	}
	return ret;
}

int lua_wrapper::execute_script_handler(int nHandler, int numArgs)
{
	int ret = 0;

	if (pushFunctionByHandler(nHandler))   /* L: ... arg1 arg2 ... func */
	{
		if (numArgs > 0)
		{
			lua_insert(g_lua_State, -(numArgs + 1));/* L: ... func arg1 arg2 ... */
		}

		ret = executeFunction(numArgs);
	}
	lua_settop(g_lua_State, 0);
	return ret;
}

void lua_wrapper::remove_script_handler(int nHandler)
{
	toluafix_remove_function_by_refid(g_lua_State, nHandler);
}

void lua_wrapper::add_search_path(std::string &path)
{
	char strPath[1024] = { 0 };
	sprintf(strPath, "local path = string.match([[%s]],[[(.*)/[^/]*$]])\n package.path = package.path .. [[;]] .. path .. [[/?.lua;]] .. path .. [[/?/init.lua]]\n", path.c_str());
	luaL_dostring(g_lua_State, strPath);
}