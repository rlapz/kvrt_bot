#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_url = NULL;

static const char *_get_text_parse_mode(int type);
static int         _build_kbd_button(const TgApiKbdButton *b, Str *str);

static int _send_request(TgApiResp *r, const char req_type[], const char req[]);
static int _send_request2(TgApiResp *r, const char req_type[], const char req[], json_object **ret_obj);
static int _set_message_id(TgApiResp *r, json_object *obj);
static int _set_error_message(TgApiResp *r, json_object *obj);


/*
 * Public
 */
void
tg_api_init(const char base_url[])
{
	_base_url = base_url;
}


const char *
tg_api_resp_str(const TgApiResp *t, char buffer[], size_t size)
{
	Str str;
	str_init(&str, buffer, size);
	return str_set_fmt(&str, "Error: %s: %d: %s.", t->req_type, t->error_code, t->error_msg);
}


int
tg_api_text_send(const TgApiText *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (cstr_is_empty(t->text)))
		return TG_API_RESP_ERR_ARG;

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL)
		return TG_API_RESP_ERR_ARG;

	int ret = TG_API_RESP_ERR_SYS;
	char *const text = http_url_escape(t->text);
	if (text == NULL)
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;
	if (str_set_fmt(&str, "%s/sendMessage?chat_id=%" PRIi64, _base_url, t->chat_id) == NULL)
		goto out1;
	if ((t->msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, t->msg_id) == NULL))
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, "text_send", str.cstr);

out1:
	str_deinit(&str);
out0:
	http_url_escape_free(text);
	return ret;
}


int
tg_api_text_edit(const TgApiText *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text)))
		return TG_API_RESP_ERR_ARG;

	const char *const parse_mode = _get_text_parse_mode(t->type);
	if (parse_mode == NULL)
		return TG_API_RESP_ERR_ARG;

	int ret = TG_API_RESP_ERR_SYS;
	char *const text = http_url_escape(t->text);
	if (text == NULL)
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;
	if (str_set_fmt(&str, "%s/editMessageText?chat_id=%" PRIi64, _base_url, t->chat_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&text=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, "text_edit", str.cstr);

out1:
	str_deinit(&str);
out0:
	http_url_escape_free(text);
	return ret;
}


int
tg_api_photo_send(const TgApiPhoto *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (cstr_is_empty(t->photo)))
		return TG_API_RESP_ERR_ARG;

	const char *parse_mode = NULL;
	if (cstr_is_empty(t->text) == 0)
		parse_mode = _get_text_parse_mode(t->text_type);

	int ret = TG_API_RESP_ERR_SYS;
	char *const photo = http_url_escape(t->photo);
	if (photo == NULL)
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;
	if (str_set_fmt(&str, "%s/sendPhoto?chat_id=%" PRIi64, _base_url, t->chat_id) == NULL)
		goto out1;
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

	ret = _send_request(resp, "photo_send", str.cstr);

out1:
	str_deinit(&str);
out0:
	http_url_escape_free(photo);
	return ret;
}


