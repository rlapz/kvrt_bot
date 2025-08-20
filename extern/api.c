#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../src/config.h"
#include "../src/common.h"
#include "../src/model.h"
#include "../src/tg_api.h"
#include "../src/sqlite_pool.h"
#include "../src/util.h"


#define _TYPE_SEND_TEXT       "send_text"
#define _TYPE_SEND_PHOTO      "send_photo"
#define _TYPE_SEND_ANIMATION  "send_animation"
#define _TYPE_ANSWER_CALLBACK "answer_callback"
#define _TYPE_DELETE_MESSAGE  "delete_message"
#define _TYPE_SCHED_MESSAGE   "sched_message"
#define _TYPE_SESSION         "session"


/*
 * See: README.txt
 */
enum {
	_ARG_EXEC_NAME = 0,
	_ARG_CONFIG_FILE,
	_ARG_CMD_NAME,
	_ARG_API_TYPE,
	_ARG_DATA,
};


typedef struct arg {
	Config       config;
	const char  *cmd_name;
	const char  *api_type;
	const char  *tg_api;
	int64_t      bot_id;
	int64_t      owner_id;
	char         proc_name[4096];
	json_object *data;
	json_object *resp;
} Arg;

static int _arg_parse(Arg *a, int argc, char *argv[], json_object *resp_obj);
static int _add_response(const Arg *a, const char error[]);
static int _get_parent_proc(Arg *a);

static int _json_add_str(json_object *root, const char key[], const char value[]);

static int _send_text(const Arg *arg);
static int _send_photo(const Arg *arg);
static int _send_animation(const Arg *arg);
static int _delete_message(const Arg *arg);
static int _answer_callback(const Arg *arg);
static int _sched_message(const Arg *arg);
static int _session(const Arg *arg);


/*
 * Public
 */
int
main(int argc, char *argv[])
{
	int ret = 1;
	json_object *const resp_obj = json_object_new_object();
	if (resp_obj == NULL) {
		fprintf(stdout, "{ \"name\": \"%s\", \"error\": \"failed to create new json object\" }",
			"undefined");
		return 1;
	}

	Arg arg;
	if (_arg_parse(&arg, argc, argv, resp_obj) < 0)
		goto out0;

	tg_api_init(arg.tg_api);

	if (cstr_casecmp(arg.api_type, _TYPE_SEND_TEXT))
		ret = -_send_text(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_SEND_PHOTO))
		ret = -_send_photo(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_SEND_ANIMATION))
		ret = -_send_animation(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_DELETE_MESSAGE))
		ret = -_delete_message(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_ANSWER_CALLBACK))
		ret = -_answer_callback(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_SCHED_MESSAGE))
		ret = -_sched_message(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_SESSION))
		ret = -_session(&arg);
	else
		_add_response(&arg, "invalid api type!");

out0:
	fprintf(stdout, "%s\n\n", json_object_to_json_string_ext(resp_obj, JSON_C_TO_STRING_PRETTY));
	json_object_put(arg.data);
	json_object_put(resp_obj);
	return ret;
}


/*
 * Private
 */
static int
_arg_parse(Arg *a, int argc, char *argv[], json_object *resp_obj)
{
	int ret = -1;
	const char *error = "";

	a->data = NULL;
	a->api_type = "undefined";
	a->cmd_name = "undefined";
	if (_get_parent_proc(a) < 0) {
		error = "failed to get parent process name";
		cstr_copy_n(a->proc_name, LEN(a->proc_name), "undefined");
		goto out0;
	}

	if (argc != 5) {
		error = "invalid argument length!";
		goto out0;
	}

	const char *const cfg_file = argv[_ARG_CONFIG_FILE];
	if (cstr_is_empty(cfg_file)) {
		error = "'Config File' is empty";
		goto out0;
	}

	if (cstr_is_empty(argv[_ARG_CMD_NAME])) {
		error = "'CMD Name' is empty";
		goto out0;
	}

	a->cmd_name = argv[_ARG_CMD_NAME];
	if (config_load(&a->config, cfg_file) < 0) {
		error = "failed to load config file";
		goto out0;
	}

	if (cstr_is_empty(argv[_ARG_API_TYPE])) {
		error = "'API Type' is empty";
		goto out0;
	}

	a->api_type = argv[_ARG_API_TYPE];
	a->data = json_tokener_parse(argv[_ARG_DATA]);
	if (a->data == NULL) {
		error = "'Data': failed to parse";
		goto out0;
	}

	a->tg_api = a->config.api_url;
	a->bot_id = a->config.bot_id;
	a->owner_id = a->config.owner_id;
	ret = 0;

out0:
	a->resp = resp_obj;
	_add_response(a, error);
	return ret;
}


