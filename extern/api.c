#include <errno.h>

#include "api.h"

#include "../src/tg_api.h"
#include "../src/util.h"


/* Mandatory argument list:
 *	arg[1]: CMD Name
 * 	arg[2]: API_TYPE_*
 * 	arg[3]: TELEGRAM API + TOKEN
 * 	arg[4]: TELEGRAM SECRET KEY
 *
 * -----------------------------------------------------------------
 * API_TYPE_TEXT_* argument list:
 * 	arg[5]: Chat ID
 * 	arg[6]: Message ID	-> 0: no reply
 * 	arg[7]: Text
 *
 * API_TYPE_LIST:
 * 	TODO
 *
 * API_TYPE_CALLBACK_ANSWER:
 *	arg[5]: Callback ID
 *	arg[6]: Is Text?	-> 0: URL, 1: Text
 *	arg[7]: Text/URL
 *	arg[8]: Show alert?	-> 0: True, 1: False
 */


enum {
	_ARG_CMD_NAME = 0,
	_ARG_TYPE,
	_ARG_TG_API,
	_ARG_TG_API_SECRET_KEY,

	__ARG_END,
};


typedef struct {
	const char  *cmd_name;
	int          api_type;
	const char  *tg_api;
	const char  *tg_api_secret_key;
	int          user_data_len;
	const char **user_data;
} Args;


static int _args_init(int argc1, const char *argv1[], Args *args);
static int _exec(const Args *args);
static int _exec_send_text(const Args *args);
static int _exec_answer_callback(const Args *args);


/*
 * Public
 */
int
main(int argc, char *argv[])
{
	log_init(4096);

	Args args;
	int ret = _args_init(argc + 1, (const char **)&argv[1], &args);
	if (ret < 0)
		goto out0;

	ret = _exec(&args);

out0:
	log_deinit();
	return -ret;
}


int
api_send_text(int type, int64_t chat_id, const int64_t *reply_to, const char text[], int64_t *ret_id)
{
	int _type = 0;
	switch (type) {
	case API_TYPE_TEXT_PLAIN:
		_type = TG_API_TEXT_TYPE_PLAIN;
		break;
	case API_TYPE_TEXT_FORMAT:
		_type = TG_API_TEXT_TYPE_FORMAT;
		break;
	default:
		return -1;
	}

	return tg_api_send_text(_type, chat_id, reply_to, text, ret_id);
}


int
api_delete_message(int64_t chat_id, int64_t message_id)
{
	return tg_api_delete_message(chat_id, message_id);
}


int
api_answer_callback(const char id[], const char text[], const char url[], int show_alert)
{
	return tg_api_answer_callback_query(id, text, url, show_alert);
}


/*
 * Private
 */
static int
_args_init(int argc1, const char *argv1[], Args *args)
{
	if (argc1 <= (__ARG_END + 1)) {
		log_err(EINVAL, "api: _args_init: incomplete mandatory arguments");
		return -1;
	}

	if (argc1 == (__ARG_END + 2)) {
		log_err(EINVAL, "api: _args_init: no user data argument was provided");
		return -1;
	}

	args->cmd_name = argv1[_ARG_CMD_NAME];
	if (cstr_is_empty(args->cmd_name)) {
		log_err(EINVAL, "api: _args_init[%d]: CMD_NAME is empty", _ARG_CMD_NAME);
		return -1;
	}

	int64_t api_type;
	if (cstr_to_int64(argv1[_ARG_TYPE], &api_type) < 0) {
		log_err(errno, "api: _args_init[%d]: ARG_TYPE: failed to parse", _ARG_TYPE);
		return -1;
	}

	args->api_type = (int)api_type;

	args->tg_api = argv1[_ARG_TG_API];
	if (cstr_is_empty(args->tg_api)) {
		log_err(EINVAL, "api: _args_init[%d]: TG_API is empty", _ARG_TG_API);
		return -1;
	}

	args->tg_api_secret_key = argv1[_ARG_TG_API_SECRET_KEY];
	if (cstr_is_empty(args->tg_api_secret_key)) {
		log_err(EINVAL, "api: _args_init[%d]: TG_API_SECRET_KEY is empty", _ARG_TG_API_SECRET_KEY);
		return -1;
	}

	args->user_data = &argv1[__ARG_END];
	args->user_data_len = argc1 - __ARG_END - 2;

#ifdef DEBUG
	for (int i = 0; i < args->user_data_len; i++)
		log_debug("api: _args_init: user data arg[%d]: %s", i, args->user_data[i]);
#endif
	return 0;
}


static int
_exec(const Args *args)
{
	switch (args->api_type) {
	case API_TYPE_TEXT_PLAIN:
	case API_TYPE_TEXT_FORMAT:
		return _exec_send_text(args);
	case API_TYPE_LIST:
		return 0;
	case API_TYPE_CALLBACK_ANSWER:
		return _exec_answer_callback(args);
	}

	log_err(EINVAL, "api: _exec: invalid api type: %d", args->api_type);
	return -1;
}


static int
_exec_send_text(const Args *args)
{
	if (args->user_data_len != 3) {
		log_err(EINVAL, "api: _exec_send_text: invalid argument length");
		return -1;
	}

	int64_t chat_id;
	if (cstr_to_int64(args->user_data[0], &chat_id) < 0) {
		log_err(EINVAL, "api: _exec_send_text: invalid \"Chat ID\" value");
		return -1;
	}

	int64_t message_id;
	if (cstr_to_int64(args->user_data[1], &message_id) < 0) {
		log_err(EINVAL, "api: _exec_send_text: invalid \"Message ID\" value");
		return -1;
	}

	if (cstr_is_empty(args->user_data[2])) {
		log_err(EINVAL, "api: _exec_send_text: \"Text\" is empty");
		return -1;
	}

	tg_api_init(args->tg_api);
	return api_send_text(args->api_type, chat_id, (message_id != 0)? &message_id : NULL,
			     args->user_data[2], NULL);
}


static int
_exec_answer_callback(const Args *args)
{
	if (args->user_data_len != 4) {
		log_err(EINVAL, "api: _exec_answer_callback: invalid argument length");
		return -1;
	}

	const char *const callback_id = args->user_data[0];
	if (cstr_is_empty(callback_id)) {
		log_err(EINVAL, "api: _exec_answer_callback: \"Callback ID\" is empty");
		return -1;
	}

	int64_t is_text;
	if ((cstr_to_int64(args->user_data[1], &is_text) < 0) || (int64_is_bool(is_text) == 0)) {
		log_err(EINVAL, "api: _exec_send_text: invalid \"Is Text\" value");
		return -1;
	}

	const char *const text_url = args->user_data[2];
	if (cstr_is_empty(text_url)) {
		log_err(EINVAL, "api: _exec_answer_callback: \"Text/URL\" is empty");
		return -1;
	}

	int64_t show_alert;
	if ((cstr_to_int64(args->user_data[1], &show_alert) < 0) || (int64_is_bool(show_alert) == 0)) {
		log_err(EINVAL, "api: _exec_send_text: invalid \"Show Alert\" value");
		return -1;
	}

	tg_api_init(args->tg_api);
	if (is_text)
		return api_answer_callback(callback_id, text_url, NULL, (int)show_alert);

	return api_answer_callback(callback_id, NULL, text_url, (int)show_alert);
}
