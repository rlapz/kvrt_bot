#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


#define _SET_ERROR_ARG(T, MSG)\
	_set_error(T, __func__, TG_API_RESP_ERR_TYPE_ARG, -EINVAL, MSG)

#define _SET_ERROR_API(T, OBJ)\
	_set_error_api(T, __func__, OBJ)

#define _SET_ERROR_SYS(T, ERRN, MSG)\
	_set_error(T, __func__, TG_API_RESP_ERR_TYPE_SYS, -(abs(ERRN)), MSG)

#define _SET_NO_ERROR(T)\
	_set_error(T, __func__, TG_API_RESP_ERR_TYPE_NONE, 0, NULL)

#define _ERR_BUILD_HTTP_REQ "failed to build http request!"


static const char *_base_url = NULL;


static const char *_get_text_parse_mode(int type);
static int         _build_kbd_button(const TgApiKbdButton *b, Str *str);

static int  _send_request(TgApiResp *r, const char req[], json_object **ret_obj);
static void _set_message_id(TgApiResp *r, json_object *obj);
static void _set_error(TgApiResp *r, const char ctx[], int type, int errn, const char msg[]);
static void _set_error_api(TgApiResp *r, const char ctx[], json_object *obj);


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
	if ((t->chat_id == 0) || (cstr_is_empty(t->text))) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL) {
		_SET_ERROR_ARG(resp, "'type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'text': failed to encode!");
		return -1;
	}

	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/sendMessage?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if ((t->msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(text);
	return ret;
}


int
tg_api_text_edit(const TgApiText *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text))) {
		_SET_ERROR_ARG(resp, "'chat_id', 'msg_id', or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL) {
		_SET_ERROR_ARG(resp, "'type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'text': failed to encode!");
		return -1;
	}

	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/editMessageText?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(text);
	return ret;
}


int
tg_api_photo_send(const TgApiPhoto *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (cstr_is_empty(t->photo))) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'photo': empty!");
		return -1;
	}

	const char *parse_mode = NULL;
	if (cstr_is_empty(t->text) == 0) {
		parse_mode = _get_text_parse_mode(t->text_type);
		if (parse_mode == NULL) {
			_SET_ERROR_ARG(resp, "'text_type': invalid value!");
			return -1;
		}
	}

	char *const photo = http_url_escape(t->photo);
	if (photo == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'photo': failed to encode!");
		return -1;
	}

	int ret = -1;
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

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(photo);
	return ret;
}


int
tg_api_animation_send(const TgApiAnimation *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (cstr_is_empty(t->animation))) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'animation': empty!");
		return -1;
	}

	const char *parse_mode = NULL;
	if (cstr_is_empty(t->text) == 0) {
		parse_mode = _get_text_parse_mode(t->text_type);
		if (parse_mode == NULL) {
			_SET_ERROR_ARG(resp, "'text_type': invalid value!");
			return -1;
		}
	}

	char *const animation = http_url_escape(t->animation);
	if (animation == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'animation': failed to encode!");
		return -1;
	}

	int ret = -1;
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

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(animation);
	return ret;
}


int
tg_api_caption_edit(const TgApiCaption *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text))) {
		_SET_ERROR_ARG(resp, "'chat_id', 'msg_id', or 'text': empty!");
		return -1;
	}

	const char *const parse_mode = _get_text_parse_mode(t->text_type);
	if (parse_mode == NULL) {
		_SET_ERROR_ARG(resp, "'text_type': invalid value!");
		return -1;
	}

	char *const text = http_url_escape(t->text);
	if (text == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'text': failed to encode!");
		return -1;
	}

	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/editMessageCaption?chat_id=%" PRIi64, _base_url, t->chat_id) < 0)
		goto out0;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&caption=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(text);
	return ret;
}


