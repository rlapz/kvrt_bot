#include <assert.h>
#include <inttypes.h>

#include <curl/curl.h>

#include "tg_api.h"

#include "tg.h"
#include "util.h"


static const char *_base_api = NULL;


static int _response_get_message_id(const char resp[], int64_t *ret_id);
static int _build_inline_keyboard(const TgApiInlineKeyboard kbds[], unsigned kbds_len,
				  char *ret_str[]);
static int _parse_response(json_object *root, json_object **res_obj);


/*
 * Public
 */
void
tg_api_init(const char base_api[])
{
	_base_api = base_api;
}


/* TODO: error handler */
int
tg_api_send_text(int type, int64_t chat_id, const int64_t *reply_to, const char text[], int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (cstr_is_empty(text))
		return ret;

	Str str;
	str_init_alloc(&str, 0);
	str_set_fmt(&str, "%s/sendMessage?chat_id=%" PRIi64, _base_api, chat_id);

	if (reply_to != NULL)
		str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, *reply_to);

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN:
		str_append_fmt(&str, "&text=%s", new_text);
		break;
	case TG_API_TEXT_TYPE_FORMAT:
		str_append_fmt(&str, "&parse_mode=MarkdownV2&text=%s", new_text);
		break;
	default:
		goto out0;
	}

	char *const resp = http_send_get(str.cstr, "application/json");
	if (ret_id != NULL)
		ret = _response_get_message_id(resp, ret_id);
	else
		ret = 0;

	free(resp);
out0:
	curl_free(new_text);
	str_deinit(&str);
	return ret;
}


int
tg_api_delete_message(int64_t chat_id, int64_t message_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if ((chat_id == 0) || (message_id == 0))
		return ret;

	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return -1;

	str_set_fmt(&str, "%s/deleteMessage?chat_id=%" PRIi64 "&message_id=%" PRIi64, _base_api,
		    chat_id, message_id);

	char *const resp = http_send_get(str.cstr, "application/json");
	if  (resp == NULL)
		goto out0;

	free(resp);
	ret = 0;

out0:
	str_deinit(&str);
	return ret;
}


int
tg_api_send_inline_keyboard(int64_t chat_id, const int64_t *reply_to, const char text[],
			    const TgApiInlineKeyboard kbds[], size_t kbds_len,
			    int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (cstr_is_empty(text))
		return ret;

	char *ret_str = NULL;
	if (_build_inline_keyboard(kbds, (unsigned)kbds_len, &ret_str) < 0)
		return -1;

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 0) < 0)
		goto out1;

	str_set_fmt(&str, "%s/sendMessage?chat_id=%" PRIi64, _base_api, chat_id);
	if (reply_to != NULL)
		str_append_fmt(&str, "&reply_to_message_id=%" PRIi64, *reply_to);
	str_append_fmt(&str, "&parse_mode=MarkdownV2&text=%s&reply_markup=%s", new_text, ret_str);

	char *const resp = http_send_get(str.cstr, "application/json");
	if (ret_id != NULL)
		ret = _response_get_message_id(resp, ret_id);
	else
		ret = 0;

	free(resp);
	str_deinit(&str);

out1:
	curl_free(new_text);
out0:
	curl_free(ret_str);
	return ret;
}


int
tg_api_edit_inline_keyboard(int64_t chat_id, int64_t msg_id, const char text[],
			    const TgApiInlineKeyboard kbds[], size_t kbds_len,
			    int64_t *ret_id)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (cstr_is_empty(text))
		return ret;

	char *ret_str = NULL;
	if (_build_inline_keyboard(kbds, (unsigned)kbds_len, &ret_str) < 0)
		return ret;

	char *const new_text = curl_easy_escape(NULL, text, 0);
	if (new_text == NULL)
		goto out0;

	Str str;
	if (str_init_alloc(&str, 0) < 0)
		goto out1;

	str_set_fmt(&str, "%s/editMessageText?chat_id=%" PRIi64, _base_api, chat_id);
	str_append_fmt(&str, "&message_id=%" PRIi64 "&parse_mode=MarkdownV2&text=%s", msg_id, new_text);
	str_append_fmt(&str, "&reply_markup=%" PRIi64, ret_str);

	char *const resp = http_send_get(str.cstr, "application/json");
	if (ret_id != NULL)
		ret = _response_get_message_id(resp, ret_id);
	else
		ret = 0;

	free(resp);
	str_deinit(&str);

out1:
	curl_free(new_text);
out0:
	curl_free(ret_str);
	return ret;
}


