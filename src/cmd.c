#include <assert.h>
#include <errno.h>
#include <string.h>

#include "cmd.h"

#include "common.h"
#include "tg.h"
#include "util.h"


/*
 * CmdExtern child process argument list:
 * 	CMD:
 * 		0: Executable file
 * 		1: Exec type "cmd"
 * 		2: Chat ID
 * 		3: User ID
 * 		4: Message ID
 * 		5: Chat text
 * 		6: Raw JSON
 * 	Callback:
 * 		0: Executable file
 * 		1: Exec type "callback"
 * 		2: Callback ID
 * 		3: Chat ID
 * 		4: User ID
 *              5: Message ID
 * 		6-n: User data
 */

#define _CMD_EXTERN_ARGS_SIZE (16)


static CmdBuiltin _cmd_builtin_list[] = {
	CMD_BUILTIN_LIST_GENERAL,
	CMD_BUILTIN_LIST_ADMIN,
	CMD_BUILTIN_LIST_TEST,
};

static int      _cmd_builtin_list_len = LEN(_cmd_builtin_list);
static CstrMap  _map;

static int  _register_builtin(CstrMap *m);
static void _remove_builtin(int *iter);
static int  _exec_builtin(const Cmd *c, const char name[], int chat_flags);
static int  _exec_extern(const Cmd *c, const char name[], int chat_flags);
static int  _exec_cmd_message(const Cmd *c, const char name[]);
static int  _verify(const Cmd *c, int chat_flags, int flags);
static int  _spawn_child_process(const Cmd *c, const char file_name[]);


/*
 * Public
 */
int
cmd_init(void)
{
	if (cstr_map_init(&_map, CFG_CMD_BUILTIN_MAP_SIZE) < 0) {
		log_err(errno, "cmd: cmd_init: cstr_map_init");
		return -1;
	}

	log_debug("cmd: cmd_init: cstr_map: map size: %zu", _map.size);
	if (_register_builtin(&_map) < 0) {
		cstr_map_deinit(&_map);
		return -1;
	}

	return 0;
}


void
cmd_deinit(void)
{
	cstr_map_deinit(&_map);
}


void
cmd_exec(const Cmd *c)
{
	char buff[1024];
	const size_t len = cstr_copy_n2(buff, LEN(buff), c->bot_cmd.name, c->bot_cmd.name_len);

	const char *const name = cstr_to_lower_n(buff, len);
	if (_exec_cmd_message(c, name))
		return;

	const int cflags = model_chat_get_flags(c->msg->chat.id);
	if (cflags < 0) {
		send_text_plain(c->msg, "Falied to get chat flags");
		return;
	}

	if (_exec_builtin(c, name, cflags))
		return;

	if (_exec_extern(c, name, cflags))
		return;

	if ((c->msg->chat.type != TG_CHAT_TYPE_PRIVATE) && (c->bot_cmd.has_username == 0))
		return;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return;

	const char *const rep = str_set_fmt(&str, "\"%s\": Invalid command!", name);
	send_text_plain(c->msg, (rep != NULL) ? rep : "Invalid command!");
	str_deinit(&str);
}


int
cmd_builtin_get_list(const CmdBuiltin *list[])
{
	*list = _cmd_builtin_list;
	return _cmd_builtin_list_len;
}


int
cmd_builtin_is_exists(const char name[])
{
	if (cstr_map_get(&_map, name) != NULL)
		return 1;

	return 0;
}


/*
 * Private
 */
static int
_register_builtin(CstrMap *m)
{
	for (int i = 0; i < _cmd_builtin_list_len; i++) {
		const CmdBuiltin *const p = &_cmd_builtin_list[i];
		if (cstr_is_empty(p->name) || (p->callback_fn == NULL))
			goto rem0;

		if (strlen(p->name) >= MODEL_CMD_NAME_SIZE) {
			log_err(0, "cmd: _register_builtin: \"%s\": too long! Max: %u", p->name, MODEL_CMD_NAME_SIZE);
			goto rem0;
		}

		if (strlen(p->description) >= MODEL_CMD_DESC_SIZE) {
			log_err(0, "cmd: _register_builtin: \"%s\": too long! Max: %u", p->description, MODEL_CMD_DESC_SIZE);
			goto rem0;
		}

		if (cstr_map_get(m, p->name) != NULL) {
			log_err(0, "cmd: _register_builtin: cstr_map_get: \"%s\": already registered", p->name);
			return -1;
		}

		if (model_cmd_message_is_exists(p->name)) {
			log_err(0, "cmd: _register_builtin: cstr_map_get: \"%s\": already registered as CMD Message", p->name);
			return -1;
		}

		if (model_cmd_extern_is_exists(p->name)) {
			log_err(0, "cmd: _register_builtin: cstr_map_get: \"%s\": already registered as Extern CMD", p->name);
			return -1;
		}

		if (cstr_map_set(m, p->name, (void *)p) < 0) {
			log_err(errno, "cmd: _register_builtin: cstr_map_set: \"%s\": failed", p->name);
			goto rem0;
		}

#ifdef DEBUG
		log_info("cmd: _register_builtin: %s: %p: OK", p->name, p->callback_fn);
#else
		log_info("cmd: _register_builtin: %s: OK", p->name);
#endif
		continue;

rem0:
		_remove_builtin(&i);
	}

	log_info("cmd: _register_builtin: registered %zu builtin cmd(s)", _cmd_builtin_list_len);
	return 0;
}


