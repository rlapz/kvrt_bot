#ifndef __MODULE_H__
#define __MODULE_H__


#include "tg_api.h"
#include "util.h"


typedef struct module {
	TgApi *api;
	Str    str;
} Module;


#endif
