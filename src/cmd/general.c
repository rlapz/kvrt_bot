#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../util.h"


static const char *const _icon_admin  = "🅰️";
static const char *const _icon_nsfw   = "🔞";
static const char *const _icon_extra  = "🎲";
static const char *const _icon_extern = "📦";


static char *_cmd_list_body(const ModelCmd list[], unsigned len, int flags,
			    int is_private_chat);


/*
 * Public
 */
/* TODO: parametrized mandatory commands */
void
cmd_general_start(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, NULL, "Hello! :3\nPlease click /help to show command list.");
}


void
cmd_general_help(const CmdParam *cmd)
{
	int is_err = 1;
	Pager pager = {
		.id_user = cmd->id_user,
		.id_owner = cmd->id_owner,
		.id_chat = cmd->id_chat,
		.id_message = cmd->msg->id,
		.id_callback = cmd->id_callback,
		.ctx = cmd->name,
		.title = "Command list:",
	};

	const char *res_text = "";
	if (pager_init(&pager, cmd->args) < 0) {
		res_text = "pager_init: failed";
		goto out0;
	}

	const int is_private_chat = (cmd->msg->chat.type == TG_CHAT_TYPE_PRIVATE)? 1 : 0;
	const int cflags = model_chat_get_flags(cmd->id_chat);
	if (cflags < 0) {
		if (cstr_is_empty(pager.id_callback) == 0) {
			res_text = "Not a callback!";
			goto out0;
		}

		res_text = "Failed to get chat flags!";
		return;
	}

	ModelCmd cmd_list[CFG_LIST_ITEMS_SIZE];
	PagerList list = { .page_num = pager.page };
	if (cmd_get_list(cmd_list, LEN(cmd_list), &list, cflags, is_private_chat) < 0) {
		res_text = "Failed to get 'cmd' list!";
		goto out0;
	}

	char *const body = _cmd_list_body(cmd_list, list.items_len, cflags, is_private_chat);
	if (body == NULL) {
		res_text = "Failed to allocate memory!";
		goto out0;
	}

	pager.body = body;
	is_err = pager_send(&pager, &list, NULL);
	free(body);

out0:
	if (is_err == 0)
		return;

	if (cstr_is_empty(pager.id_callback))
		send_error_text(cmd->msg, NULL, res_text);
	else
		answer_callback_text(pager.id_callback, res_text, 1);
}


void
cmd_general_dump(const CmdParam *cmd)
{
	const char *const json_str = json_object_to_json_string_ext(cmd->json, JSON_C_TO_STRING_PRETTY);
	char *const resp = CSTR_CONCAT("```json\n", json_str, "```");
	send_text_format(cmd->msg, NULL, "%s", resp);
	free(resp);
}


void
cmd_general_dump_admin(const CmdParam *cmd)
{
	const TgMessage *const msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		send_text_plain(msg, NULL, "There are no administrators in the private chat!");
		return;
	}

	TgChatAdminList list;
	TgApiResp resp = { .udata = &list };
	char buff[1024];

	const int ret = tg_api_get_admin_list(cmd->id_chat, &resp);
	if (ret == TG_API_RESP_ERR_API) {
		LOG_ERRN("general", "tg_api_get_admin_list: %s", tg_api_resp_str(&resp, buff, LEN(buff)));
		return;
	} else if (ret < 0) {
		LOG_ERRN("general", "tg_api_get_admin_list: errnum: %d", ret);
		return;
	}

	CmdParam _cmd = {
		.msg = msg,
		.json = list.tmp_obj,
	};

	cmd_general_dump(&_cmd);
	tg_chat_admin_list_free(&list);
}


