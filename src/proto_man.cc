#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>
#include "proto_man.h"
#include "cache_alloc.h"
#include "small_alloc.h"
//#include "game.pb.h"  // 引入由game.proto生成的头文件

extern cache_allocer* wbuf_allocer;
#define my_malloc small_alloc
#define my_free small_free

#define CMD_HEADER 8
static int g_proto_type = PROTO_BUF;
// 假设这里用一个结构体来表示消息类型标识与具体Protobuf消息类型的映射关系

static std::vector<ProtoMsgMapping> g_pb_msg_mappings;

// 初始化函数，传入具体的消息类型映射关系（示例，可根据实际调整参数形式等）
void proto_man::init(int proto_type, const std::vector<ProtoMsgMapping>& msg_mappings)
{
    g_proto_type = proto_type;
    g_pb_msg_mappings = msg_mappings;
}

int proto_man::proto_type()
{
    return g_proto_type;
}

// 注册Protobuf命令映射关系，这里简化为注册消息类型映射（示例，可按需完善）
void proto_man::register_protobuf_cmd_map(const std::vector<ProtoMsgMapping>& msg_mappings)
{
    g_pb_msg_mappings = msg_mappings;
}

// 根据命令类型（ctype）查找对应的Protobuf消息类型描述符（示例，可优化查找逻辑等）
const google::protobuf::Descriptor* find_descriptor_by_ctype(int ctype) {
    for (const auto& mapping : g_pb_msg_mappings) {
        if (mapping.ctype == ctype) {
            return mapping.descriptor;
        }
    }
    return nullptr;
}

// 创建消息的函数修改为利用反射动态创建消息
google::protobuf::Message* proto_man::create_message(int ctype) {
    const google::protobuf::Descriptor* descriptor = find_descriptor_by_ctype(ctype);
    if (descriptor) {
        const google::protobuf::Message* prototype =
            google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype) {
            return prototype->New();
        }
    }
    return nullptr;
}

void proto_man::release_message(google::protobuf::Message* m)
{
    delete m;
}

bool proto_man::decode_raw_cmd(unsigned char* cmd, int cmd_len, raw_cmd* raw)
{
    if (cmd_len < CMD_HEADER)
    {
        return false;
    }
    raw->stype = cmd[0] | (cmd[1] << 8);
    raw->ctype = cmd[2] | (cmd[3] << 8);
    raw->utag = cmd[4] | (cmd[5] << 8) | (cmd[6] << 16) | (cmd[7] << 24);
    raw->raw_data = cmd;
    raw->raw_len = cmd_len;
    return true;
}

bool proto_man::decode_cmd_msg(unsigned char* cmd, int cmd_len, cmd_msg** out_msg)
{
    *out_msg = NULL;
    if (cmd_len < CMD_HEADER)
    {
        return false;
    }
    struct cmd_msg* msg = (struct cmd_msg*)my_malloc(sizeof(struct cmd_msg));
    // memset(msg, 0, sizeof(struct cmd_msg*));
    msg->stype = cmd[0] | (cmd[1] << 8);
    msg->ctype = cmd[2] | (cmd[3] << 8);
    msg->utag = cmd[4] | (cmd[5] << 8) | (cmd[6] << 16) | (cmd[7] << 24);
    msg->body = NULL;

    *out_msg = msg;
    if (cmd_len == CMD_HEADER)
    {
        return true;
    }

    if (g_proto_type == PROTO_JSON)
    {
        int json_len = cmd_len - CMD_HEADER;
        // char* json_str = (char*)malloc(json_len + 1);
        char* json_str = (char*)cache_alloc(wbuf_allocer, json_len + 1);
        memcpy(json_str, cmd + CMD_HEADER, json_len);
        json_str[json_len] = '\0';
        msg->body = (void*)json_str;
    }
    else
    {
        google::protobuf::Message* p_m = create_message(msg->ctype);  // 根据ctype动态创建消息
        if (p_m == NULL)
        {
            my_free(msg);
            *out_msg = NULL;
            return false;
        }
        if (!p_m->ParseFromArray(cmd + CMD_HEADER, cmd_len - CMD_HEADER))
        {
            my_free(msg);
            *out_msg = NULL;
            release_message(p_m);
            return false;
        }
        msg->body = p_m;
    }
    return true;
}

void proto_man::cmd_msg_free(cmd_msg* msg)
{
    if (msg->body)
    {
        if (g_proto_type == PROTO_JSON)
        {
            // free(msg->body);
            cache_free(wbuf_allocer, msg->body);
            msg->body = NULL;
        }
        else
        {
            google::protobuf::Message* p_m = (google::protobuf::Message*)msg->body;
            delete p_m;
            msg->body = NULL;
        }
    }
    my_free(msg);
}

unsigned char* proto_man::encode_msg_to_raw(const cmd_msg* msg, int* out_len)
{
    int raw_len = 0;
    unsigned char* raw_data = NULL;

    *out_len = 0;
    if (g_proto_type == PROTO_JSON)
    {
        char* json_str = NULL;
        int len = 0;
        if (msg->body!= NULL)
        {
            json_str = (char*)msg->body;
            len = strlen(json_str);
        }
        // raw_data = (unsigned char*)malloc(CMD_HEADER + len);
        raw_data = (unsigned char*)cache_alloc(wbuf_allocer, CMD_HEADER + len);
        if (msg->body!= NULL)
        {
            memcpy(raw_data + CMD_HEADER, json_str, len);
        }
        *out_len = (CMD_HEADER + len);
    }
    else if (g_proto_type == PROTO_BUF)
    {
        google::protobuf::Message* p_m = NULL;
        int pf_len = 0;
        if (msg->body!= NULL)
        {
            p_m = (google::protobuf::Message*)msg->body;
            pf_len = p_m->ByteSizeLong();
        }
        // raw_data = (unsigned char*)malloc(CMD_HEADER + pf_len);
        raw_data = (unsigned char*)cache_alloc(wbuf_allocer, CMD_HEADER + pf_len);
        if (msg->body!= NULL)
        {
            if (!p_m->SerializePartialToArray(raw_data + CMD_HEADER, pf_len))
            {
                // free(raw_data);
                cache_free(wbuf_allocer, raw_data);
                return NULL;
            }
        }
        *out_len = (pf_len + CMD_HEADER);
    }
    else
    {
        return NULL;
    }

    raw_data[0] = (msg->stype & 0xff);
    raw_data[1] = ((msg->stype & 0xff00) >> 8);
    raw_data[2] = (msg->ctype & 0xff);
    raw_data[3] = ((msg->ctype & 0xff00) >> 8);
    memcpy(raw_data + 4, &msg->utag, 4);

    return raw_data;
}

void proto_man::msg_raw_free(unsigned char* raw)
{
    cache_free(wbuf_allocer, raw);
}