static int
_json_add_str(json_object *root, const char key[], const char value[])
{
	json_object *const str_obj = json_object_new_string(value);
	if (str_obj == NULL)
		return -1;

	if (json_object_object_add(root, key, str_obj) != 0) {
		json_object_put(str_obj);
		return -1;
	}

	return 0;
}


static int
_add_response(const Arg *a, const char error[])
{
	if (_json_add_str(a->resp, "type", a->api_type) < 0)
		return -1;
	if (_json_add_str(a->resp, "name", a->cmd_name) < 0)
		return -1;
	if (_json_add_str(a->resp, "proc", a->proc_name) < 0)
		return -1;

	return _json_add_str(a->resp, "error", error);
}


static int
_get_parent_proc(Arg *a)
{
	const pid_t ppid = getppid();
	char path[4096];
	int ret = snprintf(path, LEN(path), "/proc/%d/cmdline", ppid);
	if (ret < 0)
		return -1;


	path[ret] = '\0';
	const int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = -1;
	size_t total = 0;
	const size_t rsize = LEN(a->proc_name);
	char *const buffer = a->proc_name;
	while (total < rsize) {
		const ssize_t rd = read(fd, buffer + total, rsize - total);
		if (rd < 0)
			goto out0;

		if (rd == 0)
			break;

		total += (size_t)rd;
	}

	for (size_t i = 0; i < total; i++) {
		if (buffer[i] == '\0')
			buffer[i] = ' ';
	}

	buffer[total] = '\0';
	ret = 0;

out0:
	close(fd);
	return ret;
}


static int
_send_text(const Arg *arg)
{
	int ret = -1;
	int64_t ret_id = 0;
	const char  *error = "";

	json_object *type_obj;
	if (json_object_object_get_ex(arg->data, "type", &type_obj) == 0) {
		error = "no 'type' field";
		goto out0;
	}

	const char *const type_str = json_object_get_string(type_obj);

	int type;
	if (cstr_casecmp(type_str, "plain")) {
		type = TG_API_TEXT_TYPE_PLAIN;
	} else if (cstr_casecmp(type_str, "format")) {
		type = TG_API_TEXT_TYPE_FORMAT;
	} else {
		error = "'type': invalid value";
		goto out0;
	}

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		error = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		error = "no 'message_id' field";
		goto out0;
	}

	json_object *user_id_obj;
	if (json_object_object_get_ex(arg->data, "user_id", &user_id_obj) == 0) {
		error = "no 'user_id' field";
		goto out0;
	}

	const int64_t user_id = json_object_get_int64(user_id_obj);

	json_object *text_obj;
	if (json_object_object_get_ex(arg->data, "text", &text_obj) == 0) {
		error = "no 'text' field";
		goto out0;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		error = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);

	const char *const text = json_object_get_string(text_obj);
	if (cstr_is_empty(text)) {
		error = "'text': empty";
		goto out0;
	}

	char *const markup = new_deleter(user_id);
	const TgApiText api = {
		.type = type,
		.chat_id = chat_id,
		.msg_id = msg_id,
		.text = text,
		.markup = markup,
	};

	TgApiResp resp;
	ret = tg_api_text_send(&api, &resp);
	if (ret < 0)
		error = resp.error_msg;

	ret_id = resp.msg_id;
	free(markup);

out0:
	json_object_object_add(arg->resp, "message_id", json_object_new_int64(ret_id));
	_add_response(arg, error);
	return ret;
}


