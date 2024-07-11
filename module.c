#include <string.h>

#include "module.h"
#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/general.h"
#include "builtin/anti_lewd.h"


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
module_builtin_handle_text(Module *m, const TgMessage *msg)
{
	const char *const text = msg->text.text;
	anti_lewd_detect_text(m, msg, text);
}


void
module_builtin_handle_command(Module *m, const char cmd[], const TgMessage *msg,
			      json_object *json_obj, const char *args)
{
	str_reset(&m->str, 0);

	if (strcmp(cmd, "/start") == 0)
		general_start(m, msg);
	else if (strcmp(cmd, "/help") == 0)
		general_help(m, msg);
	else if (strcmp(cmd, "/settings") == 0)
		general_settings(m, msg);
	else if (strcmp(cmd, "/dump") == 0)
		general_dump(m, msg, json_obj);
	else
		general_inval(m, msg);

	(void)args;
}


void
module_builtin_handle_media(Module *m, const TgMessage *msg)
{
	/* TODO */
	(void)m;
	(void)msg;
}
