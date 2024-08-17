#include <assert.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include "module.h"

#include "tg.h"
#include "tg_api.h"
#include "util.h"

#include "builtin/filter.h"
#include "builtin/general.h"


static int  _cmd_compare(const char cmd[], const BotCmd *src);
static void _handle_message(Module *m, const TgMessage *t, json_object *json_obj);
static void _handle_text(Module *m, const TgMessage *msg, json_object *json_obj);
static void _handle_command(Module *m, const TgMessage *msg, json_object *json_obj);
static void _handle_new_member(Module *m, const TgMessage *msg);
static void _handle_inline_query(Module *m, const TgInlineQuery *query, json_object *json_obj);
static void _handle_callback_query(Module *m, const TgCallbackQuery *query, json_object *json_obj);

/* ret: 0: command not found */
static int _builtin_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);

/* ret: 0: command not found */
static int _external_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj);


/*
 * Public
 */
int
module_init(Module *m, Chld *chld, int64_t bot_id, int64_t owner_id, const char base_api[],
	    const char cmd_path[], const char db_path[])
{
	if (db_init(&m->db, db_path) < 0)
		return -1;

	if (tg_api_init(&m->api, base_api) < 0)
		goto err0;

	if (str_init_alloc(&m->str, 1024) < 0) {
		log_err(ENOMEM, "module: module_init: str_init_alloc");
		goto err1;
	}

	m->chld = chld;
	m->bot_id = bot_id;
	m->owner_id = owner_id;
	m->cmd_path = cmd_path;
	return 0;

err1:
	tg_api_deinit(&m->api);
err0:
	db_deinit(&m->db);
	return -1;
}


void
module_deinit(Module *m)
{
	str_deinit(&m->str);
	tg_api_deinit(&m->api);
	db_deinit(&m->db);
}


void
module_handle(Module *m, json_object *json_obj)
{
	TgUpdate update;
	if (tg_update_parse(&update, json_obj) < 0) {
		json_object_put(json_obj);
		return;
	}

	log_debug("module: module_handle: %s", json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY));

	switch (update.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(m, &update.message, json_obj);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		_handle_callback_query(m, &update.callback_query, json_obj);
		break;
	case TG_UPDATE_TYPE_INLINE_QUERY:
		_handle_inline_query(m, &update.inline_query, json_obj);
		break;
	default:
		break;
	}

	tg_update_free(&update);
	json_object_put(json_obj);
}


/*
 * private
 */
static inline int
_cmd_compare(const char cmd[], const BotCmd *src)
{
	return cstr_casecmp_n(cmd, src->name, (size_t)src->name_len);
}


static void
_handle_message(Module *m, const TgMessage *t, json_object *json_obj)
{
	if (t->from == NULL)
		return;

	log_debug("module: _handle_message: message type: %s", tg_message_type_str(t->type));
	switch (t->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		_handle_command(m, t, json_obj);
		break;
	case TG_MESSAGE_TYPE_TEXT:
		_handle_text(m, t, json_obj);
		break;
	case TG_MESSAGE_TYPE_NEW_MEMBER:
		_handle_new_member(m, t);
		break;
	default:
		break;
	}
}


static void
_handle_text(Module *m, const TgMessage *msg, json_object *json_obj)
{
	const char *const text = msg->text.text;
	filter_text(m, msg, text);

	(void)json_obj;
}


static void
_handle_command(Module *m, const TgMessage *msg, json_object *json_obj)
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


static void
_handle_new_member(Module *m, const TgMessage *msg)
{
	const TgUser *const user = &msg->new_member;
	if (user->id == m->bot_id)
		general_admin_reload(m, msg);
}


static void
_handle_inline_query(Module *m, const TgInlineQuery *query, json_object *json_obj)
{
	(void)m;
	(void)query;
	(void)json_obj;
}


static void
_handle_callback_query(Module *m, const TgCallbackQuery *query, json_object *json_obj)
{
	(void)m;
	(void)query;
	(void)json_obj;
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
	else if (_cmd_compare("/cmd_msg", cmd))
		general_message_set(m, msg, cmd->args, cmd->args_len);
	else if (general_message_get(m, msg, cmd->name, cmd->name_len) == 0)
		return 0;

	return 1;
}


/* not tested yet! */
static int
_external_handle_command(Module *m, const BotCmd *cmd, const TgMessage *msg, json_object *json_obj)
{
	DbCmd db_cmd;
	char cmd_name[34];
	char *argv[DB_CMD_ARGS_MAX_SIZE + 1];
	const int64_t chat_id = msg->chat.id;


	cmd_name[0] = '/';
	cstr_copy_n2(cmd_name + 1, LEN(cmd_name) - 1, cmd->name, cmd->name_len);

	const int ret = db_cmd_get(&m->db, &db_cmd, chat_id, cmd_name);
	if (ret < 0)
		return 1;

	/* command  not found */
	if (ret == 0)
		return 0;

	log_debug("module: _external_handle_command: chat_id: %" PRIi64 ", name: %s, file: %s, "
		  "enable: %d, args: %d", chat_id, db_cmd.name, db_cmd.file, db_cmd.is_enable,
		  db_cmd.args);

	if (db_cmd.is_enable == 0)
		return 1;

	if ((db_cmd.is_admin) && (general_admin_check(m, msg) < 0))
		return 1;

	const char *const fname = str_set_fmt(&m->str, "%s/%s", m->cmd_path, db_cmd.file);
	if (fname == NULL)
		return 1;

	int count = 0;
	char chat_id_buf[16];
	char user_id_buf[16];
	const int args = db_cmd.args;
	if (args & DB_CMD_ARG_TYPE_CHAT_ID) {
		if (snprintf(chat_id_buf, LEN(chat_id_buf), "%" PRIi64, chat_id) < 0)
			return 1;

		argv[count++] = chat_id_buf;
	}

	if (args & DB_CMD_ARG_TYPE_USER_ID) {
		if (snprintf(user_id_buf, LEN(user_id_buf), "%" PRIi64, msg->from->id) < 0)
			return 1;

		argv[count++] = user_id_buf;
	}

	if (args & DB_CMD_ARG_TYPE_TEXT) {
		const char *const txt = msg->text.text;
		if (txt == NULL)
			return 1;

		argv[count++] = (char *)txt;
	}

	if (args & DB_CMD_ARG_TYPE_RAW)
		argv[count++] = (char *)json_object_to_json_string(json_obj);

	argv[count] = NULL;
	if (chld_spawn(m->chld, fname, argv) < 0)
		log_err(0, "module: _external_handle_command: chld_spawn: failed");

	return 1;
}
