#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>


#define CFG_DEFAULT_LISTEN_HOST        "127.0.0.1"
#define CFG_DEFAULT_LISTEN_PORT        "8007"
#define CFG_DEFAULT_WORKER_THREADS_NUM (8)
#define CFG_DEFAULT_WORKER_JOBS_MIN    (8)
#define CFG_DEFAULT_WORKER_JOBS_MAX    (32)
#define CFG_DEFAULT_DB_FILE            "db.sql"
#define CFG_DEFAULT_CMD_PATH           "./extern/"

#define CFG_API_TOKEN_SIZE     (64)
#define CFG_API_SECRET_SIZE    (256)
#define CFG_LOG_BUFFER_SIZE    (1024 * 1024)
#define CFG_CLIENTS_MIN_SIZE   (8)
#define CFG_CLIENTS_MAX_SIZE   (128)
#define CFG_CLIENT_BUFFER_SIZE (8192)
#define CFG_EVENTS_SIZE        (32)
#define CFG_EVENT_RESPONSE_OK  "HTTP/1.1 200 OK\r\nContent-Length:0\r\n\r\n"
#define CFG_EVENT_RESPONSE_ERR "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n"
#define CFG_TELEGRAM_API       "https://api.telegram.org/bot"
#define CFG_MSG_SPECIAL_CHARS  "_*[]()~`>#+-=|{}.!"

#define CFG_ENV_TELEGRAM_API    "TG_API"
#define CFG_ENV_CMD_EXTERN_PATH "CMD_PATH"
#define CFG_ENV_OWNER_ID        "OWNER_ID"
#define CFG_ENV_BOT_ID          "BOT_ID"
#define CFG_ENV_DB_PATH         "DB_PATH"

#define CFG_UTIL_BOT_CMD_ARGS_SIZE         (16)
#define CFG_UTIL_CALLBACK_QUERY_ITEMS_SIZE (8)
#define CFG_UTIL_CHLD_ITEMS_SIZE           (256)

#define CFG_ITEM_LIST_SIZE (10)


typedef struct config {
	const char *hook_url;
	size_t      hook_url_len;
	const char *hook_path;
	size_t      hook_path_len;
	const char *listen_host;
	int         listen_port;
	char        api_token[CFG_API_TOKEN_SIZE];
	size_t      api_token_len;
	char        api_secret[CFG_API_SECRET_SIZE];
	size_t      api_secret_len;
	unsigned    worker_threads_num;
	unsigned    worker_jobs_min;
	unsigned    worker_jobs_max;
	const char *db_file;
	int64_t     bot_id;
	int64_t     owner_id;
	const char *cmd_path; /* external command path */
} Config;

int  config_load_from_env(Config *c);
void config_dump(const Config *c);


#endif