int
tg_api_answer_callback_query(const char id[], const char text[], const char url[], int show_alert)
{
	assert(_base_api != NULL);

	int ret = -1;
	if (CSTR_IS_EMPTY(id, text, url))
		return ret;

	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return ret;

	str_set_fmt(&str, "%s/answerCallbackQuery?callback_query_id=%s", _base_api, id);
	if (text != NULL) {
		char *const new_text = curl_easy_escape(NULL, text, 0);
		if (new_text == NULL)
			goto out0;

		str_append_fmt(&str, "&text=%s", new_text);
		curl_free(new_text);
	}

	if (url != NULL) {
		char *const new_url = curl_easy_escape(NULL, url, 0);
		if (new_url == NULL)
			goto out0;

		str_append_fmt(&str, "&url=%s", new_url);
		curl_free(new_url);
	}

	str_append_fmt(&str, "&show_alert=%d", show_alert);

	char *const resp = http_send_get(str.cstr, "application/json");
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
	if (str_init_alloc(&str, 0) < 0)
		return ret;

	const char *const req = str_set_fmt(&str, "%s/getChatAdministrators?chat_id=%" PRIi64,
					    _base_api, chat_id);
	if (req == NULL)
		goto out0;

	char *const resp = http_send_get(req, "application/json");
	if (resp == NULL)
		goto out0;

	json_object *const json = json_tokener_parse(resp);
	if (list != NULL) {
		json_object *res;
		if (_parse_response(json, &res) < 0)
			goto out1;

		if (tg_chat_admin_list_parse(list, res) < 0)
			goto out1;
	}

	*res_obj = json;
	ret = 0;

out1:
	if (ret < 0)
		json_object_put(json);

	free(resp);
out0:
	str_deinit(&str);
	return ret;
}


/*
 * Private
 */
static int
_response_get_message_id(const char resp[], int64_t *ret_id)
{
	int ret = -1;
	if (resp == NULL)
		return ret;

	json_object *const root_obj = json_tokener_parse(resp);
	if (root_obj == NULL)
		return ret;

	dump_json_obj("tg_api: _response_get_message_id", root_obj);

	json_object *ok_obj;
	if (json_object_object_get_ex(root_obj, "ok", &ok_obj) == 0)
		goto out0;

	const int is_ok = json_object_get_boolean(ok_obj);
	if (is_ok != 1)
		goto out0;

	json_object *result_obj;
	if (json_object_object_get_ex(root_obj, "result", &result_obj) == 0)
		goto out0;

	json_object *id_obj;
	if (json_object_object_get_ex(result_obj, "message_id", &id_obj) == 0)
		goto out0;

	*ret_id = json_object_get_int64(id_obj);
	ret = 0;

out0:
	json_object_put(root_obj);
	return ret;
}


static int
_build_inline_keyboard(const TgApiInlineKeyboard kbds[], unsigned kbds_len, char *ret_str[])
{
	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return -1;

	str_set_fmt(&str, "{\"inline_keyboard\": [");
	for (unsigned i = 0; i < kbds_len; i++) {
		str_append_n(&str, "[", 1);

		const TgApiInlineKeyboard *const kbd = &kbds[i];
		for (unsigned j = 0; j < kbd->len; j++) {
			const TgApiInlineKeyboardButton *const btn = kbd[i].buttons;
			str_append_fmt(&str, "{\"text\": \"%s\"", btn->text);

			if (btn->data != NULL) {
				str_append_fmt(&str, ", \"callback_data\": \"");

				for (unsigned k = 0; k < btn->data_len; k++) {
					const TgApiInlineKeyboardButtonData *const d = &btn->data[k];
					switch (d->type) {
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT:
						str_append_fmt(&str, "%" PRIi64 " ", d->int_);
						break;
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_UINT:
						str_append_fmt(&str, "%" PRIu64 " ", d->uint);
						break;
					case TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT:
						str_append_fmt(&str, "%s ", d->text);
						break;
					}
				}

				if (btn->data_len > 0)
					str_pop(&str, 1);

				str_append_n(&str, "\"", 1);
			} else if (btn->url != NULL) {
				str_append_fmt(&str, ", \"url\": \"%s\"", btn->url);
			}

			str_append_n(&str, "}, ", 3);
		}

		if (kbd->len > 0)
			str_pop(&str, 2);

		str_append_n(&str, "], ", 3);
	}

	if (kbds_len > 0)
		str_pop(&str, 2);

	str_append_n(&str, "]}", 2);

	*ret_str = curl_easy_escape(NULL, str.cstr, (int)str.len);
	str_deinit(&str);

	return 0;
}


static int
_parse_response(json_object *root, json_object **res_obj)
{
	json_object *ok_obj;
	if (json_object_object_get_ex(root, "ok", &ok_obj) == 0)
		return -1;

	const int is_ok = json_object_get_boolean(ok_obj);
	if (is_ok == 0)
		return -1;

	if (json_object_object_get_ex(root, "result", res_obj) == 0)
		return -1;

	return 0;
}
