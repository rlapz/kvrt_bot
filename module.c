#include <string.h>

#include "module.h"

#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/filter.h"
#include "builtin/general.h"


static int _cmd_compare(const char cmd[], const BotCmd *src);

/* ret: 0: command not found */
static int _builtin_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);

/* ret: 0: command not found */
static int _external_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);


int
module_init(Module *m, int64_t owner_id, TgApi *api, Db *db, const char cmd_path[])
{
	const int ret = str_init_alloc(&m->str, 1024);
	if (ret < 0) {
		log_err(ret, "module: module_init: str_init_alloc");
		return -1;
	}

	m->owner_id = owner_id;
	m->api = api;
	m->db = db;
	m->cmd_path = cmd_path;
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
static inline int
_cmd_compare(const char cmd[], const BotCmd *src)
{
	return cstr_casecmp_n(cmd, src->name, (size_t)src->name_len);
}


static int
_builtin_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj)
{
	if (_cmd_compare("/cmd_set", cmd))
		general_cmd_set_enable(m, msg, cmd->args, cmd->args_len);
	else if (_cmd_compare("/admin_reload", cmd))
		general_admin_reload(m, msg);
	else if (_cmd_compare("/dump", cmd))
		general_dump(m, msg, json_obj);
	else if (_cmd_compare("/dump_admin", cmd))
		general_dump_admin(m, msg);
	else if (general_message(m, msg, cmd->name, cmd->name_len) == 0)
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
