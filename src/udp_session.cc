#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include "uv.h"
#include "session.h"
#include "proto_man.h"
#include "udp_session.h"
using namespace std;

void udp_session::close()
{

}

static void on_uv_udp_send_end(uv_udp_send_t* req, int status)
{
    if(status == 0)
    {
        //printf("send success\n");
    }
    free(req);
}

void udp_session::send_data(unsigned char *body, int len)
{
    uv_buf_t w_buf;
    w_buf = uv_buf_init((char*)body,len);
    uv_udp_send_t* req = (uv_udp_send_t*)malloc(sizeof(uv_udp_send_t));

    uv_udp_send(req,this->udp_handler,&w_buf,1,this->addr,on_uv_udp_send_end);
}

const char* udp_session::get_address(int* port)
{
    *port = this->c_port;
    return this->c_address;
}

void udp_session::send_msg(struct cmd_msg *msg)
{
    unsigned char* encode_pkg = NULL;
    int encode_len = 0;
    encode_pkg = proto_man::encode_msg_to_raw(msg, &encode_len);
    if(encode_pkg)
    {
        this->send_data(encode_pkg, encode_len);
        proto_man::msg_raw_free(encode_pkg);
    }
}

void udp_session::send_raw_cmd(raw_cmd* raw)
{
	this->send_data(raw->raw_data, raw->raw_len);
}
