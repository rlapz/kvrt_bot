#include <string.h>

#include "module.h"
#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/general.h"
#include "builtin/anime_schedule.h"


int
module_init(Module *m, TgApi *api)
{
	const int ret = str_init_alloc(&m->str, 1024);
	if (ret < 0) {
		log_err(ret, "module: module_init: str_init_alloc");
		return -1;
	}

	m->api = api;
	return 0;
}


void
module_deinit(Module *m)
{
	str_deinit(&m->str);
}


void
module_builtin_handle_command(Module *m, const char cmd[], const TgMessage *msg, const char args[])
{
	if (strcmp(cmd, "/start") == 0)
		general_start(m, msg, args);
	else if (strcmp(cmd, "/help") == 0)
		general_help(m, msg, args);
	else if (strcmp(cmd, "/anime_schedule") == 0)
		anime_schedule(m, msg, args);
}
