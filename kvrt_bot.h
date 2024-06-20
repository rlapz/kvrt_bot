#ifndef __KVRT_BOT_H__
#define __KVRT_BOT_H__


#include <sys/epoll.h>

#include "thrd_pool.h"
#include "config.h"
#include "util.h"
#include "json.h"


typedef struct epoll_event Event;

typedef struct {
	int           fd;
	unsigned      slot;
	int           state;
	int           is_buffer_ready;
	int           is_req_valid;
	char         *req_body;
	size_t        req_body_len;
	size_t        req_remn_len;
	json_value_t *req_body_json;
	size_t        bytes;
	Buffer        buffer_in;
	Event         event;
} KvrtBotClient;

typedef struct {
	unsigned      count;
	unsigned      slots[CFG_CLIENTS_MAX];
	KvrtBotClient list[CFG_CLIENTS_MAX];
} KvrtBotClientStack;

typedef struct {
	volatile int       is_alive;
	int                listen_fd;
	int                event_fd;
	Config             config;
	ThrdPool           thrd_pool;
	KvrtBotClientStack clients;
} KvrtBot;

int  kvrt_bot_init(KvrtBot *k);
void kvrt_bot_deinit(KvrtBot *k);
int  kvrt_bot_run(KvrtBot *k);
void kvrt_bot_stop(KvrtBot *k);


#endif
