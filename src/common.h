#ifndef __COMMON_H__
#define __COMMON_H__


#include <json.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "config.h"
#include "tg.h"
#include "tg_api.h"


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
int   is_admin(int64_t user_id, int64_t chat_id, int64_t owner_id);
char *tg_escape(const char src[]);


/*
 * MessageList
 */
typedef struct message_list_pagination {
	int      has_next_page;
	unsigned page_count;
	unsigned page_size;
	unsigned items_count;
	unsigned items_size;
} MessageListPagination;

typedef struct message_list {
	int64_t          id_user;
	int64_t          id_owner;
	const char      *id_callback;	/* NULL: not a callback */
	const char      *ctx;
	const char      *title;
	const char      *body;
	const TgMessage *msg;

	/* filled by message_list_init() */
	int64_t     created_by;
	const char *udata;
	unsigned    page;
} MessageList;

/* message_list_init:
 * ret:  0: success
 *      -1: error
 *      -2: expired
 *      -3: deleted
 */
int message_list_init(MessageList *l, const char args[]);
int message_list_send(const MessageList *l, const MessageListPagination *pag, int64_t *ret_id);


#endif
