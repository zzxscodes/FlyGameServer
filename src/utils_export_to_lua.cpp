#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "timestamp.h"
#include "timer_list.h"
#include "lua_wrapper.h"
extern "C"
{
#include "tolua++.h"
}
#include "tolua_fix.h"
#include "utils_export_to_lua.h"

#include "small_alloc.h"
#define my_malloc small_alloc
#define my_free small_free


static int lua_timestamp(lua_State* toLua_S)
{
	unsigned long ts = timestamp();
	lua_pushinteger(toLua_S, ts);

	return 1;
}

static int lua_timestamp_today(lua_State* toLua_S)
{
	unsigned long ts = timestamp_today();
	lua_pushinteger(toLua_S, ts);

	return 1;
}

static int lua_timestamp_yesterday(lua_State* toLua_S)
{
	unsigned long ts = timestamp_yesterday();
	lua_pushinteger(toLua_S, ts);

	return 1;
}


int register_utils_export(lua_State* toLua_S)
{
	lua_getglobal(toLua_S, "_G");
	if (lua_istable(toLua_S, -1)) {
		tolua_open(toLua_S);
		tolua_module(toLua_S, "Utils", 0);
		tolua_beginmodule(toLua_S, "Utils");

		tolua_function(toLua_S, "timestamp", lua_timestamp);
		tolua_function(toLua_S, "timestamp_today", lua_timestamp_today);
		tolua_function(toLua_S, "timestamp_yesterday", lua_timestamp_yesterday);

		tolua_endmodule(toLua_S);
	}
	lua_pop(toLua_S, 1);

	return 0;
}
