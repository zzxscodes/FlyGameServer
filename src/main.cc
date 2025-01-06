#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include "proto_man.h"
#include "netbus.h"
#include "logger.h"
#include "timer_list.h"
#include "timestamp.h"
#include "mysql_wrapper.h"
#include "redis_wrapper.h"
#include "lua_wrapper.h"
using namespace std;

int main(int argc,char**  argv)
{
	netbus::instance()->init();
	lua_wrapper::init();

	if (argc!=3)
	{
		std::string search_path = "../scripts/";
		lua_wrapper::add_search_path(search_path);
		std::string lua_file = search_path + "logic_server/main.lua";
		lua_wrapper::do_file(lua_file);
		//end
	}
	else
	{
		std::string search_path = argv[1];
		if (*(search_path.end()-1)!='/')
		{
			search_path += '/';
		}
		lua_wrapper::add_search_path(search_path);
		std::string lua_file = search_path+argv[2];
		lua_wrapper::do_file(lua_file);
	}

	netbus::instance()->run();
	lua_wrapper::exit();

	pause();
	return 0;
}