#ifndef __KVRT_BOT_H__
#define __KVRT_BOT_H__


#include <json.h>

#include <sys/epoll.h>

#include "config.h"
#include "thrd_pool.h"
#include "util.h"


typedef struct epoll_event Event;

typedef struct kvrt_bot_client {
	int          fd;
	unsigned     slot;
	int          state;
	int          req_body_offt;
	size_t       req_total_len;
	json_object *req_body_json;
	size_t       bytes;
	Buffer       buffer_in;
	Event        event;
} KvrtBotClient;

typedef struct kvrt_bot_client_stack {
	unsigned      count;
	unsigned      slots[CFG_CLIENTS_MAX];
	KvrtBotClient list[CFG_CLIENTS_MAX];
} KvrtBotClientStack;

typedef struct kvrt_bot {
	volatile int       is_alive;
	int                listen_fd;
	int                event_fd;
	Config             config;
	Chld               chld;
	ThrdPool           thrd_pool;
	KvrtBotClientStack clients;
} KvrtBot;

int  kvrt_bot_init(KvrtBot *k);
void kvrt_bot_deinit(KvrtBot *k);
int  kvrt_bot_run(KvrtBot *k);
void kvrt_bot_stop(KvrtBot *k);


#endif
