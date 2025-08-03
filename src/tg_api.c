#include <assert.h>
#include <inttypes.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_api = NULL;


static char *_build_keyboard(const TgApiKeyboard *k);
static int   _build_keyboard_buttons(const TgApiKeyboardButton *b, Str *str);

static int _parse_response(const char resp[], json_object **res_obj);
static int _response_get_message_id(json_object *obj, int64_t *ret_id);


/*
 * Public
 */
void
tg_api_init(const char base_api[])
{
	_base_api = base_api;
}


int
tg_api_send_text(int type, int64_t chat_id, int64_t reply_to, const char text[], int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	const char *arg_key;
	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN: arg_key = "&text"; break;
	case TG_API_TEXT_TYPE_FORMAT: arg_key = "&parse_mode=MarkdownV2&text"; break;
	default: return ret;
	}

	if (cstr_is_empty(text))
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_fmt(&str, "%s/sendMessage?chat_id=%" PRIi64, _base_api, chat_id) == NULL)
		goto out0;

	if ((reply_to != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, reply_to) == NULL))
		goto out0;

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	const char *const req = str_append_fmt(&str, "%s=%s", arg_key, new_text);
	if (req == NULL)
		goto out1;

	char *const resp = http_send_get(req, "application/json");
	if (resp == NULL)
		goto out1;

	json_object *res_obj;
	ret = _parse_response(resp, &res_obj);
	if (ret < 0)
		goto out2;

	ret = _response_get_message_id(res_obj, ret_id);

	json_object_put(res_obj);

out2:
	free(resp);
out1:
	curl_free(new_text);
out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_send_photo(int type, int64_t chat_id, int64_t reply_to, const char photo[], const char capt[],
		  int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	// TODO: send file
	if (type != TG_API_PHOTO_TYPE_URL)
		return ret;

	if (cstr_is_empty(photo))
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	char *const photo_e = http_url_escape(photo);
	if (photo_e == NULL)
		goto out0;

	if (str_set_fmt(&str, "%s/sendPhoto?photo=%s&chat_id=%" PRIi64, _base_api, photo_e, chat_id) == NULL)
		goto out1;

	if ((reply_to != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, reply_to) == NULL))
		goto out1;

	if (cstr_is_empty(capt) == 0) {
		char *const capt_e = http_url_escape(capt);
		if (capt_e != NULL) {
			str_append_fmt(&str, "&caption=%s", capt_e);
			http_url_escape_free(capt_e);
		}
	}

	char *const resp = http_send_get(str.cstr, "application/json");
	if (resp == NULL)
		goto out1;

	json_object *res_obj;
	ret = _parse_response(resp, &res_obj);
	if (ret < 0)
		goto out2;

	ret = _response_get_message_id(res_obj, ret_id);

	json_object_put(res_obj);

out2:
	free(resp);
out1:
	http_url_escape_free(photo_e);
