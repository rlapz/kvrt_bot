#include <errno.h>
#include <json.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tg_api.h"

#include "config.h"
#include "tg.h"
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
	int ret = str_init_alloc(&t->str, 1024);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_init: str_init_alloc");
		return ret;
	}

	if (_curl_init(t) < 0)
		goto err0;

	t->api = base_api;
	return 0;

err0:
	str_deinit(&t->str);
	return -1;
}


void
tg_api_deinit(TgApi *t)
{
	str_deinit(&t->str);
	_curl_deinit(t);
}


int
tg_api_send_text(TgApi *t, TgApiTextType type, int64_t chat_id, const int64_t *reply_to, const char text[])
{
	int ret = -1;
	char *e_text = NULL;
	const char *req = NULL;


	if (text == NULL) {
		log_err(EINVAL, "tg_api: tg_api_send_text: text null");
		return -1;
	}

	e_text = curl_easy_escape(t->curl, text, 0);
	if (e_text == NULL) {
		log_err(0, "tg_api: tg_api_send_text: curl_easy_escape: text: failed");
		return -1;
	}

	req = str_set_fmt(&t->str, "%s/sendMessage?chat_id=%" PRIi64, t->api, chat_id);
	if (req == NULL)
		goto out1;

	if (reply_to != NULL) {
		req = str_append_fmt(&t->str, "&reply_to_message_id=%" PRIi64, *reply_to);
		if (req == NULL)
			goto out1;
	}

	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN:
		req = str_append_fmt(&t->str, "&text=%s", e_text);
		break;
	case TG_API_TEXT_TYPE_FORMAT:
		req = str_append_fmt(&t->str, "&parse_mode=MarkdownV2&text=%s", e_text);
		break;
	default:
		log_err(0, "tg_api: tg_api_send_text: invalid type option");
		goto out0;
	}

	if (req == NULL) {
out1:
		log_err(errno, "tg_api: tg_api_send_text: str");
		goto out0;
	}

	log_debug("tg_api: tg_api_send_text: request: %s", req);
	ret = _curl_request_get(t, req);

out0:
	free(e_text);
	return ret;
}


int
tg_api_send_photo(TgApi *t, TgApiPhotoType type, int64_t chat_id, const int64_t *reply_to,
		  const char caption[], const char src[])
{
	int ret = -1;
	char *e_src = NULL;
	char *e_caption = NULL;
	const char *req = NULL;


	if (src == NULL) {
		log_err(EINVAL, "tg_api: tg_api_send_photo: src null");
		return -1;
	}

	/* TODO */
	if (type != TG_API_PHOTO_TYPE_LINK) {
		log_err(EINVAL, "tg_api: tg_api_send_photo: photo type: not supported");
		return -1;
	}

	e_src = curl_easy_escape(t->curl, src, 0);
	if (e_src == NULL) {
		log_err(0, "tg_api: tg_api_send_photo: curl_easy_escape: src: failed");
		return -1;
	}

	req = str_set_fmt(&t->str, "%s/sendPhoto?chat_id=%" PRIi64, t->api, chat_id);
	if (req == NULL)
		goto out1;

	if (reply_to != NULL) {
		req = str_append_fmt(&t->str, "&reply_to_message_id=%" PRIi64, *reply_to);
		if (req == NULL)
			goto out1;
	}

	if ((caption != NULL) && (caption[0] != '\0')) {
		e_caption = curl_easy_escape(t->curl, caption, 0);
		if (e_caption == NULL) {
			log_err(0, "tg_api: tg_api_send_photo: curl_easy_escape: caption: failed");
			goto out0;
		}

		req = str_append_fmt(&t->str, "&caption=%s", e_caption);
		if (req == NULL)
			goto out1;
	}

	req = str_append_fmt(&t->str, "&photo=%s", e_src);
	if (req == NULL)
		goto out1;

	if (req == NULL) {
out1:
		log_err(errno, "tg_api: tg_api_send_photo: str");
		goto out0;
	}

	log_debug("tg_api: tg_api_send_photo: request: %s", req);
	ret = _curl_request_get(t, req);

out0:
	free(e_src);
	free(e_caption);
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

	str_reset(&t->str, 0);
	curl_easy_setopt(t->curl, CURLOPT_WRITEDATA, &t->str);

	const CURLcode res = curl_easy_perform(t->curl);
	if (res != CURLE_OK) {
		log_err(0, "tg_api: _curl_request_get: curl_easy_perform: %s", curl_easy_strerror(res));
		return -1;
	}


#ifdef DEBUG
	json_object *const json = json_tokener_parse(t->str.cstr);
	if (json == NULL) {
		log_debug("tg_api: _curl_request_get: response: \n---\n%s\n---", t->str.cstr);
	} else {
		log_debug("tg_api: _curl_request_get: response: \n---\n%s\n---",
			  json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
	}

	json_object_put(json);
#endif
	return 0;
}
