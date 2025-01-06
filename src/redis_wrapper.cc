#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "uv.h"
#include "redis_wrapper.h"

#define my_malloc malloc
#define my_free free

struct connect_req
{
    char* ip;
    int port;
    void(*open_cb)(const char* err, void* context, void* udata);
    char* err;
    void* context;
    void* udata;
};

struct redis_context
{
    void* pConn;
    uv_mutex_t lock;
    int is_closed;
};

// 连接相关工作函数保持不变，但内存分配等操作现在更具跨平台性
static void connect_work(uv_work_t* req)
{
    struct connect_req* r = (struct connect_req*)req->data;
    struct timeval timeout = {5,0};
    redisContext* rc = redisConnectWithTimeout(r->ip,r->port,timeout);
    if(rc->err)
    {
        r->err = strdup(rc->errstr);
        r->context = NULL;
        redisFree(rc);
    }
    else
    {
        struct redis_context* c = (struct redis_context*)my_malloc(sizeof(struct redis_context));
        memset(c,0,sizeof(struct redis_context));
        c->pConn = rc;
        uv_mutex_init(&c->lock);
        r->err = NULL;
        r->context = c;
    }
}

static void on_connect_complete(uv_work_t* req, int status)
{
    struct connect_req* r = (struct connect_req*)req->data;
    r->open_cb(r->err, r->context, r->udata);

    if (r->ip)
    {
        free(r->ip);
    }
    if (r->err)
    {
        free(r->err);
    }
    my_free(r);
    my_free(req);
}

void redis_wrapper::connect(char* ip, int port, void(*open_cb)(const char* err,void* context,void* udata), void* udata)
{
    uv_work_t* w = (uv_work_t*)my_malloc(sizeof(uv_work_t));
    memset(w, 0, sizeof(uv_work_t));
    struct connect_req* r = (struct connect_req*)my_malloc(sizeof(struct connect_req));
    memset(r, 0, sizeof(struct connect_req));
    r->ip = strdup(ip);
    r->port = port;
    r->open_cb = open_cb;
    r->udata = udata;
    w->data = (void*)r;
    uv_queue_work(uv_default_loop(), w, connect_work, on_connect_complete);
}

// 关闭相关工作函数保持基本逻辑，内存释放等符合跨平台调整后的规则
static void close_work(uv_work_t* req)
{
    struct redis_context* r = (struct redis_context*)(req->data);
    uv_mutex_lock(&r->lock);
    redisContext* c = (redisContext*)r->pConn;
    redisFree(c);
    r->pConn = NULL;
    uv_mutex_unlock(&r->lock);
}

static void on_close_complete(uv_work_t* req, int status)
{
    struct redis_context* r = (struct redis_context*)(req->data);
    my_free(r);
    my_free(req);
}

void redis_wrapper::close_redis(void* context)
{
    struct redis_context* c = (struct redis_context*)context;
    if(c->is_closed)
    {
        return;
    }
    uv_work_t* w = (uv_work_t*)my_malloc(sizeof(uv_work_t));
    memset(w, 0, sizeof(uv_work_t));
    w->data = (context);

    c->is_closed = 1;
    uv_queue_work(uv_default_loop(), w, close_work, on_close_complete);
}

struct query_req
{
    void* context;
    char* cmd;
    void(*query_cb)(const char* err, redisReply* result, void* udata);
    char* err;
    redisReply* result;
    void* udata;
};

// 查询工作函数内部，注意处理字符串和数据结构时考虑跨平台的一致性
static void query_work(uv_work_t* req)
{
    query_req* r = (query_req*)req->data;
    
    struct redis_context* my_conn = (struct redis_context*)(r->context);
    struct redisContext* rc = (struct redisContext*)my_conn->pConn;

    uv_mutex_lock(&my_conn->lock);
    redisReply* reply = (redisReply*)redisCommand(rc,r->cmd);
    if(reply->type == REDIS_REPLY_ERROR)
    {
        r->err = strdup(reply->str);
        r->result = NULL;
        freeReplyObject(reply);
    }
    else
    {
        r->result = reply;
        r->err = NULL;
    }
    uv_mutex_unlock(&my_conn->lock);
}

static void on_query_complete(uv_work_t* req, int status)
{
    query_req* r = (query_req*)req->data;
    r->query_cb(r->err, r->result, r->udata);
    if (r->cmd)
    {
        free(r->cmd);
    }
    if (r->result)
    {
        freeReplyObject(r->result);
    }
    if (r->err)
    {
        free(r->err);
    }
    my_free(r);
    my_free(req);
}

void redis_wrapper::query(void* context, char* cmd, void(*query_cb)(const char* err,redisReply* result, void* udata), void* udata)
{
    struct redis_context* c = (struct redis_context*)context;
    if(c->is_closed)
    {
        return;
    }
    uv_work_t* w = (uv_work_t*)my_malloc(sizeof(uv_work_t));
    query_req* r = (query_req*)my_malloc(sizeof(query_req));
    memset(r, 0, sizeof(query_req));
    r->context = context;
    r->cmd = strdup(cmd);
    r->query_cb = query_cb;
    r->udata = udata;
    w->data = r;
    uv_queue_work(uv_default_loop(), w, query_work, on_query_complete);
}