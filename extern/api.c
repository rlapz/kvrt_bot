#include <errno.h>
#include <stdint.h>
#include <stdarg.h>

#include "../src/config.h"
#include "../src/common.h"
#include "../src/model.h"
#include "../src/tg_api.h"
#include "../src/util.h"


#define _TYPE_SEND_TEXT       "send_text"
#define _TYPE_ANSWER_CALLBACK "answer_callback"
#define _TYPE_DELETE_MESSAGE  "delete_message"
#define _TYPE_SCHED_MESSAGE   "sched_message"


/*
 * See: README.txt
 */

#define _RESP_ADD_ERROR_FMT(R, ...) _json_add_str_fmt(R, "error", __VA_ARGS__)
#define _RESP_ADD_ERROR(R, V)       _json_add_str(R, "error", V)


enum {
	_ARG_EXEC_NAME = 0,
	_ARG_CONFIG_FILE,
	_ARG_API_TYPE,
	_ARG_DATA,
};


typedef struct arg {
	Config      *config;
	const char  *api_type;
	const char  *tg_api;
	int64_t      bot_id;
	int64_t      owner_id;
	json_object *data;
	json_object *resp;
} Arg;

static int _arg_parse(Arg *a, int argc, char *argv[], json_object *resp_obj);

static int _json_add_str(json_object *root, const char key[], const char value[]);
static int _json_add_str_fmt(json_object *root, const char key[], const char fmt[], ...);

static int _send_text(const Arg *arg);
static int _delete_message(const Arg *arg);
static int _answer_callback(const Arg *arg);
static int _sched_message(const Arg *arg);


/*
 * Public
 */
int
main(int argc, char *argv[])
{
	int ret = 1;
	json_object *const resp_obj = json_object_new_object();
	if (resp_obj == NULL) {
		fprintf(stdout, "{ \"error\": \"failed to create new json object\" }");
		return 1;
	}

	Arg arg;
	if (_arg_parse(&arg, argc, argv, resp_obj) < 0)
		goto out0;

	tg_api_init(arg.tg_api);

	if (cstr_casecmp(arg.api_type, _TYPE_SEND_TEXT))
		ret = -_send_text(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_DELETE_MESSAGE))
		ret = -_delete_message(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_ANSWER_CALLBACK))
		ret = -_answer_callback(&arg);
	else if (cstr_casecmp(arg.api_type, _TYPE_SCHED_MESSAGE))
		ret = -_sched_message(&arg);
	else
		_RESP_ADD_ERROR(resp_obj, "invalid api type!");

	config_free(&arg.config);

	(void)_json_add_str_fmt;

out0:
	fprintf(stdout, "%s\n", json_object_to_json_string(resp_obj));
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
	const char *resp = "";

	a->data = NULL;
	if (argc != 4) {
		resp = "invalid argument length!";
		goto out0;
	}

	const char *const cfg_file = argv[_ARG_CONFIG_FILE];
	if (cstr_is_empty(cfg_file)) {
		resp = "'Config File' is empty";
		goto out0;
	}

	if (config_load_from_json(cfg_file, &a->config) < 0) {
		resp = "filed to load config file";
		goto out0;
	}

	a->api_type = argv[_ARG_API_TYPE];
	if (cstr_is_empty(a->api_type)) {
		resp = "'API Type' is empty";
		goto out0;
	}

	a->data = json_tokener_parse(argv[_ARG_DATA]);
	if (a->data == NULL) {
		resp = "'Data': failed to parse";
		goto out0;
	}

	a->tg_api = a->config->api.url;
	a->bot_id = a->config->tg.bot_id;
	a->owner_id = a->config->tg.owner_id;
	ret = 0;

out0:
	if (ret < 0)
		config_free(&a->config);

	a->resp = resp_obj;
	_RESP_ADD_ERROR(resp_obj, resp);
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
_json_add_str_fmt(json_object *root, const char key[], const char fmt[], ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	if (ret < 0)
		return -1;

	const size_t new_str_len = (size_t)ret + 1;
	char *const new_str = malloc(new_str_len);
	if (new_str == NULL)
		return -1;

	va_start(va, fmt);
	ret = vsnprintf(new_str, new_str_len, fmt, va);
	va_end(va);

	if (ret < 0)
		goto out0;

	new_str[ret] = '\0';

	ret = -1;
	json_object *const str_obj = json_object_new_string(new_str);
	if (str_obj == NULL)
		goto out0;

	if (json_object_object_add(root, key, str_obj) != 0)
		json_object_put(str_obj);
	else
		ret = 0;

out0:
	free(new_str);
	return ret;
}