int
tg_api_caption_edit(const TgApiCaption *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if ((t->chat_id == 0) || (t->msg_id == 0) || (cstr_is_empty(t->text)))
		return TG_API_RESP_ERR_ARG;

	const char *const parse_mode = _get_text_parse_mode(t->text_type);
	if (parse_mode == NULL)
		return TG_API_RESP_ERR_ARG;

	int ret = TG_API_RESP_ERR_SYS;
	char *const text = http_url_escape(t->text);
	if (text == NULL)
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;
	if (str_set_fmt(&str, "%s/editMessageCaption?chat_id=%" PRIi64, _base_url, t->chat_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, t->msg_id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s&caption=%s", parse_mode, text) == NULL)
		goto out1;
	if ((cstr_is_empty(t->markup) == 0) && (str_append_fmt(&str, "&reply_markup=%s", t->markup) == NULL))
		goto out1;

	ret = _send_request(resp, "caption_edit", str.cstr);

out1:
	str_deinit(&str);
out0:
	http_url_escape_free(text);
	return ret;
}


int
tg_api_callback_answer(const TgApiCallback *t, TgApiResp *resp)
{
	assert(!VAL_IS_NULL_OR(t, resp));
	if (CSTR_IS_EMPTY_OR(t->id, t->value))
		return TG_API_RESP_ERR_ARG;

	const char *val_key;
	switch (t->value_type) {
	case TG_API_CALLBACK_VALUE_TYPE_TEXT: val_key = "&text"; break;
	case TG_API_CALLBACK_VALUE_TYPE_URL: val_key = "&url"; break;
	default: return TG_API_RESP_ERR_ARG;
	}

	int ret = TG_API_RESP_ERR_SYS;
	char *const value = http_url_escape(t->value);
	if (value == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;
	if (str_set_fmt(&str, "%s/answerCallbackQuery?callback_query_id=%s", _base_url, t->id) == NULL)
		goto out1;
	if (str_append_fmt(&str, "%s=%s", val_key, value) == NULL)
		goto out1;

	ret = _send_request(resp, "callback_answer", str.cstr);

out1:
	str_deinit(&str);
out0:
	http_url_escape_free(value);
	return ret;
}


int
tg_api_delete(int64_t chat_id, int64_t msg_id, TgApiResp *resp)
{
	if ((chat_id == 0) || (msg_id == 0))
		return TG_API_RESP_ERR_ARG;

	char buffer[1024];
	Str str;
	str_init(&str, buffer, LEN(buffer));
	if (str_set_fmt(&str, "%s/deleteMessage?chat_id=%" PRIi64, _base_url, chat_id) == NULL)
		return TG_API_RESP_ERR_SYS;
	if (str_append_fmt(&str, "&message_id=%" PRIi64, msg_id) == NULL)
		return TG_API_RESP_ERR_SYS;

	return _send_request(resp, "delete", str.cstr);
}


char *
tg_api_markup_kbd(const TgApiMarkupKbd *t)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
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
tg_api_get_admin_list(int64_t chat_id, TgApiResp *resp)
{
	assert(resp != NULL);
	if ((chat_id == 0) || (resp->udata == NULL))
		return TG_API_RESP_ERR_ARG;

	int ret = TG_API_RESP_ERR_SYS;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;
	if (str_set_fmt(&str, "%s/getChatAdministrators?chat_id=%" PRIi64, _base_url, chat_id) == NULL)
		goto out0;

	json_object *ret_obj;
	ret = _send_request2(resp, "get_admin_list", str.cstr, &ret_obj);
	if (ret < 0)
		goto out0;

	ret = TG_API_RESP_ERR_API;
	json_object *res_obj;
	if (json_object_object_get_ex(ret_obj, "result", &res_obj) == 0)
		goto out1;

	TgChatAdminList *const list = (TgChatAdminList *)resp->udata;
	if (tg_chat_admin_list_parse(list, res_obj) < 0)
		goto out1;

	ret = 0;

out1:
	json_object_put(ret_obj);
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
_send_request(TgApiResp *r, const char req_type[], const char req[])
{
	char *const raw = http_send_get(req, "application/json");
	if (raw == NULL)
		return TG_API_RESP_ERR_API;

	int ret = TG_API_RESP_ERR_API;
	json_object *obj = json_tokener_parse(raw);
	if (obj == NULL)
		goto out0;

	json_object *ok_obj;
	if (json_object_object_get_ex(obj, "ok", &ok_obj) == 0)
		goto out1;

	memset(r, 0, sizeof(*r));
	if (json_object_get_boolean(ok_obj))
		ret = _set_message_id(r, obj);
	else
		ret = _set_error_message(r, obj);

	r->req_type = req_type;

out1:
	json_object_put(obj);
out0:
	free(raw);
	return ret;
}


static int
_send_request2(TgApiResp *r, const char req_type[], const char req[], json_object **ret_obj)
{
	char *const raw = http_send_get(req, "application/json");
	if (raw == NULL)
		return TG_API_RESP_ERR_API;

	int ret = TG_API_RESP_ERR_API;
	json_object *obj = json_tokener_parse(raw);
	if (obj == NULL)
		goto out0;

	json_object *ok_obj;
	if (json_object_object_get_ex(obj, "ok", &ok_obj) == 0)
		goto out1;

	if (json_object_get_boolean(ok_obj))
		ret = _set_message_id(r, obj);
	else
		ret = _set_error_message(r, obj);

	r->req_type = req_type;

out1:
	if (ret == 0)
		*ret_obj = obj;
	else
		json_object_put(obj);
out0:
	free(raw);
	return ret;
}


static int
_set_message_id(TgApiResp *r, json_object *obj)
{
	json_object *res_obj;
	if (json_object_object_get_ex(obj, "result", &res_obj) == 0)
		return 0;

	json_object *id_obj;
	if (json_object_object_get_ex(res_obj, "message_id", &id_obj) == 0)
		return 0;

	r->msg_id = json_object_get_int64(id_obj);
	return 0;
}


static int
_set_error_message(TgApiResp *r, json_object *obj)
{
	json_object *err_code_obj;
	if (json_object_object_get_ex(obj, "error_code", &err_code_obj) == 0)
		return TG_API_RESP_ERR_API;

	json_object *err_desc_obj;
	if (json_object_object_get_ex(obj, "description", &err_desc_obj) == 0)
		return TG_API_RESP_ERR_API;

	r->error_code = json_object_get_int(err_code_obj);
	cstr_copy_n(r->error_msg, LEN(r->error_msg), json_object_get_string(err_desc_obj));
	return TG_API_RESP_ERR_API;
}