int
tg_api_callback_answer(const TgApiCallback *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if (CSTR_IS_EMPTY_OR(t->id, t->value)) {
		_SET_ERROR_ARG(resp, "'id' or 'value': empty!");
		return -1;
	}

	const char *val_key;
	switch (t->value_type) {
	case TG_API_CALLBACK_VALUE_TYPE_TEXT: val_key = "&text"; break;
	case TG_API_CALLBACK_VALUE_TYPE_URL: val_key = "&url"; break;
	default:
		_SET_ERROR_ARG(resp, "'value_type': invalid value!");
		return -1;
	}

	char *const value = http_url_escape(t->value);
	if (value == NULL) {
		_SET_ERROR_SYS(resp, ENOMEM, "'value': failed to encode!");
		return -1;
	}

	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024, "%s/answerCallbackQuery?callback_query_id=%s", _base_url, t->id) < 0)
		goto out0;
	if (str_append_fmt(&str, "%s=%s", val_key, value) == NULL)
		goto out1;
	if (str_append_fmt(&str, "&show_alert=%s", bool_to_cstr(t->show_alert)) == NULL)
		goto out1;

	if (_send_request(resp, str.cstr, NULL) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	str_deinit(&str);
out0:
	if (ret < 0)
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);

	http_url_escape_free(value);
	return ret;
}


int
tg_api_delete(int64_t chat_id, int64_t msg_id, TgApiResp *resp)
{
	assert(resp != NULL);
	if ((chat_id == 0) || (msg_id == 0)) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'msg_id': empty!");
		return -1;
	}

	char buffer[1024];
	const int ret = snprintf(buffer, LEN(buffer), "%s/deleteMessage?chat_id=%" PRIi64 "&message_id=%" PRIi64,
				 _base_url, chat_id, msg_id);

	if ((ret < 0) || ((size_t)ret >= LEN(buffer))) {
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);
		return -1;
	}

	if (_send_request(resp, buffer, NULL) < 0)
		return -1;

	_SET_NO_ERROR(resp);
	return 0;
}


int
tg_api_ban(int64_t chat_id, int64_t user_id, TgApiResp *resp)
{
	assert(resp != NULL);
	if ((chat_id == 0) || (user_id == 0)) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'msg_id': empty!");
		return -1;
	}

	char buffer[1024];
	const int ret = snprintf(buffer, LEN(buffer), "%s/banChatMember?chat_id=%" PRIi64
						      "&user_id=%" PRIi64 "&revoke_messages=false",
				 _base_url, chat_id, user_id);

	if ((ret < 0) || ((size_t)ret >= LEN(buffer))) {
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);
		return -1;
	}

	if (_send_request(resp, buffer, NULL) < 0)
		return -1;

	_SET_NO_ERROR(resp);
	return 0;
}


int
tg_api_unban(int64_t chat_id, int64_t user_id, TgApiResp *resp)
{
	assert(resp != NULL);
	if ((chat_id == 0) || (user_id == 0)) {
		_SET_ERROR_ARG(resp, "'chat_id' or 'msg_id': empty!");
		return -1;
	}

	char buffer[1024];
	const int ret = snprintf(buffer, LEN(buffer), "%s/unbanChatMember?chat_id=%" PRIi64 "&user_id=%" PRIi64,
				 _base_url, chat_id, user_id);

	if ((ret < 0) || ((size_t)ret >= LEN(buffer))) {
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);
		return -1;
	}

	if (_send_request(resp, buffer, NULL) < 0)
		return -1;

	_SET_NO_ERROR(resp);
	return 0;
}


