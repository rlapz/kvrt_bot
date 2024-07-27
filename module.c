#include <string.h>

#include "module.h"

#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/filter.h"
#include "builtin/general.h"


/* ret: 0: command not found */
static int _builtin_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);

/* ret: 0: command not found */
static int _external_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);


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
	filter_text(m, msg, text);

	(void)json_obj;
}


void
module_handle_command(Module *m, const TgMessage *msg, json_object *json_obj)
{
	BotCmd cmd;
	if (bot_cmd_parse(&cmd, '/', msg->text.text) < 0)
		goto err0;

	if (_builtin_handle_command(m, &cmd, msg, json_obj))
		return;

	if (_external_handle_command(m, &cmd, msg, json_obj))
		return;

err0:
	general_inval(m, msg);
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
_builtin_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj)
{
	const char * const cmd_name = cmd->name;
	const size_t cmd_name_len = cmd->name_len;


	if (general_global(m, msg, cmd_name, cmd_name_len))
		return 1;

	if (cstr_casecmp_n("/cmd_set", cmd_name, cmd_name_len))
		general_cmd_set(m, msg, cmd->args, cmd->args_len);
	else if (cstr_casecmp_n("/dump", cmd_name, cmd_name_len))
		general_dump(m, msg, json_obj);
	else if (cstr_casecmp_n("/test", cmd_name, cmd_name_len))
		general_test(m, msg, cmd->args, cmd->args_len);
	else
		return 0;

	return 1;
}


static int
_external_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj)
{
	(void)m;
	(void)cmd;
	(void)msg;
	(void)json_obj;
	return 0;
}
