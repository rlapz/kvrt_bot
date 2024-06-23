#ifndef __KVRT_BOT_H__
#define __KVRT_BOT_H__


#include <json.h>

#include <sys/epoll.h>

#include "thrd_pool.h"
#include "config.h"
#include "util.h"


typedef struct epoll_event Event;

typedef struct kvrt_bot_client KvrtBotClient;
struct kvrt_bot_client {
	int          fd;
	unsigned     slot;
	int          state;
	int          is_buffer_ready;
	int          is_req_valid;
	int          req_body_offt;
	size_t       req_total_len;
	json_object *req_body_json;
	size_t       bytes;
	Buffer       buffer_in;
	Event        event;
};

typedef struct kvrt_bot_client_stack KvrtBotClientStack;
struct kvrt_bot_client_stack {
	unsigned      count;
	unsigned      slots[CFG_CLIENTS_MAX];
	KvrtBotClient list[CFG_CLIENTS_MAX];
};

typedef struct kvrt_bot KvrtBot;
struct kvrt_bot {
	volatile int       is_alive;
	int                listen_fd;
	int                event_fd;
	Config             config;
	ThrdPool           thrd_pool;
	KvrtBotClientStack clients;
};

int  kvrt_bot_init(KvrtBot *k);
void kvrt_bot_deinit(KvrtBot *k);
int  kvrt_bot_run(KvrtBot *k);
void kvrt_bot_stop(KvrtBot *k);


#endif
