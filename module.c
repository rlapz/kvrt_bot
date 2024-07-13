#include <string.h>

#include "module.h"
#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/general.h"
#include "builtin/anti_lewd.h"


static int  _builtin_handle_command(Module *m, const char cmd[], const TgMessage *msg,
				    json_object *json_obj, const char args[]);
static int  _external_handle_command(Module *m, const char cmd[], const TgMessage *msg,
				     json_object *json_obj, const char args[]);


int
 module_init(Module *m, TgApi *api, Db *db)
{
	const int ret = str_init_alloc(&m->str, 1024);
	if (ret < 0) {
		log_err(ret, "module: module_init: str_init_alloc");
		return -1;
	}

	m->api = api;
	m->db = db;
	return 0;
}


void
module_deinit(Module *m)
{
	str_deinit(&m->str);
}


void
module_handle_text(Module *m, const TgMessage *msg, json_object *json_obj)
{
	const char *const text = msg->text.text;
	anti_lewd_detect_text(m, msg, text);

	(void)json_obj;
}


void
module_handle_command(Module *m, const char cmd[], const TgMessage *msg, json_object *json_obj,
		      const char args[])
{
	if (_builtin_handle_command(m, cmd, msg, json_obj, args) == 0)
		return;

	if (_external_handle_command(m, cmd, msg, json_obj, args) == 0)
		return;

	general_inval(m, msg);
}


void
module_handle_media(Module *m, const TgMessage *msg, json_object *json_obj)
{
	(void)m;
	(void)msg;
	(void)json_obj;
}


void
module_handle_inline_query(Module *m, const TgInlineQuery *query, json_object *json_obj)
{
	(void)m;
	(void)query;
	(void)json_obj;
}


void
module_handle_callback_query(Module *m, const TgCallbackQuery *query, json_object *json_obj)
{
	(void)m;
	(void)query;
	(void)json_obj;
}


/*
 * private
 */
static int
_builtin_handle_command(Module *m, const char cmd[], const TgMessage *msg, json_object *json_obj,
			const char args[])
{
	if (strcmp(cmd, "/start") == 0)
		general_start(m, msg);
	else if (strcmp(cmd, "/help") == 0)
		general_help(m, msg);
	else if (strcmp(cmd, "/settings") == 0)
		general_settings(m, msg);
	else if (strcmp(cmd, "/dump") == 0)
		general_dump(m, msg, json_obj);
	else
		return -1;

	(void)args;
	return 0;
}


static int
_external_handle_command(Module *m, const char cmd[], const TgMessage *msg, json_object *json_obj,
			 const char args[])
{
	(void)m;
	(void)cmd;
	(void)msg;
	(void)json_obj;
	(void)args;
	return -1;
}
