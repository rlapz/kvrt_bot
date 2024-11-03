#ifndef __COMMON_H__
#define __COMMON_H__


#include <update.h>


int  common_privileges_check(Update *u, const TgMessage *msg);
void common_send_text_plain(Update *u, const TgMessage *msg, const char plain[]);
void common_send_text_format(Update *u, const TgMessage *msg, const char text[]);


#endif
