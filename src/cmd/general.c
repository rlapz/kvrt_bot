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
cmd_general_start(const Cmd *cmd)
{
	ModelParam param;
	if (model_param_get(&param, "msg_start") <= 0) {
		send_text_format(cmd->msg, "Hello \\:3");
		return;
	}

	send_text_format(cmd->msg, param.value0_out);
}


void
cmd_general_help(const Cmd *cmd)
{
	int is_err = 1;
	const int is_callback = cmd->is_callback;
	const int cflags = model_chat_get_flags(cmd->msg->chat.id);
	if (cflags < 0) {
		if (is_callback)
			goto out0;

		send_text_plain(cmd->msg, "Falied to get chat flags");
		return;
	}

	unsigned page_num = 1;
	if (is_callback && (message_list_get_args(cmd->callback->id, cmd->query.args, &page_num, NULL) < 0))
		goto out0;

	unsigned start;
	CmdBuiltin *list = NULL;
	MessageListPagination req_page = { .page_count = page_num };
	if (cmd_builtin_get_list(&list, cflags, &start, &req_page) < 0)
		goto out0;

	char *const body = _builtin_list_body(list, start, req_page.items_count);
	if (body == NULL)
		goto out1;

	char ctx[MODEL_CMD_NAME_SIZE];
	cstr_copy_n2(ctx, LEN(ctx), cmd->bot_cmd.name, cmd->bot_cmd.name_len);

	MessageList mlist = {
		.ctx = ctx,
		.msg = cmd->msg,
		.title = "Builtin command list",
		.body = body,
		.is_edit = cmd->is_callback,
	};

	if (message_list_send(&mlist, &req_page, NULL) < 0)
		goto out2;

	is_err = 0;

out2:
	free(body);
out1:
	free(list);
out0:
	if (is_err && is_callback)
		tg_api_answer_callback_query(cmd->callback->id, "Error!", NULL, 1);
}


void
cmd_general_dump(const Cmd *cmd)
{
	const char *const json_str = json_object_to_json_string_ext(cmd->json, JSON_C_TO_STRING_PRETTY);
	char *const resp = CSTR_CONCAT("```json\n", json_str, "```");
	send_text_format(cmd->msg, resp);
	free(resp);
}


void
cmd_general_dump_admin(const Cmd *cmd)
{
	const TgMessage *msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		send_text_plain(msg, "There are no administrators in the private chat!");
		return;
	}

	json_object *json;
	if (tg_api_get_admin_list(msg->chat.id, NULL, &json) < 0) {
		send_text_plain(msg, "Failed to get admin list");
		return;
	}

	Cmd _cmd = {
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

	str_append_fmt(&str, "\n\\-\\> %s: Admin only, %s: Extra, %s: NSFW, %s: Extern \\<\\-\n",
		       _icon_admin, _icon_extra, _icon_nsfw, _icon_extern);
	return str.cstr;

err0:
	str_deinit(&str);
	return NULL;
}
