#include <stdlib.h>

#include "tg_api.h"
#include "config.h"
#include "util.h"


#define _COMPOSE_FMT(PTR, RET, ...)\
	do {								\
		RET = 0;						\
		str_reset(&PTR->str, PTR->api_offt);			\
		if (str_append_fmt(&PTR->str, __VA_ARGS__) == NULL)	\
			RET = -1;					\
	} while (0)


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

	if (str_set_fmt(&t->str, "%s", base_api) == NULL) {
		log_err(ret, "tg_api: tg_api_init: str_set_fmt: base api");
		goto err0;
	}

	if (_curl_init(t) < 0)
		goto err0;

	t->api = base_api;
	t->api_offt = t->str.len;
	return 0;

err0:
	str_deinit(&t->str);
	return -1;
}


void
tg_api_deinit(TgApi *t)
{
	_curl_deinit(t);
	str_deinit(&t->str);
}


int
tg_api_send_text_plain(TgApi *t, const char chat_id[], const char reply_to[], const char text[])
{
	int ret;
	char *const e_text = curl_easy_escape(t->curl, text, 0);
	if (e_text == NULL)
		return -1;

	_COMPOSE_FMT(t, ret, "sendMessage?chat_id=%s&reply_to_message_id=%s&text=%s",
		     chat_id, reply_to, e_text);

	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_send_text_plain: _COMPOSE_FMT");
		goto out0;
	}

	ret = _curl_request_get(t, t->str.cstr);

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
	return 0;
}


static int
_curl_request_get(TgApi *t, const char url[])
{
	curl_easy_reset(t->curl);
	curl_easy_setopt(t->curl, CURLOPT_URL, url);
	curl_easy_setopt(t->curl, CURLOPT_WRITEFUNCTION, _curl_writer_fn);
	curl_easy_setopt(t->curl, CURLOPT_WRITEDATA, t);

	const CURLcode res = curl_easy_perform(t->curl);
	if (res != CURLE_OK) {
		log_err(0, "tg_api: _curl_request_get: curl_easy_perform: %s", curl_easy_strerror(res));
		return -1;
	}

	return 0;
}