char *
tg_api_markup_kbd(const TgApiMarkupKbd *t)
{
	Str str;
	if (str_init_alloc(&str, 1024, "%s", "{\"inline_keyboard\": [") < 0)
		return NULL;

	char *ret = NULL;
	const unsigned rows_len = t->rows_len;
	for (unsigned i = 0; i < rows_len; i++) {
		if (str_append_c(&str, '[') == NULL)
			goto out0;

		const TgApiKbdButton *const cols = t->rows[i].cols;
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

	char *const esc = http_url_escape(str.cstr);
	if (esc == NULL)
		goto out0;

	ret = strdup(esc);
	http_url_escape_free(esc);

out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_get_admin_list(TgChatAdminList *list, int64_t chat_id, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(list, resp));
	if (chat_id == 0) {
		_SET_ERROR_ARG(resp, "'chat_id': empty!");
		return -1;
	}

	Str str;
	if (str_init_alloc(&str, 1024, "%s/getChatAdministrators?chat_id=%" PRIi64, _base_url, chat_id) < 0) {
		_SET_ERROR_SYS(resp, ENOMEM, _ERR_BUILD_HTTP_REQ);
		return -1;
	}

	int ret = -1;
	json_object *ret_obj;
	ret = _send_request(resp, str.cstr, &ret_obj);
	if (ret < 0)
		goto out0;

	json_object *res_obj;
	if (json_object_object_get_ex(ret_obj, "result", &res_obj) == 0)
		goto out1;
	if (tg_chat_admin_list_parse(list, res_obj) < 0)
		goto out1;

	_SET_NO_ERROR(resp);
	ret = 0;

out1:
	json_object_put(ret_obj);
	if (ret < 0)
		_SET_ERROR_SYS(resp, -1, "invalid http response body!");
out0:
	str_deinit(&str);
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


static int
_send_request(TgApiResp *r, const char req[], json_object **ret_obj)
{
	char *const raw = http_send_get(req, "application/json");
	if (raw == NULL) {
		_SET_ERROR_SYS(r, -1, "failed to send http request!");
		return -1;
	}

	int ret = -1;
	enum json_tokener_error err;
	json_object *const obj = json_tokener_parse_verbose(raw, &err);
	if (obj == NULL) {
		_SET_ERROR_SYS(r, -1, json_tokener_error_desc(err));
		goto out0;
	}

	json_object *ok_obj;
	if (json_object_object_get_ex(obj, "ok", &ok_obj) == 0) {
		_SET_ERROR_SYS(r, -1, "invalid http response body!");
		goto out1;
	}

	if (json_object_get_boolean(ok_obj) == 0) {
		_SET_ERROR_API(r, obj);
		goto out1;
	}

	_set_message_id(r, obj);
	ret = 0;

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
_set_error(TgApiResp *r, const char ctx[], int type, int errn, const char msg[])
{
	assert(!VAL_IS_NULL_OR(r, ctx, (errn != 0)? msg : ""));
	assert(LEN(r->error_msg) > 0);

	const char *err_type = "unknown";
	switch (type) {
	case TG_API_RESP_ERR_TYPE_ARG: err_type = "arg"; break;
	case TG_API_RESP_ERR_TYPE_API: err_type = "api"; break;
	case TG_API_RESP_ERR_TYPE_SYS: err_type = "sys"; break;
	}

	r->err_type = type;
	r->error_code = errn;
	r->error_msg[0] = '\0';
	if (errn == 0)
		return;

	const size_t nlen = LEN(r->error_msg);
	const int len = snprintf(r->error_msg, nlen, "%s: [%s]: %d: %s", ctx, err_type, errn, msg);
	if (len < 0)
		return;

	/* truncated */
	if ((size_t)len >= nlen)
		cstr_copy_n(r->error_msg + (nlen - 1) , 4, "...");
}


static void
_set_error_api(TgApiResp *r, const char ctx[], json_object *obj)
{
	json_object *err_code_obj;
	if (json_object_object_get_ex(obj, "error_code", &err_code_obj) == 0)
		return;

	json_object *err_desc_obj;
	if (json_object_object_get_ex(obj, "description", &err_desc_obj) == 0)
		return;

	const char *const msg = json_object_get_string(err_desc_obj);
	const int errn = json_object_get_int(err_code_obj);
	_set_error(r, ctx, TG_API_RESP_ERR_TYPE_API, errn, msg);
}
