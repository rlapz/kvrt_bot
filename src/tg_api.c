#include <assert.h>
#include <inttypes.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_api = NULL;


static int _build_inline_keyboard(const TgApiInlineKeyboard kbds[], unsigned kbds_len,
				  char *ret_str[]);
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
tg_api_send_inline_keyboard(int64_t chat_id, int64_t reply_to, const char text[],
			    const TgApiInlineKeyboard kbds[], unsigned kbds_len,
			    int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (cstr_is_empty(text))
		return ret;

	char *ret_str = NULL;
	if (_build_inline_keyboard(kbds, kbds_len, &ret_str) < 0)
		return -1;

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out1;

	if (str_set_fmt(&str, "%s/sendMessage?chat_id=%" PRIi64, _base_api, chat_id) == NULL)
		goto out2;

	if ((reply_to != 0) && (str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, reply_to) == NULL))
		goto out2;

	if (str_append_fmt(&str, "&parse_mode=MarkdownV2&text=%s&reply_markup=%s", new_text, ret_str) == NULL)
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
	curl_free(new_text);
out0:
	curl_free(ret_str);
	return ret;
}


int
tg_api_edit_inline_keyboard(int64_t chat_id, int64_t msg_id, const char text[],
			    const TgApiInlineKeyboard kbds[], unsigned kbds_len,
			    int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (cstr_is_empty(text))
		return ret;

	char *ret_str = NULL;
	if (_build_inline_keyboard(kbds, kbds_len, &ret_str) < 0)
		return ret;

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out1;

	if (str_set_fmt(&str, "%s/editMessageText?chat_id=%" PRIi64, _base_api, chat_id) == NULL)
		goto out2;
	if (str_append_fmt(&str, "&message_id=%" PRIi64 "&parse_mode=MarkdownV2&text=%s", msg_id, new_text) == NULL)
		goto out2;
	if (str_append_fmt(&str, "&reply_markup=%s", ret_str) == NULL)
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
	curl_free(new_text);
out0:
	curl_free(ret_str);
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
static int
_build_inline_keyboard(const TgApiInlineKeyboard kbds[], unsigned kbds_len, char *ret_str[])
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	if (str_set_n(&str, "{\"inline_keyboard\": [", 21) == NULL)
		goto out0;

	for (unsigned i = 0; i < kbds_len; i++) {
		if (str_append_n(&str, "[", 1) == NULL)
			goto out0;

		const TgApiInlineKeyboard *const kbd = &kbds[i];
		for (unsigned j = 0; j < kbd->len; j++) {
			const TgApiInlineKeyboardButton *const btn = &kbd->buttons[j];
			if (str_append_fmt(&str, "{\"text\": \"%s\"", btn->text) == NULL)
				goto out0;

			if (btn->data != NULL) {
				if (str_append_n(&str, ", \"callback_data\": \"", 20) == NULL)
					goto out0;

				for (unsigned k = 0; k < btn->data_len; k++) {
					const char *_ret = NULL;
					const TgApiInlineKeyboardButtonData *const d = &btn->data[k];
					switch (d->type) {
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT:
						_ret = str_append_fmt(&str, "%" PRIi64 " ", d->int_);
						break;
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_UINT:
						_ret = str_append_fmt(&str, "%" PRIu64 " ", d->uint);
						break;
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT:
						_ret = str_append_fmt(&str, "%s ", cstr_empty_if_null(d->text));
						break;
					}

					if (_ret == NULL)
						goto out0;
				}

				if (btn->data_len > 0)
					str_pop(&str, 1);

				if (str_append_c(&str, '"') == NULL)
					goto out0;
			} else if (btn->url != NULL) {
				if (str_append_fmt(&str, ", \"url\": \"%s\"", btn->url) == NULL)
					goto out0;
			}

			if (str_append_n(&str, "}, ", 3) == NULL)
				goto out0;
		}

		if (kbd->len > 0)
			str_pop(&str, 2);

		if (str_append_n(&str, "], ", 3) == NULL)
			goto out0;
	}

	if (kbds_len > 0)
		str_pop(&str, 2);

	if (str_append_n(&str, "]}", 2) == NULL)
		goto out0;

	char *const res = curl_easy_escape(NULL, str.cstr, (int)str.len);
	if (res != NULL) {
		*ret_str = res;
		ret = 0;
	}

out0:
	str_deinit(&str);
	return ret;
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
