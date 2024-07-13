#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>

#include "tg.h"
#include "tg_api.h"
#include "config.h"
#include "util.h"


static int    _curl_init(TgApi *t);
static void   _curl_deinit(TgApi *t);
static size_t _curl_writer_fn(char buffer[], size_t size, size_t nitems, void *udata);
static int    _curl_request_get(TgApi *t, const char url[]);


/*
 * Public
 */
int
tg_api_init(TgApi *t, const char base_api[])
{
	int ret = str_init_alloc(&t->str_compose, 1024);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_init: str_init_alloc");
		return ret;
	}

	if (str_set_n(&t->str_compose, base_api, strlen(base_api)) == NULL) {
		log_err(errno, "tg_api: tg_api_init: str_set_n: base api: |%s|", base_api);
		goto err0;
	}

	ret = str_init_alloc(&t->str_response, 1024);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_init: str_init_alloc: str_response");
		goto err0;
	}

	if (_curl_init(t) < 0)
		goto err1;

	t->api = base_api;
	t->api_offt = t->str_compose.len;
	return 0;

err1:
	str_deinit(&t->str_response);
err0:
	str_deinit(&t->str_compose);
	return -1;
}


void
tg_api_deinit(TgApi *t)
{
	str_deinit(&t->str_response);
	str_deinit(&t->str_compose);
	_curl_deinit(t);
}


int
tg_api_send_text(TgApi *t, TgApiTextType type, int64_t chat_id, const int64_t *reply_to, const char text[])
{
	int ret = -1;
	const char *req = NULL;


	if (text == NULL) {
		log_err(EINVAL, "tg_api: tg_api_send_text: text null");
		return -1;
	}

	char *const e_text = curl_easy_escape(t->curl, text, 0);
	if (e_text == NULL) {
		log_err(0, "tg_api: tg_api_send_text: curl_easy_escape: failed");
		return -1;
	}

	str_reset(&t->str_compose, t->api_offt);
	req = str_append_fmt(&t->str_compose, "sendMessage?chat_id=%" PRIi64, chat_id);
	if (req == NULL)
		goto out1;

	if (reply_to != NULL) {
		req = str_append_fmt(&t->str_compose, "&reply_to_message_id=%" PRIi64, *reply_to);
		if (req == NULL)
			goto out1;
	}

	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN:
		req = str_append_fmt(&t->str_compose, "&text=%s", e_text);
		break;
	case TG_API_TEXT_TYPE_FORMAT:
		req = str_append_fmt(&t->str_compose, "&parse_mode=MarkdownV2&text=%s", e_text);
		break;
	default:
		log_err(0, "tg_api: tg_api_send_text: invalid type option");
		goto out0;
	}

	if (req == NULL) {
out1:
		log_err(errno, "tg_api: tg_api_send_text_format: str_append_fmt");
		goto out0;
	}

	log_debug("tg_api: tg_api_send_text: request: %s", req);
	ret = _curl_request_get(t, req);

out0:
	free(e_text);
	return ret;
}


/*
 * Private
 */
static int
_curl_init(TgApi *t)
{
	CURL *const curl = curl_easy_init();
	if (curl == NULL) {
		log_err(0, "tg_api: _curl_init: curl_easy_init: failed to init");
		return -1;
	}

	t->curl = curl;
	return 0;
}


static void
_curl_deinit(TgApi *t)
{
	curl_easy_cleanup(t->curl);
}


static size_t
_curl_writer_fn(char buffer[], size_t size, size_t nitems, void *udata)
{
	const size_t real_size = size * nitems;
	if (str_append_n((Str *)udata, buffer, real_size) == NULL) {
		log_err(errno, "tg_api: _curl_writer_fn: str_append_n: failed to append");
		return 0;
	}

	return real_size;
}


static int
_curl_request_get(TgApi *t, const char url[])
{
	curl_easy_reset(t->curl);
	curl_easy_setopt(t->curl, CURLOPT_URL, url);
	curl_easy_setopt(t->curl, CURLOPT_WRITEFUNCTION, _curl_writer_fn);

	str_reset(&t->str_response, 0);
	curl_easy_setopt(t->curl, CURLOPT_WRITEDATA, &t->str_response);

	const CURLcode res = curl_easy_perform(t->curl);
	if (res != CURLE_OK) {
		log_err(0, "tg_api: _curl_request_get: curl_easy_perform: %s", curl_easy_strerror(res));
		return -1;
	}


#ifdef DEBUG
	json_object *const json = json_tokener_parse(t->str_response.cstr);
	if (json == NULL) {
		log_debug("tg_api: _curl_request_get: response: \n---\n%s\n---", t->str_response.cstr);
	} else {
		log_debug("tg_api: _curl_request_get: response: \n---\n%s\n---",
			  json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
	}

	json_object_put(json);
#endif
	return 0;
}
