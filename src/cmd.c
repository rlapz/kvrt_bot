#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include "cmd.h"

#include "common.h"
#include "tg.h"
#include "util.h"


/*
 * CmdExtern child process argument list:
 * CMD:
 * 	0: Executable file
 * 	1: CMD Name
 * 	2: Exec type "cmd"
 * 	3: Chat ID
 * 	4: User ID
 * 	5: Message ID
 * 	6: Chat text
 * 	7: Raw JSON
 *
 * Callback:
 * 	0: Executable file
 * 	1: CMD Name
 * 	2: Exec type "callback"
 * 	3: Chat ID
 * 	4: User ID
 * 	5: Message ID
 * 	6: Callback ID
 * 	7: Data
 * 	8: Raw JSON
 */

#define _CMD_EXTERN_ARGS_SIZE (16)


static CmdBuiltin _cmd_builtin_list[] = {
	CMD_BUILTIN_LIST_GENERAL,
	CMD_BUILTIN_LIST_ADMIN,
	CMD_BUILTIN_LIST_EXTRA,
	CMD_BUILTIN_LIST_TEST,
};

static int  _register_builtin(void);
static int  _parse_cmd(CmdParam *param, const char req[]);
static int  _exec_builtin(const CmdParam *c, int chat_flags);
static int  _exec_extern(const CmdParam *c, int chat_flags);
static int  _exec_cmd_message(const CmdParam *c);
static int  _verify(const CmdParam *c, int chat_flags, int flags);
static int  _spawn_child_process(const CmdParam *c, const char file_name[]);


/*
 * Public
 */
int
cmd_init(void)
{
	log_info("cmd: cmd_init: _cmd_builtin_list size: %zu bytes", sizeof(_cmd_builtin_list));
	if (model_cmd_builtin_clear() < 0)
		return -1;

	return _register_builtin();
}


void
cmd_exec(CmdParam *cmd, const char req[])
{
	if (_parse_cmd(cmd, req) < 0)
		return;

	if (_exec_cmd_message(cmd))
		return;

	const int cflags = model_chat_get_flags(cmd->id_chat);
	if (cflags < 0) {
		send_text_plain(cmd->msg, "Failed to get chat flags!");
		return;
	}

	if (_exec_builtin(cmd, cflags))
		return;

	if (_exec_extern(cmd, cflags))
		return;

	if ((cmd->msg->chat.type != TG_CHAT_TYPE_PRIVATE) && (cmd->has_username == 0))
		return;

	send_text_plain(cmd->msg, "Invalid command!");
}


int
cmd_get_list(ModelCmd list[], int len, MessageListPagination *pag, int flags, int is_private)
{
	assert(VAL_IS_NULL_OR(list, pag) == 0);
	if (pag->page_num == 0)
		return -1;

	int total;
	const int offt = (pag->page_num - 1) * len;
	const int llen = model_cmd_get_list(list, len, offt, &total, flags, is_private);
	if ((llen < 0) || (total <= 0))
		return -1;

	message_list_pagination_set(pag, pag->page_num, len, llen, total);
	return 0;
}


/*
 * Private
 */
static int
_register_builtin(void)
{
	int count = 0;
	for (int i = 0; i < (int)LEN(_cmd_builtin_list); i++) {
		const CmdBuiltin *const p = &_cmd_builtin_list[i];
		if (CSTR_IS_EMPTY_OR(p->name, p->description) || (p->callback_fn == NULL))
			continue;

		if (strlen(p->name) >= MODEL_CMD_NAME_SIZE) {
			log_err(0, "cmd: _register_builtin: '%s': too long! Max: %u", p->name, MODEL_CMD_NAME_SIZE);
			return -1;
		}

		if (strlen(p->description) >= MODEL_CMD_DESC_SIZE) {
			log_err(0, "cmd: _register_builtin: '%s': too long! Max: %u", p->description, MODEL_CMD_DESC_SIZE);
			return -1;
		}

		if (model_cmd_builtin_is_exists(p->name)) {
			log_err(0, "cmd: _register_builtin: '%s': already registered", p->name);
			return -1;
		}

		if (model_cmd_message_is_exists(p->name)) {
			log_err(0, "cmd: _register_builtin: '%s': already registered as CMD Message", p->name);
			return -1;
		}

		if (model_cmd_extern_is_exists(p->name)) {
			log_err(0, "cmd: _register_builtin: '%s': already registered as Extern CMD", p->name);
			return -1;
		}

		const ModelCmdBuiltin cmd = {
			.idx = i,
			.flags = p->flags,
			.name_in = p->name,
			.description_in = p->description,
		};

		if (model_cmd_builtin_add(&cmd) < 0)
			return -1;

		log_info("cmd: _register_builtin: %d. %s: %s", count, p->name, p->description);
		count++;
	}

	log_info("cmd: _register_builtin: registered %d builtin cmd(s)", count);
	return 0;
}


