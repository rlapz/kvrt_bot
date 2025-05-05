#ifndef __API_H__
#define __API_H__


#include "stdint.h"


enum {
	API_TYPE_TEXT_PLAIN = 0,
 	API_TYPE_TEXT_FORMAT,
	API_TYPE_LIST,
	API_TYPE_CALLBACK_ANSWER,

	__API_TYPE_END,
};

/*
 * thin wrapper tg_api_* for external command
 */
int api_send_text(int type, int64_t chat_id, const int64_t *reply_to, const char text[], int64_t *ret_id);
int api_delete_message(int64_t chat_id, int64_t message_id);
int api_answer_callback(const char id[], const char text[], const char url[], int show_alert);


#endif
