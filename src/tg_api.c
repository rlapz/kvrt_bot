#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_url = NULL;

static const char *_get_text_parse_mode(int type);
static int         _build_kbd_button(const TgApiKbdButton *b, Str *str);

static void _init_resp(TgApiResp *r, const char req_type[]);
static int  _send_request(TgApiResp *r, const char req[], json_object **ret_obj);
static void _set_message_id(TgApiResp *r, json_object *obj);
static void _set_error_message_sys(TgApiResp *r, int errn, const char msg[]);
static void _set_error_message_api(TgApiResp *r, json_object *obj);


/*
 * Public
 */
void
tg_api_init(const char base_url[])
{
	_base_url = base_url;
}


int
tg_api_text_send(const TgApiText *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "text_send");

	if ((t->chat_id == 0) || (cstr_is_empty(t->text))) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id' or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL) {
		_set_error_message_sys(resp, EINVAL, "arg: 'type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'text': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/sendMessage?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if ((t->msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(text);
	return ret;
}


int
tg_api_text_edit(const TgApiText *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "text_edit");

	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text))) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id', 'msg_id', or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL) {
		_set_error_message_sys(resp, EINVAL, "arg: 'type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'text': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/editMessageText?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(text);
	return ret;
}


int
tg_api_photo_send(const TgApiPhoto *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "photo_send");

	if ((t->chat_id == 0) || (cstr_is_empty(t->photo))) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id' or 'photo': empty!");
		return -1;
	}

	const char *parse_mode = NULL;
	if (cstr_is_empty(t->text) == 0) {
		parse_mode = _get_text_parse_mode(t->text_type);
		if (parse_mode == NULL) {
			_set_error_message_sys(resp, EINVAL, "arg: 'text_type': invalid value!");
			return -1;
		}
	}

	char *const photo = http_url_escape(t->photo);
	if (photo == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'photo': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/sendPhoto?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&photo=%s", photo) == NULL)
		goto out1;
	if ((t->msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;
	if ((parse_mode != NULL) && (cstr_is_empty(t->text) == 0)) {
		char *const caption = http_url_escape(t->text);
		if (caption != NULL)
			str_append_fmt(&str, "%s&caption=%s", parse_mode, caption);

		http_url_escape_free(caption);
	}

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(photo);
	return ret;
}


int
tg_api_animation_send(const TgApiAnimation *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "animation_send");

	if ((t->chat_id == 0) || (cstr_is_empty(t->animation))) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id' or 'animation': empty!");
		return -1;
	}

	const char *parse_mode = NULL;
	if (cstr_is_empty(t->text) == 0) {
		parse_mode = _get_text_parse_mode(t->text_type);
		if (parse_mode == NULL) {
			_set_error_message_sys(resp, EINVAL, "arg: 'text_type': invalid value!");
			return -1;
		}
	}

	char *const animation = http_url_escape(t->animation);
	if (animation == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'animation': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/sendAnimation?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&animation=%s", animation) == NULL)
		goto out1;
	if ((t->msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;
	if ((parse_mode != NULL) && (cstr_is_empty(t->text) == 0)) {
		char *const caption = http_url_escape(t->text);
		if (caption != NULL)
			str_append_fmt(&str, "%s&caption=%s", parse_mode, caption);

		http_url_escape_free(caption);
	}

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(animation);
	return ret;
}


int
tg_api_caption_edit(const TgApiCaption *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "caption_edit");

	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text))) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id', 'msg_id', or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->text_type);
	if (parse_mode == NULL) {
		_set_error_message_sys(resp, EINVAL, "arg: 'text_type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'text': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/editMessageCaption?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&caption=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(text);
	return ret;
}


int
tg_api_callback_answer(const TgApiCallback *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	_init_resp(resp, "callback_answer");

	if (CSTR_IS_EMPTY_OR(t->id, t->value)) {
		_set_error_message_sys(resp, EINVAL, "arg: 'id' or 'value': empty!");
		return -1;
	}

	const char *val_key;
	switch (t->value_type) {
	case TG_API_CALLBACK_VALUE_TYPE_TEXT: val_key = "&text"; break;
	case TG_API_CALLBACK_VALUE_TYPE_URL: val_key = "&url"; break;
	default:
		_set_error_message_sys(resp, EINVAL, "arg: 'value_type': invalid value!");
		return -1;
	}

	char *const value = http_url_escape(t->value);
	if (value == NULL) {
		_set_error_message_sys(resp, ENOMEM, "arg: 'value': failed to encode!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/answerCallbackQuery?callback_query_id=%s", _base_url, t->id) < 0)
		goto out0;
	if (str_append_fmt(&str, "%s=%s", val_key, value) == NULL)
		goto out1;
	if (str_append_fmt(&str, "&show_alert=%s", bool_to_cstr(t->show_alert)) == NULL)
		goto out1;

	ret = _send_request(resp, str.cstr, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	http_url_escape_free(value);
	return ret;
}


int
tg_api_delete(int64_t chat_id, int64_t msg_id, TgApiResp *resp)
{
	assert(resp != NULL);
	_init_resp(resp, "delete_message");

	if ((chat_id == 0) || (msg_id == 0)) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id' or 'msg_id': empty!");
		return -1;
	}

	int ret = -2;
	char buffer[1024];
	if (snprintf(buffer, LEN(buffer), "%s/deleteMessage?chat_id=%" PRIi64 "&message_id=%" PRIi64,
		     _base_url, chat_id, msg_id) < 0) {
		goto out0;
	}

	ret = _send_request(resp, buffer, NULL);
	if (ret >= 0)
		_set_error_message_sys(resp, 0, "");

out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	return ret;
}


char *
tg_api_markup_kbd(const TgApiMarkupKbd *t)
{
	Str str;
	if (str_init_alloc(&str, 1024, NULL) < 0)
		return NULL;

	char *ret = NULL;
	if (str_set_n(&str, "{\"inline_keyboard\": [", 21) == NULL)
		goto out0;

	const unsigned rows_len = t->rows_len;
	for (unsigned i = 0; i < rows_len; i++) {
		if (str_append_c(&str, '[') == NULL)
			goto out0;

		const TgApiKbdButton *cols = t->rows[i].cols;
		const unsigned cols_len = t->rows[i].cols_len;
		for (unsigned j = 0; j < cols_len; j++) {
			if (_build_kbd_button(&cols[j], &str) < 0)
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

	ret = http_url_escape2(str.cstr);

out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_get_admin_list(TgChatAdminList *list, int64_t chat_id, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(list, resp));
	_init_resp(resp, "get_admin_list");

	if (chat_id == 0) {
		_set_error_message_sys(resp, EINVAL, "arg: 'chat_id': empty!");
		return -1;
	}

	int ret = -2;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/getChatAdministrators?chat_id=%" PRIi64, _base_url, chat_id) < 0)
		goto out0;

	json_object *ret_obj;
	ret = _send_request(resp, str.cstr, &ret_obj);
	if (ret < 0)
		goto out1;

	ret = -1;
	json_object *res_obj;
	if (json_object_object_get_ex(ret_obj, "result", &res_obj) == 0) {
		_set_error_message_sys(resp, -1, "invalid http response body!");
		goto out2;
	}

	if (tg_chat_admin_list_parse(list, res_obj) < 0) {
		_set_error_message_sys(resp, -1, "invalid http response body!");
		goto out2;
	}

	_set_error_message_sys(resp, 0, "");
	ret = 0;

out2:
	json_object_put(ret_obj);
out1:
	str_deinit(&str);
out0:
	if (ret == -2)
		_set_error_message_sys(resp, ENOMEM, "failed to build http request!");

	return ret;
}


/*
 * Private
 */
static const char *
_get_text_parse_mode(int type)
{
	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN: return "";
	case TG_API_TEXT_TYPE_FORMAT: return "&parse_mode=MarkdownV2";
	}

	return NULL;
}


static int
_build_kbd_button(const TgApiKbdButton *b, Str *str)
{
	if (str_append_fmt(str, "{\"text\": \"%s\"", b->label) == NULL)
		return -1;

	if (b->args != NULL) {
		if (str_append_n(str, ", \"callback_data\": \"", 20) == NULL)
			return -1;

		const unsigned args_len = b->args_len;
		for (unsigned i = 0; i < args_len; i++) {
			const char *_ret = NULL;
			const TgApiKbdButtonArg *const d = &b->args[i];
			switch (d->type) {
			case TG_API_KBD_BUTTON_ARG_TYPE_INT64:
				_ret = str_append_fmt(str, "%" PRIi64 " ", d->int64);
				break;
			case TG_API_KBD_BUTTON_ARG_TYPE_UINT64:
				_ret = str_append_fmt(str, "%" PRIu64 " ", d->uint64);
				break;
			case TG_API_KBD_BUTTON_ARG_TYPE_TEXT:
				_ret = str_append_fmt(str, "%s ", cstr_empty_if_null(d->text));
				break;
			}

			if (_ret == NULL)
				return -1;
		}

		if (args_len > 0)
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


static void
_init_resp(TgApiResp *r, const char req_type[])
{
	memset(r, 0, sizeof(*r));
	r->req_type = req_type;
}


static int
_send_request(TgApiResp *r, const char req[], json_object **ret_obj)
{
	char *const raw = http_send_get(req, "application/json");
	if (raw == NULL) {
		_set_error_message_sys(r, -1, "failed to send http request!");
		return -1;
	}

	int ret = -1;
	enum json_tokener_error err;
	json_object *obj = json_tokener_parse_verbose(raw, &err);
	if (obj == NULL) {
		_set_error_message_sys(r, -1, json_tokener_error_desc(err));
		goto out0;
	}

	json_object *ok_obj;
	if (json_object_object_get_ex(obj, "ok", &ok_obj) == 0) {
		_set_error_message_sys(r, -1, "invalid http response body!");
		goto out1;
	}

	ret = 0;
	if (json_object_get_boolean(ok_obj)) {
		_set_message_id(r, obj);
	} else {
		_set_error_message_api(r, obj);
		ret = -1;
	}

out1:
	if ((ret == 0) && (ret_obj != NULL))
		*ret_obj = obj;
	else
		json_object_put(obj);
out0:
	free(raw);
	return ret;
}


static void
_set_message_id(TgApiResp *r, json_object *obj)
{
	json_object *res_obj;
	if (json_object_object_get_ex(obj, "result", &res_obj) == 0)
		return;

	json_object *id_obj;
	if (json_object_object_get_ex(res_obj, "message_id", &id_obj) == 0)
		return;

	r->msg_id = json_object_get_int64(id_obj);
}


static void
_set_error_message_sys(TgApiResp *r, int errn, const char msg[])
{
	r->error_code = (errn > 0)? -errn : errn;
	if (r->error_code == 0)
		snprintf(r->error_msg, LEN(r->error_msg), "%s: success: %d", r->req_type, r->error_code);
	else
		snprintf(r->error_msg, LEN(r->error_msg), "%s: error: %d: %s", r->req_type, r->error_code, msg);
}


static void
_set_error_message_api(TgApiResp *r, json_object *obj)
{
	json_object *err_code_obj;
	if (json_object_object_get_ex(obj, "error_code", &err_code_obj) == 0)
		return;

	json_object *err_desc_obj;
	if (json_object_object_get_ex(obj, "description", &err_desc_obj) == 0)
		return;

	r->error_code = json_object_get_int(err_code_obj);
	if (r->error_code == 0) {
		snprintf(r->error_msg, LEN(r->error_msg), "%s: success: %d", r->req_type, r->error_code);
		return;
	}

	const char *const msg = json_object_get_string(err_desc_obj);
	snprintf(r->error_msg, LEN(r->error_msg), "%s: error: %d: %s", r->req_type, r->error_code, msg);
}
