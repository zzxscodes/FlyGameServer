#ifndef __UDP_SESSION_H__
#define __UDP_SESSION_H__

class udp_session : public session
{
public:
    virtual void close();
    virtual void send_data(unsigned char* body, int len);
    virtual const char* get_address(int* port);
    virtual void send_msg(struct cmd_msg* msg);
    virtual void send_raw_cmd(struct raw_cmd* raw);

    uv_udp_t* udp_handler;
    char c_address[32];
    int c_port;
    const struct sockaddr* addr;
};

#endif