static int
_send_photo(const Arg *arg)
{
	int ret = -1;
	int64_t ret_id = 0;
	const char  *error = "";

	json_object *text_type_obj;
	if (json_object_object_get_ex(arg->data, "text_type", &text_type_obj) == 0) {
		error = "no 'text_type' field";
		goto out0;
	}

	const char *const text_type_str = json_object_get_string(text_type_obj);

	int text_type;
	if (cstr_casecmp(text_type_str, "plain")) {
		text_type = TG_API_TEXT_TYPE_PLAIN;
	} else if (cstr_casecmp(text_type_str, "format")) {
		text_type = TG_API_TEXT_TYPE_FORMAT;
	} else {
		error = "'text_type': invalid value";
		goto out0;
	}

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		error = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		error = "no 'message_id' field";
		goto out0;
	}

	json_object *user_id_obj;
	if (json_object_object_get_ex(arg->data, "user_id", &user_id_obj) == 0) {
		error = "no 'user_id' field";
		goto out0;
	}

	const int64_t user_id = json_object_get_int64(user_id_obj);

	json_object *photo_obj;
	if (json_object_object_get_ex(arg->data, "photo", &photo_obj) == 0) {
		error = "no 'photo' field";
		goto out0;
	}

	const char *text = "";
	json_object *text_obj;
	if (json_object_object_get_ex(arg->data, "text", &text_obj)) {
		text = json_object_get_string(text_obj);
		text = (cstr_is_empty(text))? "" : text;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		error = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);

	const char *const photo = json_object_get_string(photo_obj);
	if (cstr_is_empty(photo)) {
		error = "'photo': empty";
		goto out0;
	}

	char *const markup = new_deleter(user_id);
	const TgApiPhoto api = {
		.text_type = text_type,
		.chat_id = chat_id,
		.msg_id = msg_id,
		.photo = photo,
		.text = text,
		.markup = markup,
	};

	TgApiResp resp;
	ret = tg_api_photo_send(&api, &resp);
	if (ret < 0)
		error = resp.error_msg;

	ret_id = resp.msg_id;
	free(markup);

out0:
	json_object_object_add(arg->resp, "message_id", json_object_new_int64(ret_id));
	_add_response(arg, error);
	return ret;
}


static int
_send_animation(const Arg *arg)
{
	int ret = -1;
	int64_t ret_id = 0;
	const char  *error = "";

	json_object *text_type_obj;
	if (json_object_object_get_ex(arg->data, "text_type", &text_type_obj) == 0) {
		error = "no 'text_type' field";
		goto out0;
	}

	const char *const text_type_str = json_object_get_string(text_type_obj);

	int text_type;
	if (cstr_casecmp(text_type_str, "plain")) {
		text_type = TG_API_TEXT_TYPE_PLAIN;
	} else if (cstr_casecmp(text_type_str, "format")) {
		text_type = TG_API_TEXT_TYPE_FORMAT;
	} else {
		error = "'text_type': invalid value";
		goto out0;
	}

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		error = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		error = "no 'message_id' field";
		goto out0;
	}

	json_object *user_id_obj;
	if (json_object_object_get_ex(arg->data, "user_id", &user_id_obj) == 0) {
		error = "no 'user_id' field";
		goto out0;
	}

	const int64_t user_id = json_object_get_int64(user_id_obj);

	json_object *animation_obj;
	if (json_object_object_get_ex(arg->data, "animation", &animation_obj) == 0) {
		error = "no 'animation' field";
		goto out0;
	}

	const char *text = "";
	json_object *text_obj;
	if (json_object_object_get_ex(arg->data, "text", &text_obj)) {
		text = json_object_get_string(text_obj);
		text = (cstr_is_empty(text))? "" : text;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		error = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);

	const char *const animation = json_object_get_string(animation_obj);
	if (cstr_is_empty(animation)) {
		error = "'animation': empty";
		goto out0;
	}

	char *const markup = new_deleter(user_id);
	const TgApiAnimation api = {
		.text_type = text_type,
		.chat_id = chat_id,
		.msg_id = msg_id,
		.animation = animation,
		.text = text,
		.markup = markup,
	};

	TgApiResp resp;
	ret = tg_api_animation_send(&api, &resp);
	if (ret < 0)
		error = resp.error_msg;

	ret_id = resp.msg_id;
	free(markup);

out0:
	json_object_object_add(arg->resp, "message_id", json_object_new_int64(ret_id));
	_add_response(arg, error);
	return ret;
}


static int
_delete_message(const Arg *arg)
{
	int ret = -1;
	const char *error = "";

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		error = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		error = "no 'message_id' field";
		goto out0;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		error = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);
	if (msg_id == 0) {
		error = "'msg_id': invalid value";
		goto out0;
	}

	TgApiResp resp;
	ret = tg_api_delete(chat_id, msg_id, &resp);
	if (ret < 0)
		error = resp.error_msg;

