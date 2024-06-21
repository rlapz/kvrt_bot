#include "tg_api.h"
#include "config.h"
#include "util.h"


int
tg_api_init(TgApi *t, const char base_api[])
{
	int ret = str_init_alloc(&t->str, 1024);
	if (ret < 0)
		return ret;

	return 0;
}


void
tg_api_deinit(TgApi *t)
{
	str_deinit(&t->str);
}
