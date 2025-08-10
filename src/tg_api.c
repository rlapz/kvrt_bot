#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_api = NULL;


static void        _init_args(TgApi *t);
static const char *_get_parse_mode_key(int type);
static int         _get_edit_query(const TgApi *t, const char *method[], const char *val_key[],
				   const char *val[]);

static int  _handle_send_text(TgApi *t);
static int  _handle_send_photo(TgApi *t);
static int  _handle_edit_message(TgApi *t);
static int  _handle_delete_message(TgApi *t);
static int  _handle_answer_callback(TgApi *t);

static int _build_kbd_buttons(const TgApiKbdButton *b, Str *str);

static int _parse_response(TgApi *t, const char resp[]);
static int _set_error_message(TgApi *t, json_object *obj);
static int _set_message_id(TgApi *t, json_object *obj);


/*
 * Public
 */
void
tg_api_global_init(const char base_api[])
{
	_base_api = base_api;
}


const char *
tg_api_err_str(int err)
{
	switch (err) {
	case TG_API_ERR_NONE: return "TG_API_ERR_NONE: no error";
	case TG_API_ERR_API: return "TG_API_ERR_API: error from the telegram api";
	case TG_API_ERR_TYPE_INVALID: return "TG_API_ERR_TYPE_INVALID: invalid method type";
	case TG_API_ERR_TEXT_TYPE_INVALID: return "TG_API_ERR_TYPE_TEXT_INVALID: invalid text type";
	case TG_API_ERR_VALUE_TYPE_INVALID: return "TG_API_ERR_VALUE_TYPE_INVALID: invalid value type";
	case TG_API_ERR_VALUE_INVALID: return "TG_API_ERR_VALUE_INVALID: empty or invalid value";
	case TG_API_ERR_CAPTION_INVALID: return "TG_API_ERR_CAPTION_INVALID: empty or invalid caption";
	case TG_API_ERR_CHAT_ID_INVALID: return "TG_API_ERR_CHAT_ID_INVALID: invalid chat id";
	case TG_API_ERR_USER_ID_INVALID: return "TG_API_ERR_USER_ID_INVALID: invalid user id";
	case TG_API_ERR_MSG_ID_INVALID: return "TG_API_ERR_MSG_ID_INVALID: invalid message id";
	case TG_API_ERR_REPLY_MSG_ID_INVALID: return "TG_API_ERR_REPLY_MSG_ID_INVALID: invalid reply to message id";
	case TG_API_ERR_CALLBACK_ID_INVALID: return "TG_API_ERR_CALLBACK_ID_INVALID: invalid callback id";
	case TG_API_ERR_HAS_NO_TYPE: return "TG_API_ERR_HAS_NO_TYPE: has no method type";
	case TG_API_ERR_INVALID_RESP: return "TG_API_ERR_INVALID_RESP: invalid response from the telegram api";
	case TG_API_ERR_SEND_REQ: return "TG_API_ERR_SEND_REQ: failed to send request";
	case TG_API_ERR_INTERNAL: return "TG_API_ERR_INTERNAL: internal error";
	}

	return "TG_API_ERR_UNKNOWN: unknown error";
}


int
tg_api_init(TgApi *t)
{
	assert(_base_api != NULL);

	_init_args(t);
	if (str_init_alloc(&t->str, 1024) < 0)
		return TG_API_ERR_INTERNAL;

	int ret = TG_API_ERR_NONE;
	switch (t->type) {
	case TG_API_TYPE_SEND_TEXT: ret = _handle_send_text(t); break;
	case TG_API_TYPE_SEND_PHOTO: ret = _handle_send_photo(t); break;
	case TG_API_TYPE_EDIT_TEXT:
	case TG_API_TYPE_EDIT_CAPTION: ret = _handle_edit_message(t); break;
	case TG_API_TYPE_DELETE_MESSAGE: ret = _handle_delete_message(t); break;
	case TG_API_TYPE_ANSWER_CALLBACK: ret = _handle_answer_callback(t); break;
	default: ret = TG_API_ERR_TYPE_INVALID; break;
	}

	if (ret < 0)
		str_deinit(&t->str);

	return ret;
}