void
cmd_general_schedule_message(const CmdParam *cmd)
{
	const TgMessage *const msg = cmd->msg;
	SpaceTokenizer st_deadline;
	const char *const next = space_tokenizer_next(&st_deadline, cmd->args);
	if (next == NULL)
		goto out0;

	SpaceTokenizer st_message;
	if (space_tokenizer_next(&st_message, next) == NULL)
		goto out0;

	char deadline[24];
	if (st_deadline.len >= LEN(deadline) + 1)
		goto out0;

	if (st_message.len == 0)
		goto out0;

	unsigned deadline_len = 0;
	for (unsigned i = 0; i < st_deadline.len; i++) {
		if (isalpha(st_deadline.value[i]))
			continue;

		deadline[deadline_len++] = st_deadline.value[i];
	}

	deadline[deadline_len] = '\0';
	if ((st_deadline.len - deadline_len) != 1)
		goto out0;

	int64_t deadline_tm = 0;
	if (cstr_to_int64(deadline, &deadline_tm) < 0)
		goto out0;

	int mul;
	const char suffix = st_deadline.value[deadline_len];
	const char *desc;
	switch (suffix) {
	case 's':
		mul = 1;
		desc = "second(s)";
		break;
	case 'm':
		mul = 60;
		desc = "minute(s)";
		break;
	case 'h':
		mul = 3600;
		desc = "hour(s)";
		break;
	case 'd':
		mul = 86400;
		desc = "day(s)";
		break;
	default:
		goto out0;
	}

	int64_t deadline_res;
	if (__builtin_mul_overflow(deadline_tm, mul, &deadline_res)) {
		send_text_plain(msg, NULL, "Overflow: Deadline value is too big!");
		return;
	}

	ModelSchedMessage sch = {
		.type = MODEL_SCHED_MESSAGE_TYPE_SEND,
		.chat_id = msg->chat.id,
		.message_id = msg->id,
		.value_in = st_message.value,
		.expire = 5,
	};

	if (model_sched_message_add(&sch, deadline_res) <= 0)
		send_text_plain(msg, NULL, "Failed to set sechedule message!");
	else
		send_text_format(msg, NULL, "Success! Scheduled in: %s %s", deadline, desc);

	return;

out0:
	send_text_format(msg, NULL,
		"%s [Deadline] [Message]\n"
		"Allowed Deadline suffixes: \n"
		"  s: second\n  m: minute\n  h: hour\n  d: day\n\n"
		"Example: \n"
		"  %s 10s Hello world!", cmd->name, cmd->name);
}


void
cmd_general_deleter(const CmdParam *cmd)
{
	const char *text;
	if (cmd->id_callback == NULL)
		return;

	int64_t from_id = 0;
	if (cstr_to_int64(cmd->args, &from_id) < 0) {
		text = "Invalid callback!";
		goto out0;
	}

	int can_delete = 1;
	if (cmd->id_user != from_id) {
		const int ret = is_admin(cmd->id_user, cmd->id_chat, cmd->id_owner);
		if (ret < 0) {
			text = "Failed to get admin list!";
			goto out0;
		}

		can_delete = ret;
	}

	text = "Deleted";
	if (can_delete == 0) {
		text = "Permission denied!";
		goto out0;
	}

	TgApiResp resp;
	char buff[1024];

	const int ret = tg_api_delete(cmd->id_chat, cmd->id_message, &resp);
	if (ret == TG_API_RESP_ERR_API)
		text = tg_api_resp_str(&resp, buff, LEN(buff));
	else if (ret < 0)
		text = "System error!";

out0:
	answer_callback_text(cmd->id_callback, text, 0);
}


/*
 * Private
 */
static char *
_cmd_list_body(const ModelCmd list[], unsigned len, int flags, int is_private_chat)
{
	Str str;
	if (str_init_alloc(&str, 2048) < 0)
		return NULL;

	const char *admin_only;
	const char *nsfw;
	const char *extra;
	const char *extern_;
	for (unsigned i = 0; i < len; i++) {
		const ModelCmd *const mc = &list[i];
		char *const name = tg_escape(mc->name);
		if (name == NULL)
			goto err0;

		char *const desc = tg_escape(mc->description);
		if (desc == NULL) {
			free(name);
			goto err0;
		}

		admin_only = "";
		if ((is_private_chat == 0) && (mc->flags & MODEL_CMD_FLAG_ADMIN))
			admin_only = _icon_admin;

		nsfw = (mc->flags & MODEL_CMD_FLAG_NSFW)? _icon_nsfw : "";
		extra = (mc->flags & MODEL_CMD_FLAG_EXTRA)? _icon_extra : "";
		extern_ = (mc->is_builtin == 0)? _icon_extern : "";
		str_append_fmt(&str, "%s \\- %s %s%s%s%s\n", name, desc, admin_only, nsfw, extra, extern_);
		free(desc);
		free(name);
	}

	str_append(&str, "\n```Legend:\n");

	const size_t old_len = str.len;
	if (is_private_chat == 0)
		str_append_fmt(&str, "%s: Admin, ", _icon_admin);
	if (flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA)
		str_append_fmt(&str, "%s: Extra, ", _icon_extra);
	if (flags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW)
		str_append_fmt(&str, "%s: NSFW, ", _icon_nsfw);
	if (flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN)
		str_append_fmt(&str, "%s: Extern, ", _icon_extern);

	if (str.len > old_len) {
		str_pop(&str, 2);
		str_append_n(&str, "```", 3);
	} else {
		str_append_n(&str, "None```", 7);
	}

	return str.cstr;

err0:
	str_deinit(&str);
	return NULL;
}