out0:
	json_object_object_add(arg->resp, "name", json_object_new_string(arg->cmd_name));
	_add_response(arg, error);
	return ret;
}


static int
_answer_callback(const Arg *arg)
{
	int ret = -1;
	const char *error = "";

	json_object *id_obj;
	if (json_object_object_get_ex(arg->data, "id", &id_obj) == 0) {
		error = "no 'id' field";
		goto out0;
	}

	json_object *is_text_obj;
	if (json_object_object_get_ex(arg->data, "is_text", &is_text_obj) == 0) {
		error = "no 'is_text' field";
		goto out0;
	}

	json_object *value_obj;
	if (json_object_object_get_ex(arg->data, "value", &value_obj) == 0) {
		error = "no 'value' field";
		goto out0;
	}

	json_object *show_alert_obj;
	if (json_object_object_get_ex(arg->data, "show_alert", &show_alert_obj) == 0) {
		error = "no 'show_alert' field";
		goto out0;
	}

	const char *const id = json_object_get_string(id_obj);
	if (cstr_is_empty(id)) {
		error = "'id': empty";
		goto out0;
	}

	const int is_text = json_object_get_int(is_text_obj);

	const char *const value = json_object_get_string(value_obj);
	if (cstr_is_empty(value)) {
		error = "'value': empty";
		goto out0;
	}

	const int show_alert = json_object_get_int(show_alert_obj);
	if (is_text)
		ret = TG_API_CALLBACK_VALUE_TYPE_TEXT;
	else
		ret = TG_API_CALLBACK_VALUE_TYPE_URL;

	TgApiResp resp;
	const TgApiCallback api = {
		.value_type = TG_API_CALLBACK_VALUE_TYPE_TEXT,
		.show_alert = show_alert,
		.id = id,
		.value = value,
	};

	ret = tg_api_callback_answer(&api, &resp);
	if (ret < 0)
		error = resp.error_msg;

out0:
	_add_response(arg, error);
	return ret;
}


static int
_sched_message(const Arg *arg)
{
	int ret = -1;

	/* TODO */
	_add_response(arg, "not yet supported");
	return ret;
}


static int
_session(const Arg *arg)
{
	int ret = -1;
	const char *error = "";
	char *const db_file = getenv(CFG_ENV_DB_FILE);

	if (sqlite_pool_init(db_file, 1) < 0)
		return -1;

	json_object *type_obj;
	if (json_object_object_get_ex(arg->data, "type", &type_obj) == 0) {
		error = "no 'type' field";
		goto out0;
	}

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		error = "no 'chat_id' field";
		goto out0;
	}

	json_object *user_id_obj;
	if (json_object_object_get_ex(arg->data, "user_id", &user_id_obj) == 0) {
		error = "no 'user_id' field";
		goto out0;
	}

	json_object *ctx_obj;
	if (json_object_object_get_ex(arg->data, "ctx", &ctx_obj) == 0) {
		error = "no 'ctx' field";
		goto out0;
	}

	int type;
	const char *const type_str = json_object_get_string(type_obj);
	if (cstr_casecmp(type_str, "acquire")) {
		type = 0;
	} else if (cstr_casecmp(type_str, "release")) {
		type = 1;
	} else {
		error = "'type': invalid value";
		goto out0;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		error = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t user_id = json_object_get_int64(user_id_obj);
	if (user_id == 0) {
		error = "'user_id': invalid value";
		goto out0;
	}

	const char *const ctx_str = json_object_get_string(ctx_obj);
	if (cstr_is_empty(ctx_str)) {
		error = "'ctx': empty";
		goto out0;
	}

	switch (type) {
	case 0:
		ret = session_acquire(chat_id, user_id, ctx_str);
		if (ret == -1)
			error = "failed to acquire session lock";
		else if (ret == -2)
			error = "session is locked";
		break;
	case 1:
		ret = session_release(chat_id, user_id, ctx_str);
		break;
	}

out0:
	json_object_object_add(arg->resp, "name", json_object_new_string(arg->cmd_name));
	_add_response(arg, error);
	sqlite_pool_deinit();
	return ret;
}
