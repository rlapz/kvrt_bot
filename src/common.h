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
#define SEND_ERROR_TEXT(MSG, RET_ID, FMT, ...)\
	send_error_text(MSG, RET_ID, __func__, FMT, __VA_ARGS__)

int send_text_plain(const TgMessage *msg, int64_t *ret_id, const char fmt[], ...);
int send_text_format(const TgMessage *msg, int64_t *ret_id, const char fmt[], ...);
int send_photo_plain(const TgMessage *msg, int64_t *ret_id, const char photo[], const char fmt[], ...);
int send_photo_format(const TgMessage *msg, int64_t *ret_id, const char photo[], const char fmt[], ...);

int send_error_text(const TgMessage *msg, int64_t *ret_id, const char ctx[], const char fmt[], ...);

int answer_callback_text(const char id[], const char value[], int show_alert);
int delete_message(const TgMessage *msg);

char *new_deleter(int64_t user_id);


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
