#ifndef __MODULE_H__
#define __MODULE_H__


#include "tg_api.h"
#include "util.h"


typedef struct module {
	TgApi *api;
	Str    str;
} Module;

static inline int
module_init(Module *m, TgApi *api)
{
	if (str_init_alloc(&m->str, 1024) < 0)
		return -1;

	m->api = api;
	return 0;
}


static inline void
module_deinit(Module *m)
{
	str_deinit(&m->str);
}


#endif