void
tg_api_deinit(TgApi *t)
{
	json_object_put(t->res_json);
	str_deinit(&t->str);
}


int
tg_api_exec(TgApi *t)
{
	if (cstr_is_empty(t->str.cstr))
		return TG_API_ERR_HAS_NO_TYPE;

	char *const resp = http_send_get(t->str.cstr, "application/json");
	if (resp == NULL)
		return TG_API_ERR_SEND_REQ;

	const int ret = _parse_response(t, resp);
	free(resp);
	return ret;
}


int
tg_api_add_kbd(TgApi *t, const TgApiKbd *kbd)
{
	int ret = TG_API_ERR_INTERNAL;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_n(&str, "{\"inline_keyboard\": [", 21) == NULL)
		goto out0;

	const unsigned rows_len = kbd->rows_len;
	for (unsigned i = 0; i < rows_len; i++) {
		if (str_append_n(&str, "[", 1) == NULL)
			goto out0;

		const TgApiKbdButton *cols = kbd->rows[i].cols;
		const unsigned cols_len = kbd->rows[i].cols_len;
		for (unsigned j = 0; j < cols_len; j++) {
			if (_build_kbd_buttons(&cols[j], &str) < 0)
				goto out0;
		}

		if (cols_len > 0)
			str_pop(&str, 2);

		if (str_append_n(&str, "], ", 3) == NULL)
			goto out0;
	}

	if (rows_len > 0)
		str_pop(&str, 2);

	if (str_append_n(&str, "]}", 2) == NULL)
		goto out0;

	char *const kbd_str = curl_easy_escape(NULL, str.cstr, (int)str.len);
	if (kbd_str == NULL)
		goto out0;

	if (str_append_fmt(&t->str, "&reply_markup=%s", kbd_str) == NULL)
		goto out0;

	ret = TG_API_ERR_NONE;

out0:
	str_deinit(&str);
	return ret;
}


/*
 * Private
 */
static void
_init_args(TgApi *t)
{
	t->ret_msg_id = 0;
	t->api_err_code = 0;
	t->api_err_msg = NULL;
	t->res_json = NULL;
	t->has_msg_id = 0;
	memset(&t->str, 0, sizeof(t->str));
}


static const char *
_get_parse_mode_key(int type)
{
	switch (type) {
	case TG_API_PARSE_TYPE_PLAIN: return "";
	case TG_API_PARSE_TYPE_FORMAT: return "&parse_mode=MarkdownV2";
	}

	return NULL;
}


static int
_get_edit_query(const TgApi *t, const char *method[], const char *val_key[], const char *val[])
{
	switch (t->type) {
	case TG_API_TYPE_EDIT_TEXT:
		if (cstr_is_empty(t->value))
			return TG_API_ERR_VALUE_INVALID;
		*method = "editMessageText";
		*val_key = "text";
		*val = t->value;
		break;
	case TG_API_TYPE_EDIT_CAPTION:
		if (cstr_is_empty(t->caption))
			return TG_API_ERR_CAPTION_INVALID;
		*method = "editMessageCaption";
		*val_key = "caption";
		*val = t->caption;
		break;
	default:
		return TG_API_ERR_TYPE_INVALID;
	}

	return TG_API_ERR_NONE;
}


