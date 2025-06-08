#include <assert.h>
#include <math.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../util.h"


static const char *const _icon_admin  = "🅰️";
static const char *const _icon_nsfw   = "🔞";
static const char *const _icon_extra  = "🎲";
static const char *const _icon_extern = "📦";


static char *_builtin_list_body(const CmdBuiltin list[], unsigned num, unsigned len);


/*
 * Public
 */
void
cmd_general_start(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, "hello");
}


/*
 * TODO: merge builtin and external command list
 */
void
cmd_general_help(const CmdParam *cmd)
{
	int is_err = 1;
	MessageList list = {
		.id_user = cmd->id_user,
		.id_owner = cmd->id_owner,
		.id_chat = cmd->id_chat,
		.id_message = cmd->msg->id,
		.id_callback = cmd->id_callback,
		.ctx = cmd->name,
		.title = "Command list",
	};

	if (message_list_init(&list, cmd->args) < 0)
		return;

	const int cflags = model_chat_get_flags(cmd->id_chat);
	if (cflags < 0) {
		if (cstr_is_empty(list.id_callback) == 0)
			goto out0;

		send_text_plain(cmd->msg, "Falied to get chat flags");
		return;
	}

	unsigned start;
	CmdBuiltin *builtins = NULL;
	MessageListPagination pagination = { .page_count = list.page };
	if (cmd_builtin_get_list(&builtins, cflags, &start, &pagination) < 0)
		goto out0;

	list.body = _builtin_list_body(builtins, start, pagination.items_count);
	is_err = message_list_send(&list, &pagination, NULL);

	free((char *)list.body);

out0:
	if (is_err == 0)
		return;

	tg_api_answer_callback_query(list.id_callback, "Error!", NULL, 1);
}


void
cmd_general_dump(const CmdParam *cmd)
{
	const char *const json_str = json_object_to_json_string_ext(cmd->json, JSON_C_TO_STRING_PRETTY);
	char *const resp = CSTR_CONCAT("```json\n", json_str, "```");
	send_text_format(cmd->msg, resp);
	free(resp);
}


void
cmd_general_dump_admin(const CmdParam *cmd)
{
	const TgMessage *msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		send_text_plain(msg, "There are no administrators in the private chat!");
		return;
	}

	json_object *json;
	if (tg_api_get_admin_list(cmd->id_chat, NULL, &json) < 0) {
		send_text_plain(msg, "Failed to get admin list");
		return;
	}

	CmdParam _cmd = {
		.msg = msg,
		.json = json,
	};

	cmd_general_dump(&_cmd);
	json_object_put(json);
}


/*
 * Private
 */
static char *
_builtin_list_body(const CmdBuiltin list[], unsigned num, unsigned len)
{
	const char *admin_only;
	const char *nsfw;
	const char *extra;
	const char *res;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	for (unsigned i = num; i < len; i++) {
		const CmdBuiltin *const cb = &list[i];
		char *const name = tg_escape(cb->name);
		if (name == NULL)
			goto err0;

		char *const desc = tg_escape(cb->description);
		if (desc == NULL) {
			free(name);
			goto err0;
		}

		admin_only = (cb->flags & MODEL_CMD_FLAG_ADMIN)? _icon_admin : "";
		nsfw = (cb->flags & MODEL_CMD_FLAG_NSFW)? _icon_nsfw : "";
		extra = (cb->flags & MODEL_CMD_FLAG_EXTRA)? _icon_extra : "";
		res = str_append_fmt(&str, "%u\\. %s \\- %s %s%s%s\n", i + 1,
				     name, desc, admin_only, nsfw, extra);

		free(desc);
		free(name);
		if (res == NULL)
			goto err0;
	}

	str_append_fmt(&str, "\n```Legend:\n%s: Admin, %s: Extra, %s: NSFW, %s: Extern```",
		       _icon_admin, _icon_extra, _icon_nsfw, _icon_extern);
	return str.cstr;

err0:
	str_deinit(&str);
	return NULL;
}