static int
_send_text(const Arg *arg)
{
	int ret = -1;
	int64_t ret_id = 0;
	const char  *resp = "";

	json_object *type_obj;
	if (json_object_object_get_ex(arg->data, "type", &type_obj) == 0) {
		resp = "no 'type' field";
		goto out0;
	}

	const char *const type_str = json_object_get_string(type_obj);

	int type;
	if (cstr_casecmp(type_str, "plain")) {
		type = TG_API_TEXT_TYPE_PLAIN;
	} else if (cstr_casecmp(type_str, "format")) {
		type = TG_API_TEXT_TYPE_FORMAT;
	} else {
		resp = "'type': invalid value";
		goto out0;
	}

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		resp = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		resp = "no 'message_id' field";
		goto out0;
	}

	json_object *text_obj;
	if (json_object_object_get_ex(arg->data, "text", &text_obj) == 0) {
		resp = "no 'text' field";
		goto out0;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		resp = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);

	const char *const text = (const char *)json_object_get_string(text_obj);
	if (cstr_is_empty(text)) {
		resp = "'text': empty";
		goto out0;
	}

	ret = tg_api_send_text(type, chat_id, msg_id, text, &ret_id);
	if (ret < 0)
		resp = "failed to send message";

out0:
	_RESP_ADD_ERROR(arg->resp, resp);
	json_object_object_add(arg->resp, "message_id", json_object_new_int64(ret_id));
	return ret;
}


static int
_delete_message(const Arg *arg)
{
	int ret = -1;
	const char *resp = "";

	json_object *chat_id_obj;
	if (json_object_object_get_ex(arg->data, "chat_id", &chat_id_obj) == 0) {
		resp = "no 'chat_id' field";
		goto out0;
	}

	json_object *msg_id_obj;
	if (json_object_object_get_ex(arg->data, "message_id", &msg_id_obj) == 0) {
		resp = "no 'message_id' field";
		goto out0;
	}

	const int64_t chat_id = json_object_get_int64(chat_id_obj);
	if (chat_id == 0) {
		resp = "'chat_id': invalid value";
		goto out0;
	}

	const int64_t msg_id = json_object_get_int64(msg_id_obj);
	if (msg_id == 0) {
		resp = "'msg_id': invalid value";
		goto out0;
	}

	ret = tg_api_delete_message(chat_id, msg_id);
	if (ret < 0)
		resp = "failed to delete message";

out0:
	_RESP_ADD_ERROR(arg->resp, resp);
	return ret;
}


static int
_answer_callback(const Arg *arg)
{
	int ret = -1;
	const char *resp = "";

	json_object *id_obj;
	if (json_object_object_get_ex(arg->data, "id", &id_obj) == 0) {
		resp = "no 'id' field";
		goto out0;
	}

	json_object *is_text_obj;
	if (json_object_object_get_ex(arg->data, "is_text", &is_text_obj) == 0) {
		resp = "no 'is_text' field";
		goto out0;
	}

	json_object *value_obj;
	if (json_object_object_get_ex(arg->data, "value", &value_obj) == 0) {
		resp = "no 'value' field";
		goto out0;
	}

	json_object *show_alert_obj;
	if (json_object_object_get_ex(arg->data, "show_alert", &show_alert_obj) == 0) {
		resp = "no 'show_alert' field";
		goto out0;
	}

	const char *const id = json_object_get_string(id_obj);
	if (cstr_is_empty(id)) {
		resp = "'id': empty";
		goto out0;
	}

	const int is_text = json_object_get_int(is_text_obj);

	const char *const value = json_object_get_string(value_obj);
	if (cstr_is_empty(value)) {
		resp = "'value': empty";
		goto out0;
	}

	const int show_alert = json_object_get_int(show_alert_obj);
	if (is_text)
		ret = tg_api_answer_callback_query(id, value, NULL, show_alert);
	else
		ret = tg_api_answer_callback_query(id, NULL, value, show_alert);

	if (ret < 0)
		resp = "failed to answer callback query";

out0:
	_RESP_ADD_ERROR(arg->resp, resp);
	return ret;
}


static int
_sched_message(const Arg *arg)
{
	int ret = -1;

	/* TODO */
	_RESP_ADD_ERROR(arg->resp, "not yet supported");
	return ret;
}
