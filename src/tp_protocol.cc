#include "tp_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include "cache_alloc.h"
#include "session.h"
using namespace std;

extern cache_allocer* wbuf_allocer;

bool tp_protocol::read_header(unsigned char *data, int data_len, int *pkg_size, int *out_header_size)
{
    if(data_len < 2)
    {
        return false;
    }
    *pkg_size = (data[0] | data[1] << 8);
    *out_header_size = 2;
    return true;
}

unsigned char* tp_protocol::package(unsigned char *raw_data, int len, int *pkg_len)
{
    int head_size = 2;

    *pkg_len = (head_size + len);
    unsigned char* data_buf =(unsigned char*)cache_alloc(wbuf_allocer,(*pkg_len));
    data_buf[0] = (unsigned char)(*pkg_len) & 0x000000ff;
    data_buf[1] = (unsigned char)((*pkg_len) & 0x0000ff00) >> 8;

    memcpy(data_buf + head_size, raw_data, len);
    
    return data_buf;
}

void tp_protocol::release_package(unsigned char *tp_pkg)
{
    cache_free(wbuf_allocer,tp_pkg);
}
