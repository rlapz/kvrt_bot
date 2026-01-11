#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../sched.h"
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
	Pager pager = {
		.id_user = cmd->id_user,
		.id_owner = cmd->id_owner,
		.id_chat = cmd->id_chat,
		.id_message = cmd->msg->id,
		.id_callback = cmd->id_callback,
		.ctx = cmd->name,
		.title = "Command list:",
	};

	if (pager_init(&pager, cmd->args) < 0)
		return;

	const int is_private_chat = (cmd->msg->chat.type == TG_CHAT_TYPE_PRIVATE)? 1 : 0;
	const int cflags = model_chat_get_flags(cmd->id_chat);
	if (cflags < 0) {
		if (cstr_is_empty(pager.id_callback) == 0) {
			SEND_ERROR_TEXT(cmd->msg, NULL, "%s", "Not a callback!");
			return;
		}

		SEND_ERROR_TEXT(cmd->msg, NULL, "%s", "Failed to get chat flags!");
		return;
	}

	ModelCmd cmd_list[CFG_LIST_ITEMS_SIZE];
	PagerList list = { .page_num = pager.page };
	if (cmd_get_list(cmd_list, LEN(cmd_list), &list, cflags, is_private_chat) < 0) {
		SEND_ERROR_TEXT(cmd->msg, NULL, "%s", "Failed to get 'cmd' list!");
		return;
	}

	char *const body = _cmd_list_body(cmd_list, list.items_len, cflags, is_private_chat);
	if (body == NULL) {
		SEND_ERROR_TEXT(cmd->msg, NULL, "%s", "Failed to allocate memory!");
		return;
	}

	pager.body = body;
	pager_send(&pager, &list, NULL);
	free(body);
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
cmd_general_list_admin(const CmdParam *cmd)
{
	const TgMessage *const msg = cmd->msg;
	TgChatAdminList list;
	TgApiResp resp;

	const int ret = tg_api_get_admin_list(&list, cmd->id_chat, &resp);
	if (ret < 0) {
		SEND_ERROR_TEXT(msg, NULL, "tg_api_get_admin_list: %s", resp.error_msg);
		return;
	}

	Str str;
	if (str_init_alloc(&str, 1024, "%s", "Admin list:\n`") < 0) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "str_init_alloc: Failed to allocate memory!");
		goto out0;
	}

	for (unsigned i = 0; i < list.len; i++) {
		const TgChatAdmin *const adm = &list.list[i];
		str_append_fmt(&str, "%d. %s %s\n", i + 1, adm->user->first_name,
			       cstr_empty_if_null(adm->user->last_name));
		str_append_fmt(&str, "    Id        : %" PRIi64 "\n", adm->user->id);
		str_append_fmt(&str, "    First name: %s\n", adm->user->first_name);

		if (adm->user->username != NULL)
			str_append_fmt(&str, "    Username  : @%s\n", adm->user->username);

		str_append_fmt(&str, "    Is bot    : %s\n", bool_to_cstr(adm->user->is_bot));
		str_append_fmt(&str, "    Is premium: %s\n", bool_to_cstr(adm->user->is_premium));
		str_append_fmt(&str, "    Is anon   : %s\n", bool_to_cstr(adm->is_anonymous));
		if (adm->custom_title != NULL)
			str_append_fmt(&str, "    Title     : %s\n\n", adm->custom_title);
		else
			str_append_c(&str, '\n');
	}

	str_append(&str, "`");
	send_text_format(msg, NULL, "%s", str.cstr);
	str_deinit(&str);

out0:
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
		SEND_ERROR_TEXT(msg, NULL, "%s", "Overflow: Deadline value is too big!");
		return;
	}

	const SchedParam sch = {
		.type = SCHED_MESSAGE_TYPE_SEND,
		.chat_id = msg->chat.id,
		.message_id = msg->id,
		.user_id = msg->from->id,
		.message = st_message.value,
		.expire = 5,
		.interval = deadline_res,
	};

	if (sched_add(&sch) <= 0)
		SEND_ERROR_TEXT(msg, NULL, "%s", "Failed to set sechedule message!");
	else
		send_text_plain(msg, NULL, "Success! Scheduled in: %s %s", deadline, desc);

	return;