static int
_handle_send_text(TgApi *t)
{
	Str *const s = &t->str;
	if (t->chat_id == 0)
		return TG_API_ERR_CHAT_ID_INVALID;
	if (cstr_is_empty(t->value))
		return TG_API_ERR_VALUE_INVALID;
	if (t->value_type != TG_API_VALUE_TYPE_TEXT)
		return TG_API_ERR_VALUE_TYPE_INVALID;

	const char *const text_param_key = _get_parse_mode_key(t->parse_type);
	if (text_param_key == NULL)
		return TG_API_ERR_TEXT_TYPE_INVALID;

	int ret = TG_API_ERR_INTERNAL;
	if (str_set_fmt(s, "%s/sendMessage?chat_id=%" PRIi64, _base_api, t->chat_id) == NULL)
		return ret;
	if ((t->msg_id != 0) && (str_append_fmt(s, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		return ret;

	char *const text = http_url_escape(t->value);
	if (text == NULL)
		return ret;

	if (str_append_fmt(s, "%s&text=%s", text_param_key, text) == NULL)
		goto out0;

	t->has_msg_id = 1;
	ret = TG_API_ERR_NONE;

out0:
	http_url_escape_free(text);
	return ret;
}


static int
_handle_send_photo(TgApi *t)
{
	Str *const s = &t->str;
	if (t->chat_id == 0)
		return TG_API_ERR_CHAT_ID_INVALID;
	if (cstr_is_empty(t->value))
		return TG_API_ERR_VALUE_INVALID;
	if (t->value_type != TG_API_VALUE_TYPE_URL)
		return TG_API_ERR_VALUE_TYPE_INVALID;

	int ret = TG_API_ERR_INTERNAL;
	if (str_set_fmt(s, "%s/sendPhoto?chat_id=%" PRIi64, _base_api, t->chat_id) == NULL)
		return ret;
	if ((t->msg_id != 0) && (str_append_fmt(s, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		return ret;

	char *const photo = http_url_escape(t->value);
	if (photo == NULL)
		return ret;

	if (str_append_fmt(s, "&photo=%s", photo) == NULL)
		goto out0;

	if (cstr_is_empty(t->caption) == 0) {
		const char *const parse_mode_key = _get_parse_mode_key(t->parse_type);
		if (parse_mode_key == NULL) {
			ret = TG_API_ERR_TEXT_TYPE_INVALID;
			goto out0;
		}

		char *const capt_e = http_url_escape(t->caption);
		if (capt_e != NULL) {
			str_append_fmt(s, "%s&caption=%s", parse_mode_key, capt_e);
			http_url_escape_free(capt_e);
		}
	}

	t->has_msg_id = 1;
	ret = TG_API_ERR_NONE;

out0:
	http_url_escape_free(photo);
	return ret;
}


static int
_handle_edit_message(TgApi *t)
{
	if (t->chat_id == 0)
		return TG_API_ERR_CHAT_ID_INVALID;
	if (t->msg_id == 0)
		return TG_API_ERR_MSG_ID_INVALID;

	const char *method_type;
	const char *value_key;
	const char *value;
	int ret = _get_edit_query(t, &method_type, &value_key, &value);
	if (ret != TG_API_ERR_NONE)
		return ret;

	ret = TG_API_ERR_INTERNAL;
	if (str_set_fmt(&t->str, "%s/%s?chat_id=%" PRIi64, _base_api, method_type, t->chat_id) == NULL)
		return ret;
	if (str_append_fmt(&t->str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		return ret;

	const char *const parse_mode_key = _get_parse_mode_key(t->parse_type);
	if (parse_mode_key == NULL)
		return TG_API_ERR_TEXT_TYPE_INVALID;

	char *const value_e = http_url_escape(value);
	if (value_e == NULL)
		return ret;

	if (str_append_fmt(&t->str, "%s&%s=%s", parse_mode_key, value_key, value_e) == NULL)
		goto out0;

	ret = TG_API_ERR_NONE;

out0:
	http_url_escape_free(value_e);
	return ret;
}


static int
_handle_delete_message(TgApi *t)
{
	if (t->chat_id == 0)
		return TG_API_ERR_CHAT_ID_INVALID;
	if (t->msg_id == 0)
		return TG_API_ERR_MSG_ID_INVALID;

	if (str_set_fmt(&t->str, "%s/deleteMessage?chat_id=%" PRIi64, _base_api, t->chat_id) == NULL)
		return TG_API_ERR_INTERNAL;
	if (str_append_fmt(&t->str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		return TG_API_ERR_INTERNAL;

	return TG_API_ERR_NONE;
}


static int
_handle_answer_callback(TgApi *t)
{
	if (cstr_is_empty(t->callback_id))
		return TG_API_ERR_CALLBACK_ID_INVALID;
	if (cstr_is_empty(t->value))
		return TG_API_ERR_VALUE_INVALID;

	const char *arg_key;
	switch (t->value_type) {
	case TG_API_VALUE_TYPE_TEXT: arg_key = "&text"; break;
	case TG_API_VALUE_TYPE_URL: arg_key = "&url"; break;
	}

	if (str_set_fmt(&t->str, "%s/answerCallbackQuery?callback_query_id=%s", _base_api, t->callback_id) == NULL)
		return TG_API_ERR_INTERNAL;

	char *const value = http_url_escape(t->value);
	if (value == NULL)
		return TG_API_ERR_INTERNAL;

	int ret = TG_API_ERR_INTERNAL;
	if (str_append_fmt(&t->str, "%s=%s", arg_key, value) == NULL)
		goto out0;

	if (str_append_fmt(&t->str, "&show_alert=%s", bool_to_cstr(t->callback_show_alert)) == NULL)
		goto out0;

	ret = TG_API_ERR_NONE;

out0:
	http_url_escape_free(value);
	return ret;
}


static int
_build_kbd_buttons(const TgApiKbdButton *b, Str *str)
{
	if (str_append_fmt(str, "{\"text\": \"%s\"", b->text) == NULL)
		return -1;

	if (b->data_list != NULL) {
		if (str_append_n(str, ", \"callback_data\": \"", 20) == NULL)
			return -1;

		const unsigned data_list_len = b->data_list_len;
		for (unsigned k = 0; k < data_list_len; k++) {
			const char *_ret = NULL;
			const TgApiKbdButtonData *const d = &b->data_list[k];
			switch (d->type) {
			case TG_API_KBD_BUTTON_DATA_TYPE_INT64:
				_ret = str_append_fmt(str, "%" PRIi64 " ", d->int64);
				break;
			case TG_API_KBD_BUTTON_DATA_TYPE_UINT64:
				_ret = str_append_fmt(str, "%" PRIu64 " ", d->uint64);
				break;
			case TG_API_KBD_BUTTON_DATA_TYPE_TEXT:
				_ret = str_append_fmt(str, "%s ", cstr_empty_if_null(d->text));
				break;
			}

			if (_ret == NULL)
				return -1;
		}

		if (data_list_len > 0)
			str_pop(str, 1);

		if (str_append_c(str, '"') == NULL)
			return -1;
	} else if (b->url != NULL) {
		if (str_append_fmt(str, ", \"url\": \"%s\"", b->url) == NULL)
			return -1;
	}

	if (str_append_n(str, "}, ", 3) == NULL)
		return -1;

	return 0;
}


static int
_parse_response(TgApi *t, const char resp[])
{
	json_object *const obj = json_tokener_parse(resp);
	if (obj == NULL)
		return TG_API_ERR_INVALID_RESP;

	int ret = TG_API_ERR_INVALID_RESP;
	json_object *ok_obj;
	if (json_object_object_get_ex(obj, "ok", &ok_obj) == 0)
		goto out0;

	if (json_object_get_boolean(ok_obj))
		ret = _set_message_id(t, obj);
	else
		ret = _set_error_message(t, obj);

out0:
	if (ret < 0)
		json_object_put(obj);
	else
		t->res_json = obj;

	return ret;
}


static int
_set_error_message(TgApi *t, json_object *obj)
{
	json_object *error_code_obj;
	if (json_object_object_get_ex(obj, "error_code", &error_code_obj) == 0)
		return TG_API_ERR_INVALID_RESP;

	json_object *desc_obj;
	if (json_object_object_get_ex(obj, "description", &desc_obj) == 0)
		return TG_API_ERR_INVALID_RESP;

	t->api_err_code = json_object_get_int(error_code_obj);
	t->api_err_msg = json_object_get_string(desc_obj);
	return TG_API_ERR_API;
}


static int
_set_message_id(TgApi *t, json_object *obj)
{
	if (t->has_msg_id == 0)
		return TG_API_ERR_NONE;

	json_object *res_obj;
	if (json_object_object_get_ex(obj, "result", &res_obj) == 0)
		return TG_API_ERR_INVALID_RESP;

	json_object *id_obj;
	if (json_object_object_get_ex(res_obj, "message_id", &id_obj) == 0)
		return TG_API_ERR_INVALID_RESP;

	t->ret_msg_id = json_object_get_int64(id_obj);
	return TG_API_ERR_NONE;
}
