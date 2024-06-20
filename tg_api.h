#ifndef __TG_API_H__
#define __TG_API_H__


#include "util.h"


typedef struct {
	const char *api;
	Str         str;
} TgApi;

int  tg_api_init(TgApi *t, const char base_api[]);
void tg_api_deinit(TgApi *t);


#endif