out0:
	send_text_plain(msg, NULL,
		"%s [Deadline] [Message]\n"
		"Allowed Deadline suffixes: \n"
		"  s: second\n  m: minute\n  h: hour\n  d: day\n\n"
		"Example: \n"
		"  %s 10s Hello world!", cmd->name, cmd->name);
}


void
cmd_general_report(const CmdParam *cmd)
{
	const TgMessage *const msg = cmd->msg;
	if (msg->reply_to == NULL) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "Please reply a message.");
		return;
	}

	SpaceTokenizer st;
	const char *const next = space_tokenizer_next(&st, cmd->args);
	if (next == NULL)
		st.value = "General";

	ModelAdmin list[8];
	int ret = model_admin_get_list(list, (int)LEN(list), msg->chat.id);
	if (ret < 0) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "Failed to get admin list.");
		return;
	}

	if (ret == 0) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "Please reload the admin list.");
		return;
	}

	Str str;
	char *const reason = tg_escape(st.value);
	if (reason == NULL) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "Internal error!");
		return;
	}

	if (str_init_alloc(&str, 1024, "Report: \"%s\"\n", reason) < 0) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "Internal error!");
		free(reason);
		return;
	}

	free(reason);

	int j = 1;
	for (int i = 0; i < ret; i++) {
		const ModelAdmin *const adm = &list[i];
		if (adm->is_bot)
			continue;

		char *const fname = tg_escape(adm->first_name);
		if (fname == NULL)
			continue;

		str_append_fmt(&str, "%d\\. [%s](tg://user?id=%" PRIi64 ")\n", j, fname, adm->user_id);

		free(fname);
		j++;
	}

	str_pop(&str, 1);

	{
		char *const fname = tg_escape(msg->from->first_name);
		str_append_fmt(&str, "\n\n\\-\\-\\-\nReported by: [%s](tg://user?id=%" PRIi64 ")",
			       fname, msg->from->id);
		free(fname);
	}


	if ((msg->reply_to->from == NULL) || (msg->reply_to->from->is_bot == 0)) {
		send_text_format(&(TgMessage){
			.chat = (TgChat) { .id = msg->chat.id },
			.id = msg->reply_to->id,
			.from = &(TgUser) { .id = msg->from->id },
		}, NULL, "%s", str.cstr);
	} else {
		send_text_format(msg, NULL, "%s", str.cstr);
	}

	str_deinit(&str);
}


void
cmd_general_deleter(const CmdParam *cmd)
{
	if (cmd->id_callback == NULL)
		return;

	int64_t from_id = 0;
	if (cstr_to_int64(cmd->args, &from_id) < 0) {
		answer_callback_text(cmd->id_callback, "Invalid callback!", 0);
		return;
	}

	int can_delete = 1;
	if (cmd->id_user != from_id) {
		const int ret = is_admin(cmd->id_user, cmd->id_chat, cmd->id_owner);
		if (ret < 0) {
			answer_callback_text(cmd->id_callback, "Failed to get admin list!", 0);
			return;
		}

		can_delete = ret;
	}

	if (can_delete == 0) {
		answer_callback_text(cmd->id_callback, "Permission denied!", 0);
		return;
	}

	TgApiResp resp;
	const int ret = tg_api_delete(cmd->id_chat, cmd->id_message, &resp);
	if (ret < 0) {
		LOG_ERRN("common", "tg_api_delete: %s", resp.error_msg);
		answer_callback_text(cmd->id_callback, resp.error_msg, 1);
		return;
	}

	answer_callback_text(cmd->id_callback, "Deleted!", 0);
}


/*
 * Private
 */
static char *
_cmd_list_body(const ModelCmd list[], unsigned len, int flags, int is_private_chat)
{
	Str str;
	if (str_init_alloc(&str, 2048, NULL) < 0)
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
