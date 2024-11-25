#ifndef __COMMON_H__
#define __COMMON_H__


#include <update.h>


int   common_privileges_check(Update *u, const TgMessage *msg);
char *common_tg_escape(char dest[], const char src[]);


static inline void
common_send_text_plain(Update *u, const TgMessage *msg, const char plain[])
{
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, plain);
}


static inline void
common_send_text_format(Update *u, const TgMessage *msg, const char text[])
{
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_FORMAT, msg->chat.id, &msg->id, text);
}


#endif
