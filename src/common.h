#ifndef __COMMON_H__
#define __COMMON_H__


#include <json.h>
#include <stdint.h>
#include <time.h>

#include "config.h"
#include "tg.h"
#include "tg_api.h"


/*
 * BotCmd
 */
typedef struct bot_cmd {
	const char *name;
	unsigned    name_len;
	const char *args;
	int         has_username;
} BotCmd;

int bot_cmd_parse(BotCmd *b, const char uname[], const char src[]);


/*
 * CallbackQuery
 */
typedef struct callback_query {
	const char *ctx;
	unsigned    ctx_len;
	const char *args;
} CallbackQuery;

int callback_query_parse(CallbackQuery *c, const char src[]);


/*
 * tg_api wrappers
 */
static inline void
send_text_plain(const TgMessage *msg, const char text[])
{
	tg_api_send_text(TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, text, NULL);
}

static inline void
send_text_format(const TgMessage *msg, const char text[])
{
	tg_api_send_text(TG_API_TEXT_TYPE_FORMAT, msg->chat.id, &msg->id, text, NULL);
}


/*
 * misc
 */
int   privileges_check(const TgMessage *msg, int64_t owner_id);
char *tg_escape(const char src[]);


/*
 * MessageList
 */
typedef struct message_list_pagination {
	int      has_next_page;
	unsigned current_page;
	unsigned total_page;
	unsigned total;
} MessageListPagination;

typedef struct message_list {
	const char            *id;
	const TgMessage       *msg;
	const char            *ctx;
	const char            *title;
	const char            *body;
	const char            *udata;
	MessageListPagination  pagination;
} MessageList;

int message_list_paginate(const MessageList *l, int64_t *ret_id);
int message_list_send(const MessageList *l, int64_t *ret_id);
int message_list_check_expiration(const char id[], time_t prev);


#endif
