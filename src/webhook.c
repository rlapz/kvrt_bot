#include <errno.h>
#include <json.h>
#include <stdio.h>

#include "webhook.h"

#include "util.h"


#define ALLOWED_UPDATES "[\"message\",\"callback_query\",\"inline_query\"]"


static void _print_json(const char raw[]);


int
webhook_set(const Config *config)
{
	LOG_INFO("webhook", "%s", "setting up webhook...");
	LOG_INFO("webhook", "url: \"%s%s\"", config->hook_url, config->hook_path);

	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		LOG_ERR(ret, "webhook", "%s", "str_init_alloc");
		return -1;
	}

	const char *const req = str_set_fmt(&str, "%s%s/setWebhook?url=%s%s&allowed_updates="
						  ALLOWED_UPDATES "&drop_pending_updates=True&secret_token=%s",
					    CFG_TELEGRAM_API, config->api_token, config->hook_url,
					    config->hook_path, config->api_secret);
	if (req == NULL) {
		LOG_ERRP("webhook", "%s", "str_set_fmt: NULL");
		goto out0;
	}

	char *const res = http_send_get(req, "application/json");
	if (res == NULL) {
		LOG_ERRN("webhook", "%s", "http_send_get: failed");
		goto out0;
	}

	_print_json(res);
	ret = 0;

	free(res);

out0:
	str_deinit(&str);
	return ret;
}


int
webhook_del(const Config *config)
{
	LOG_INFO("webhook", "%s", "deleting webhook...");

	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		LOG_ERR(ret, "webhook", "%s", "str_init_alloc");
		return -1;
	}

	const char *const req = str_set_fmt(&str, "%s%s/deleteWebhook?drop_pending_updates=True",
					    CFG_TELEGRAM_API, config->api_token);
	if (req == NULL) {
		LOG_ERRP("webhook", "%s", "str_set_fmt: NULL");
		goto out0;
	}

	char *const res = http_send_get(req, "application/json");
	if (res == NULL) {
		LOG_ERRN("webhook", "%s", "http_send_get: failed");
		goto out0;
	}

	_print_json(res);
	ret = 0;

	free(res);

out0:
	str_deinit(&str);
	return ret;
}


int
webhook_info(const Config *config)
{
	LOG_INFO("webhook", "%s", "");

	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		LOG_ERR(ret, "webhook", "%s", "str_init_alloc");
		return -1;
	}

	const char *const req = str_set_fmt(&str, "%s%s/getWebhookInfo",
					    CFG_TELEGRAM_API, config->api_token);
	if (req == NULL) {
		LOG_ERRP("webhook", "%s", "str_set_fmt: NULL");
		goto out0;
	}

	char *const res = http_send_get(req, "application/json");
	if (res == NULL) {
		LOG_ERRN("webhook", "%s", "http_send_get: failed");
		goto out0;
	}

	_print_json(res);
	ret = 0;

	free(res);

out0:
	str_deinit(&str);
	return ret;
}


/*
 * Private
 */
static void
_print_json(const char raw[])
{
	json_object *const json = json_tokener_parse(raw);
	if (json == NULL)
		return;

	const char *const json_str = json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY);
	if (json_str == NULL)
		goto out0;

	puts(json_str);

out0:
	json_object_put(json);
}
