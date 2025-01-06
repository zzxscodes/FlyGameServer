#ifndef __PROTO_MAN_H__
#define __PROTO_MAN_H__

#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>
#include <map>

enum 
{
    PROTO_JSON = 0,
    PROTO_BUF = 1,
};

struct cmd_msg
{
    int stype;
    int ctype;
    unsigned int utag;
    void* body;
};

struct raw_cmd
{
    int stype;
    int ctype;
    unsigned int utag;
    unsigned char* raw_data;
    int raw_len;
};

struct ProtoMsgMapping {
    int ctype;
    const google::protobuf::Descriptor* descriptor;
};

class proto_man {
public:
    // 修改init函数声明
    static void init(int proto_type, const std::vector<ProtoMsgMapping>& msg_mappings);
    static int proto_type();
    // 修改register_protobuf_cmd_map函数声明
    static void register_protobuf_cmd_map(const std::vector<ProtoMsgMapping>& msg_mappings);
    static const char* protobuf_cmd_name(int ctype);


    static bool decode_raw_cmd(unsigned char* cmd, int cmd_len, struct raw_cmd* raw);
    static bool decode_cmd_msg(unsigned char* cmd, int cmd_len, struct cmd_msg** out_msg);
    static void cmd_msg_free(struct cmd_msg* msg);

    static unsigned char* encode_msg_to_raw(const struct cmd_msg* msg, int* out_len);
    static void msg_raw_free(unsigned char* raw);

    static google::protobuf::Message* create_message(int ctype);
    static void release_message(google::protobuf::Message* m);
};
#endif