out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_delete_message(int64_t chat_id, int64_t message_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if ((chat_id == 0) || (message_id == 0))
		return -1;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	const char *const req = str_set_fmt(&str, "%s/deleteMessage?chat_id=%" PRIi64 "&message_id=%" PRIi64,
					    _base_api, chat_id, message_id);
	char *const resp = http_send_get(req, "application/json");
	if (resp == NULL)
		goto out0;

	json_object *res_obj;
	ret = _parse_response(resp, &res_obj);
	if (ret < 0)
		goto out1;

	json_object_put(res_obj);

out1:
	free(resp);
out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_send_keyboard(const TgApiKeyboard *k, int64_t chat_id, int64_t msg_id, int64_t *ret_id)
{
	assert(_base_api != NULL);

	if (cstr_is_empty(k->value))
		return -1;

	const char *method;
	const char *arg_key;
	const char *caption = NULL;
	int is_edit = 0;
	switch (k->type) {
	case TG_API_KEYBOARD_TYPE_TEXT:
		method = "sendMessage";
		arg_key = "text";
		break;
	case TG_API_KEYBOARD_TYPE_PHOTO:
		method = "sendPhoto";
		arg_key = "photo";
		caption = k->caption;
		break;
	case TG_API_KEYBOARD_TYPE_EDIT_TEXT:
		method = "editMessageText";
		arg_key = "text";
		is_edit = 1;
		break;
	case TG_API_KEYBOARD_TYPE_EDIT_CAPTION:
		method = "editMessageCaption";
		arg_key = "caption";
		caption = k->caption;
		is_edit = 1;
		break;
	default:
		return -1;
	}

	char *const kbd_str = _build_keyboard(k);
	if (kbd_str == NULL)
		return -1;

	int ret = -1;
	char *const value_e = http_url_escape(k->value);
	if (value_e == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out1;

	if (str_set_fmt(&str, "%s/%s?chat_id=%" PRIi64, _base_api, method, chat_id) == NULL)
		goto out2;

	if (is_edit) {
		if (str_append_fmt(&str, "&message_id=%" PRIi64, msg_id) == NULL)
			goto out2;
	} else {
		if ((msg_id != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, msg_id) == NULL))
			goto out2;
	}

	if (cstr_is_empty(caption) == 0) {
		char *const capt_e = http_url_escape(caption);
		if (capt_e != NULL) {
			str_append_fmt(&str, "&caption=%s", capt_e);
			http_url_escape_free(capt_e);
		}
	}

	if (str_append_fmt(&str, "&parse_mode=MarkdownV2&%s=%s", arg_key, value_e) == NULL)
		goto out2;
	if (str_append_fmt(&str, "&reply_markup=%s", kbd_str) == NULL)
		goto out2;

	char *const resp = http_send_get(str.cstr, "application/json");
	if (resp == NULL)
		goto out2;

	json_object *res_obj;
	ret = _parse_response(resp, &res_obj);
	if (ret < 0)
		goto out3;

	ret = _response_get_message_id(res_obj, ret_id);

	json_object_put(res_obj);

out3:
	free(resp);
out2:
	str_deinit(&str);
out1:
	http_url_escape_free(value_e);
out0:
	free(kbd_str);
	return ret;
}


int
tg_api_answer_callback_query(int type, const char id[], const char arg[], int show_alert)
{
	assert(_base_api != NULL);

	int ret = -1;
	const char *arg_key;
	switch (type) {
	case TG_API_ANSWER_CALLBACK_TYPE_TEXT: arg_key = "&text"; break;
	case TG_API_ANSWER_CALLBACK_TYPE_URL: arg_key = "&url"; break;
	default: return ret;
	}

	if (CSTR_IS_EMPTY(id, arg))
		return ret;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_fmt(&str, "%s/answerCallbackQuery?callback_query_id=%s", _base_api, id) == NULL)
		goto out0;

	{
		char *const _new_arg = curl_easy_escape(NULL, arg, 0);
		if (_new_arg == NULL)
			goto out0;

		const char *const _ret = str_append_fmt(&str, "%s=%s", arg_key, _new_arg);

		curl_free(_new_arg);
		if (_ret == NULL)
			goto out0;
	}

	if (str_append_fmt(&str, "&show_alert=%d", show_alert) == NULL)
		goto out0;

	char *const resp = http_send_get(str.cstr, "application/json");
	if (resp == NULL)
		goto out0;

	json_object *res_obj;
	ret = _parse_response(resp, &res_obj);
	if (ret < 0)
		goto out1;

	json_object_put(res_obj);

out1:
	free(resp);
out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_get_admin_list(int64_t chat_id, TgChatAdminList *list, json_object **res_obj)
{
	assert(_base_api != NULL);

	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_fmt(&str, "%s/getChatAdministrators?chat_id=%" PRIi64, _base_api, chat_id) == NULL)
		goto out0;

	char *const resp = http_send_get(str.cstr, "application/json");
	if (resp == NULL)
		goto out0;

	json_object *json;
	if (_parse_response(resp, &json) < 0)
		goto out1;

	if (list != NULL) {
		json_object *_res_obj;
		if (json_object_object_get_ex(json, "result", &_res_obj) == 0)
			goto out2;

		if (tg_chat_admin_list_parse(list, _res_obj) < 0)
			goto out2;
	}

	*res_obj = json;
	ret = 0;

out2:
	if (ret < 0)
		json_object_put(json);

out1:
	free(resp);
out0:
	str_deinit(&str);
	return ret;
}


/*
 * Private
 */
static char *
_build_keyboard(const TgApiKeyboard *k)
{
	char *ret = NULL;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_n(&str, "{\"inline_keyboard\": [", 21) == NULL)
		goto out0;

	const unsigned rows_len = k->rows_len;
	for (unsigned i = 0; i < rows_len; i++) {
		if (str_append_n(&str, "[", 1) == NULL)
			goto out0;

		const TgApiKeyboardButton *const cols = k->rows[i].cols;
		const unsigned cols_len = k->rows[i].cols_len;
		for (unsigned j = 0; j < cols_len; j++) {
			if (_build_keyboard_buttons(&cols[j], &str) < 0)
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

	ret = curl_easy_escape(NULL, str.cstr, (int)str.len);

out0:
	str_deinit(&str);
	return ret;
}


static int
_build_keyboard_buttons(const TgApiKeyboardButton *b, Str *str)
{
	if (str_append_fmt(str, "{\"text\": \"%s\"", b->text) == NULL)
		return -1;

	if (b->data != NULL) {
		if (str_append_n(str, ", \"callback_data\": \"", 20) == NULL)
			return -1;

		const unsigned data_len = b->data_len;
		for (unsigned k = 0; k < data_len; k++) {
			const char *_ret = NULL;
			const TgApiKeyboardButtonData *const d = &b->data[k];
			switch (d->type) {
			case TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT:
				_ret = str_append_fmt(str, "%" PRIi64 " ", d->int_);
				break;
			case TG_API_KEYBOARD_BUTTON_DATA_TYPE_UINT:
				_ret = str_append_fmt(str, "%" PRIu64 " ", d->uint);
				break;
			case TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT:
				_ret = str_append_fmt(str, "%s ", cstr_empty_if_null(d->text));
				break;
			}

			if (_ret == NULL)
				return -1;
		}

		if (data_len > 0)
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
_parse_response(const char resp[], json_object **res_obj)
{
	json_object *const root_obj = json_tokener_parse(resp);
	if (root_obj == NULL)
		return -1;

	json_object *ok_obj;
	if (json_object_object_get_ex(root_obj, "ok", &ok_obj) == 0)
		goto err0;

	if (json_object_get_boolean(ok_obj) == 0)
		goto err0;

	*res_obj = root_obj;
	return 0;

err0:
	json_object_put(root_obj);
	return -1;
}


static int
_response_get_message_id(json_object *obj, int64_t *ret_id)
{
	if (ret_id == NULL)
		return 0;

	json_object *res_obj;
	if (json_object_object_get_ex(obj, "result", &res_obj) == 0)
		return -1;

	json_object *id_obj;
	if (json_object_object_get_ex(res_obj, "message_id", &id_obj) == 0)
		return -1;

	*ret_id = json_object_get_int64(id_obj);
	return 0;
}
