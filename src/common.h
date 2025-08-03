#ifndef __COMMON_H__
#define __COMMON_H__


#include <json.h>
#include <stdint.h>
#include <time.h>

#include "config.h"
#include "tg.h"
#include "tg_api.h"


/*
 * tg_api wrappers
 */
#define SEND_TEXT_PLAIN(MSG, TYPE)\
	tg_api_send_text(TG_API_TEXT_TYPE_PLAIN, MSG->chat.id, MSG->id, TYPE, NULL)

#define SEND_TEXT_FORMAT(MSG, TYPE)\
	tg_api_send_text(TG_API_TEXT_TYPE_FORMAT, MSG->chat.id, MSG->id, TYPE, NULL)

#define SEND_TEXT_PLAIN_FMT(MSG, DELETABLE, RET_ID, ...)\
	send_text_fmt(MSG, TG_API_TEXT_TYPE_PLAIN, DELETABLE, RET_ID, __VA_ARGS__)

#define SEND_TEXT_FORMAT_FMT(MSG, DELETABLE, RET_ID, ...)\
	send_text_fmt(MSG, TG_API_TEXT_TYPE_FORMAT, DELETABLE, RET_ID, __VA_ARGS__)

#define ANSWER_CALLBACK_TEXT(ID, TEXT, SHOW_ALERT)\
	tg_api_answer_callback(TG_API_ANSWER_CALLBACK_TYPE_TEXT, ID, TEXT, SHOW_ALERT)

#define ANSWER_CALLBACK_URL(ID, URL, SHOW_ALERT)\
	tg_api_answer_callback(TG_API_ANSWER_CALLBACK_TYPE_URL, ID, URL, SHOW_ALERT)


int send_text_fmt(const TgMessage *msg, int type, int deletable, int64_t *ret_id, const char fmt[], ...);
int send_photo_fmt(const TgMessage *msg, int type, int deletable, int64_t *ret_id, const char photo[],
		   const char fmt[], ...);


/*
 * misc
 */
int   is_admin(int64_t user_id, int64_t chat_id, int64_t owner_id);
char *tg_escape(const char src[]);


/*
 * MessageList
 */
typedef struct message_list_pagination {
	unsigned page_num;
	unsigned page_size;
	unsigned items_len;
	unsigned items_size;
} MessageListPagination;

void message_list_pagination_set(MessageListPagination *m, unsigned curr_page, unsigned per_page,
				 unsigned items_len, unsigned items_size);


typedef struct message_list {
	int64_t     id_user;
	int64_t     id_owner;
	int64_t     id_chat;
	int64_t     id_message;
	const char *id_callback;	/* NULL: not a callback */
	const char *ctx;
	const char *title;
	const char *body;

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