static void
_remove_builtin(int *iter)
{
	int i = *iter;
	const int len = (_cmd_builtin_list_len) - 1;
	_cmd_builtin_list[i] = _cmd_builtin_list[len];
	memset(&_cmd_builtin_list[len], 0, sizeof(CmdBuiltin));

	_cmd_builtin_list_len = len;
	*iter = (i - 1);
}


static int
_exec_builtin(const Cmd *c, const char name[], int chat_flags)
{
	const CmdBuiltin *const handler = cstr_map_get(&_map, name);
	if (handler == NULL)
		return 0;

	if (_verify(c, chat_flags, handler->flags) == 0)
		return 1;

	const int64_t from_id = (c->msg->from)? c->msg->from->id : -1;
	log_info("cmd: _exec_builtin: [%" PRIi64 ":%" PRIi64 ":%" PRIi64 "]: %s: %p",
		 c->msg->chat.id, from_id, c->msg->id, handler->name, handler->callback_fn);
	handler->callback_fn(c);
	return 1;
}


static int
_exec_extern(const Cmd *c, const char name[], int chat_flags)
{
	ModelCmdExtern ce;
	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN) == 0)
		return 1;

	const int ret = model_cmd_extern_get_one(&ce, c->msg->chat.id, name);
	if (ret < 0)
		return 1;

	if (ret == 0)
		return 0;

	if (_verify(c, chat_flags, ce.flags) == 0)
		return 1;

	if (_spawn_child_process(c, ce.file_name_out) < 0) {
		send_text_plain(c->msg, "Failed to execute external command");
		return 1;
	}

	return 1;
}


static int
_exec_cmd_message(const Cmd *c, const char name[])
{
	char value[MODEL_CMD_MESSAGE_VALUE_SIZE];
	const int ret = model_cmd_message_get_value(c->msg->chat.id, name, value, LEN(value));
	if (ret < 0)
		return 1;

	if (ret == 0)
		return 0;

	const int64_t from_id = (c->msg->from)? c->msg->from->id : -1;
	log_info("cmd: _exec_cmd_message: [%" PRIi64 ":%" PRIi64 ":%" PRIi64 "]: %s",
		 c->msg->chat.id, from_id, c->msg->id, name);

	send_text_format(c->msg, value);
	return 1;
}


static int
_verify(const Cmd *c, int chat_flags, int flags)
{
	if (c->is_callback && ((flags & MODEL_CMD_FLAG_CALLBACK) == 0))
		return 0;

	if ((flags & MODEL_CMD_FLAG_NSFW) && ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) == 0))
		return 0;

	if ((flags & MODEL_CMD_FLAG_ADMIN) && (privileges_check(c->msg, c->id_owner) == 0))
		return 0;

	return 1;
}


static int
_spawn_child_process(const Cmd *c, const char file_name[])
{
	/* +3 = (executable file + flag(CMD/CALLBACK) + NULL) */
	char *argv[_CMD_EXTERN_ARGS_SIZE + 3] = {
		[0] = (char *)file_name,
		[1] = (c->is_callback)? "callback" : "cmd",
	};

	int i = 2;
	if (c->is_callback)
		argv[i++] = (char *)c->callback->id;

	char chat_id[24];
	snprintf(chat_id, LEN(chat_id), "%" PRIi64, c->msg->chat.id);
	argv[i++] = chat_id;

	char user_id[24];
	snprintf(user_id, LEN(user_id), "%" PRIi64, c->msg->from->id);
	argv[i++] = user_id;

	char message_id[24];
	snprintf(message_id, LEN(message_id), "%" PRIi64, c->msg->id);
	argv[i++] = message_id;

	if (c->is_callback) {
		argv[i] = (char *)c->callback->data;
	} else {
		argv[i++] = (char *)c->msg->text.cstr;
		argv[i] = (char *)json_object_to_json_string(c->json);
	}

	return chld_spawn(file_name, argv);
}
