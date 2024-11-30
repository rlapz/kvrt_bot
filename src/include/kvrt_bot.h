#ifndef __KVRT_BOT_H__
#define __KVRT_BOT_H__


#include <json.h>

#include <sys/epoll.h>

#include <config.h>
#include <thrd_pool.h>
#include <util.h>


typedef struct epoll_event Event;

typedef struct kvrt_bot_client {
	int          fd;
	int          state;
	int          is_success;
	size_t       body_len;
	json_object *body;
	size_t       bytes;
	Event        event;
	char         buffer[CFG_CLIENT_BUFFER_SIZE];
} KvrtBotClient;

typedef struct kvrt_bot {
	volatile int is_alive;
	int          listen_fd;
	int          event_fd;
	unsigned     clients_count;
	Config       config;
	Chld         chld;
	Mp           clients;
	ThrdPool     thrd_pool;
} KvrtBot;

int  kvrt_bot_init(KvrtBot *k);
void kvrt_bot_deinit(KvrtBot *k);
int  kvrt_bot_run(KvrtBot *k);
void kvrt_bot_stop(KvrtBot *k);


#endif
