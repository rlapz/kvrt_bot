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
#define SEND_TEXT_PLAIN(MSG, TEXT)\
	send_text(MSG, TG_API_PARSE_TYPE_PLAIN, TEXT, NULL)

#define SEND_TEXT_FORMAT(MSG, TEXT)\
	send_text(MSG, TG_API_PARSE_TYPE_FORMAT, TEXT, NULL)

#define SEND_TEXT_PLAIN_FMT(MSG, RET_ID, ...)\
	send_text_fmt(MSG, TG_API_PARSE_TYPE_PLAIN, RET_ID, __VA_ARGS__)

#define SEND_TEXT_FORMAT_FMT(MSG, RET_ID, ...)\
	send_text_fmt(MSG, TG_API_PARSE_TYPE_FORMAT, RET_ID, __VA_ARGS__)

#define ANSWER_CALLBACK_TEXT(ID, TEXT, SHOW_ALERT)\
	answer_callback(TG_API_VALUE_TYPE_TEXT, ID, TEXT, SHOW_ALERT)

#define ANSWER_CALLBACK_URL(ID, URL, SHOW_ALERT)\
	answer_callback(TG_API_VALUE_TYPE_URL, ID, URL, SHOW_ALERT)


int send_text(const TgMessage *msg, int parse_type, const char text[], int64_t *ret_id);
int send_photo(const TgMessage *msg, int parse_type, const char photo[], const char caption[],
	       int64_t *ret_id);
int answer_callback(int value_type, const char id[], const char value[], int show_alert);
int delete_message(int64_t chat_id, int64_t message_id);

int send_text_fmt(const TgMessage *msg, int parse_type, int64_t *ret_id, const char fmt[], ...);
int send_photo_fmt(const TgMessage *msg, int parse_type, int64_t *ret_id, const char photo[],
		   const char fmt[], ...);



/*
 * misc
 */
int   is_admin(int64_t user_id, int64_t chat_id, int64_t owner_id);
char *tg_escape(const char src[]);


/*
 * Pager
 */
typedef struct pager_list {
	unsigned page_num;
	unsigned page_size;
	unsigned items_len;
	unsigned items_size;
} PagerList;

void pager_list_set(PagerList *p, unsigned curr_page, unsigned per_page, unsigned items_len,
		    unsigned items_size);


typedef struct pager {
	int64_t     id_user;
	int64_t     id_owner;
	int64_t     id_chat;
	int64_t     id_message;
	const char *id_callback;	/* NULL: not a callback */
	const char *ctx;
	const char *title;
	const char *body;

	/* filled by pager_init() */
	int64_t     created_by;
	const char *udata;
	unsigned    page;
} Pager;

/* pager_init:
 * ret:  0: success
 *      -1: error
 *      -2: expired
 *      -3: deleted
 */
int pager_init(Pager *p, const char args[]);
int pager_send(const Pager *p, const PagerList *list, int64_t *ret_id);


#endif