static int
_parse_cmd(CmdParam *c, const char req[])
{
	SpaceTokenizer st;
	const char *const next = space_tokenizer_next(&st, req);
	unsigned name_len = st.len;

	// callback
	if (c->id_callback != NULL)
		goto out0;

	/* verify username & skip username */
	const char *const _uname = strchr(st.value, '@');
	const char *const _uname_end = st.value + st.len;
	if ((_uname != NULL) && (_uname < _uname_end)) {
		if (cstr_casecmp_n(c->bot_username, _uname, (size_t)(_uname_end - _uname)) == 0)
			return -1;

		name_len = (unsigned)(_uname - st.value);
		c->has_username = 1;
	} else {
		c->has_username = 0;
	};

out0:
	cstr_copy_lower_n2(c->name, LEN(c->name), st.value, name_len);
	c->args = next;
	return 0;
}


static int
_exec_builtin(const CmdParam *c, int chat_flags)
{
	const int index = model_cmd_builtin_get_index(c->name);
	if (index == -2)
		return 0;

	if (is_valid_index(index, LEN(_cmd_builtin_list)) == 0) {
		send_text_plain(c->msg, "Failed to get builtin command data!");
		return 1;
	}

	const CmdBuiltin *const handler = &_cmd_builtin_list[index];
	if (_verify(c, chat_flags, handler->flags) == 0)
		return 1;

	log_info("cmd: _exec_builtin: [%" PRIi64 ":%" PRIi64 ":%" PRIi64 "]: %s: %p",
		 c->id_chat, c->id_user, c->id_message, handler->name, handler->callback_fn);

	handler->callback_fn(c);
	return 1;
}


static int
_exec_extern(const CmdParam *c, int chat_flags)
{
	ModelCmdExtern ce;
	const int ret = model_cmd_extern_get(&ce, c->name);
	if (ret < 0) {
		send_text_plain(c->msg, "Failed to get external command data!");
		return 1;
	}

	if (ret == 0)
		return 0;

	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN) == 0)
		return 1;

	if (_verify(c, chat_flags, ce.flags) == 0)
		return 1;

	log_info("cmd: _exec_extern: [%" PRIi64 ":%" PRIi64 ":%" PRIi64 "]: %s: %s",
		 c->id_chat, c->id_user, c->id_message, ce.name, ce.file_name);

	if (_spawn_child_process(c, ce.file_name) < 0) {
		send_text_plain(c->msg, "Failed to execute external command!");
		return 1;
	}

	return 1;
}


static int
_exec_cmd_message(const CmdParam *c)
{
	if (c->id_callback != NULL)
		return 0;

	char value[MODEL_CMD_MESSAGE_VALUE_SIZE];
	const int ret = model_cmd_message_get_value(c->id_chat, c->name, value, LEN(value));
	if (ret < 0) {
		send_text_plain(c->msg, "Failed to get command message data!");
		return 1;
	}

	if (ret == 0)
		return 0;

	log_info("cmd: _exec_cmd_message: [%" PRIi64 ":%" PRIi64 ":%" PRIi64 "]: %s",
		 c->id_chat, c->id_user, c->msg->id, c->name);

	send_text_format(c->msg, value);
	return 1;
}


static int
_verify(const CmdParam *c, int chat_flags, int flags)
{
	const int is_private = (c->msg->chat.type == TG_CHAT_TYPE_PRIVATE);
	if ((is_private) && (flags & MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT))
		return 0;

	if ((c->id_callback != NULL) && ((flags & MODEL_CMD_FLAG_CALLBACK) == 0))
		return 0;

	if ((flags & MODEL_CMD_FLAG_NSFW) && ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) == 0))
		return 0;

	if ((flags & MODEL_CMD_FLAG_EXTRA) && ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA) == 0))
		return 0;

	if (is_private == 0) {
		if ((flags & MODEL_CMD_FLAG_ADMIN) && (is_admin(c->id_user, c->id_chat, c->id_owner) == 0)) {
			send_text_plain(c->msg, "Permission denied!");
			return 0;
		}
	}

	return 1;
}


static int
_spawn_child_process(const CmdParam *c, const char file_name[])
{
	const int is_callback = (c->id_callback != NULL);

	/* +4 = (executable file + command name + flag(CMD/CALLBACK) + NULL) */
	char *argv[_CMD_EXTERN_ARGS_SIZE + 4] = {
		[0] = (char *)file_name,
		[1] = (char *)c->name,
		[2] = (is_callback)? "callback" : "cmd",
	};

	int i = 3;

	char chat_id[24];
	snprintf(chat_id, LEN(chat_id), "%" PRIi64, c->id_chat);
	argv[i++] = chat_id;

	char user_id[24];
	snprintf(user_id, LEN(user_id), "%" PRIi64, c->id_chat);
	argv[i++] = user_id;

	char message_id[24];
	snprintf(message_id, LEN(message_id), "%" PRIi64, c->id_message);
	argv[i++] = message_id;

	if (is_callback) {
		argv[i++] = (char *)c->id_callback;
		argv[i++] = (char *)c->args;
	} else {
		argv[i++] = (char *)c->msg->text.cstr;
	}

	argv[i] = (char *)json_object_to_json_string(c->json);

	return chld_spawn(file_name, argv);
}
