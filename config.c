#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "util.h"


/*
 * Public
 */
int
config_load_from_env(Config *c)
{
	const char *env = getenv("KVRT_BOT_API_TOKEN");
	if ((env == NULL) || (env[0] == '\0')) {
		log_err(0, "config: config_load_from_env: no 'API_TOKEN'");
		return -1;
	}

	cstr_copy_n(c->api_token, sizeof(c->api_token), env);
	if (unsetenv("KVRT_BOT_API_TOKEN") < 0)
		log_err(errno, "config: config_load_from_env: unsetenv: 'API_TOKEN'");


	if (((env = getenv("KVRT_BOT_API_SECRET")) == NULL) || (env[0] == '\0')) {
		log_err(0, "config: config_load_from_env: no 'API_SECRET'");
		return -1;
	}

	cstr_copy_n(c->api_secret, sizeof(c->api_secret), env);
	if (unsetenv("KVRT_BOT_API_SECRET") < 0)
		log_err(errno, "config: config_load_from_env: unsetenv: 'API_SECRET'");


	if (((env = getenv("KVRT_BOT_HOOK_URL")) == NULL) || (env[0] == '\0')) {
		log_err(0, "config: config_load_from_env: no 'HOOK_URL'");
		return -1;
	}
	c->hook_url = env;

	if (((env = getenv("KVRT_BOT_HOOK_PATH")) == NULL) || (env[0] == '\0')) {
		log_err(0, "config: config_load_from_env: no 'HOOK_PATH'");
		return -1;
	}
	c->hook_path = env;

	if (((env = getenv("KVRT_BOT_LISTEN_HOST")) != NULL) && (env[0] != '\0'))
		c->listen_host = env;
	else
		c->listen_host = CFG_DEFAULT_LISTEN_HOST;
	
	const char *port;
	if (((env = getenv("KVRT_BOT_LISTEN_PORT")) != NULL) && (env[0] != '\0'))
		port = env;
	else
		port = CFG_DEFAULT_LISTEN_PORT;
	
	c->listen_port = atoi(port);

	if (((env = getenv("KVRT_BOT_WORKER_THREADS_NUM")) != NULL) && (env[0] != '\0')) {
		if (env[0] == '-')
			env = "0";
		c->worker_threads_num = (unsigned)atoi(env);
	} else {
		c->worker_threads_num = CFG_DEFAULT_WORKER_THREADS_NUM;
	}

	if (((env = getenv("KVRT_BOT_WORKER_JOBS_MIN")) != NULL) && (env[0] != '\0')) {
		if (env[0] == '-')
			env = "0";
		c->worker_jobs_min = (unsigned)atoi(env);
	} else {
		c->worker_jobs_min = CFG_DEFAULT_WORKER_JOBS_MIN;
	}

	if (((env = getenv("KVRT_BOT_WORKER_JOBS_MAX")) != NULL) && (env[0] != '\0')) {
		if (env[0] == '-')
			env = "0";
		c->worker_jobs_max = (unsigned)atoi(env);
	} else {
		c->worker_jobs_max = CFG_DEFAULT_WORKER_JOBS_MAX;
	}


	c->hook_url_len = strlen(c->hook_url);
	c->hook_path_len = strlen(c->hook_path);
	c->api_token_len = strlen(c->api_token);
	c->api_secret_len = strlen(c->api_secret);
	return 0;
}


void
config_dump(const Config *c)
{
	puts("---[CONFIG]---");

#ifdef DEBUG
	printf("Api Token      : %s\n", c->api_token);
	printf("Api Secret     : %s\n", c->api_secret);
#else
	printf("Api Token      : *****************\n");
	printf("Api Secret     : *****************\n");
#endif

	printf("Hook URL       : %s%s\n", c->hook_url, c->hook_path);

	printf("Listen Host    : %s\n", c->listen_host);
	printf("Listen Port    : %d\n", c->listen_port);
	printf("Worker Threads : %u\n", c->worker_threads_num);
	printf("Worker Jobs Min: %u\n", c->worker_jobs_min);
	printf("Worker Jobs Max: %u\n", c->worker_jobs_max);
	puts("---[CONFIG]---");
}
