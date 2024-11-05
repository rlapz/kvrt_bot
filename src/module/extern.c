#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#include <module.h>
#include <model.h>
#include <common.h>
#include <service.h>


static int _validate_args_len(int args, int max_len);
static int _spawn_child_process(ModuleExtern *m, Update *update, const TgMessage *msg, json_object *json);


/*
 * Public
 */
int
module_extern_exec(Update *update, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	char buffer[MODULE_EXTERN_NAME_SIZE];
	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);

	ModuleExtern module = {
		.chat_id = msg->chat.id,
		.name_ptr = buffer,
	};

	const int ret = service_module_extern_get(&update->service, &module);
	if (ret == 0)
		return 0;

	const int flags = module.flags;
	if ((flags & MODULE_FLAG_ADMIN_ONLY) && (common_privileges_check(update, msg) < 0))
		return 1;

	if (_validate_args_len(module.args, MODULE_EXTERN_ARGS_SIZE) == 0) {
		log_err(EINVAL, "module_extern: module_extern_exec: invalid args len");
		return -1;
	}

	if (_spawn_child_process(&module, update, msg, json) < 0)
		goto err0;

	return 1;

err0:
	common_send_text_plain(update, msg, "Failed to execute external command");
	return 1;
}


/*
 * Private
 */
/* TODO: optimize */
static int
_validate_args_len(int args, int max_len)
{
	int len = 0;
	if (args & MODULE_EXTERN_ARG_RAW)
		len++;
	if (args & MODULE_EXTERN_ARG_CHAT_ID)
		len++;
	if (args & MODULE_EXTERN_ARG_USER_ID)
		len++;
	if (args & MODULE_EXTERN_ARG_TEXT)
		len++;

	return (len < max_len);
}


static int
_spawn_child_process(ModuleExtern *m, Update *update, const TgMessage *msg, json_object *json)
{
	/* +2 = (executable file + NULL) */
	char *argv[MODULE_EXTERN_ARGS_SIZE + 2] = {
		[0] = m->file_name,
	};

	int i = 1;
	const int args = m->args;
	if (args & MODULE_EXTERN_ARG_RAW)
		argv[i++] = (char *)json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);

	char chat_id[24];
	if (args & MODULE_EXTERN_ARG_CHAT_ID) {
		snprintf(chat_id, LEN(chat_id), "%"PRIi64, msg->chat.id);
		argv[i++] = chat_id;
	}

	char user_id[24];
	if (args & MODULE_EXTERN_ARG_USER_ID) {
		snprintf(user_id, LEN(user_id), "%"PRIi64, msg->from->id);
		argv[i++] = user_id;
	}

	if (args & MODULE_EXTERN_ARG_TEXT)
		argv[i] = (char *)msg->text.cstr;

	if (chld_spawn(update->chld, m->file_name, argv) < 0) {
		log_err(errno, "module_extern: _spawn_child_process: chld_spawn: %s", m->file_name);
		return -1;
	}

	return 0;
}
