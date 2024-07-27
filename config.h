#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>


#define CFG_DEFAULT_LISTEN_HOST        "127.0.0.1"
#define CFG_DEFAULT_LISTEN_PORT        "8007"
#define CFG_DEFAULT_WORKER_THREADS_NUM (8)
#define CFG_DEFAULT_WORKER_JOBS_MIN    (8)
#define CFG_DEFAULT_WORKER_JOBS_MAX    (32)
#define CFG_DEFAULT_DB_FILE            "db.sql"

#define CFG_LOG_BUFFER_SIZE            (1024 * 1024)
#define CFG_CLIENTS_MAX                (32)
#define CFG_CLIENT_BUFFER_IN_MIN       (2048)
#define CFG_CLIENT_BUFFER_IN_MAX       (1024 * 1024)
#define CFG_EVENTS_MAX                 (32)
#define CFG_EVENT_RESPONSE_OK          "HTTP/1.1 200 OK\r\nContent-Length:0\r\n\r\n"
#define CFG_EVENT_RESPONSE_ERR         "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n"
#define CFG_TELEGRAM_API               "https://api.telegram.org/bot"


typedef struct config {
	const char *hook_url;
	size_t      hook_url_len;
	const char *hook_path;
	size_t      hook_path_len;
	const char *listen_host;
	int         listen_port;
	char        api_token[64];
	size_t      api_token_len;
	char        api_secret[256];
	size_t      api_secret_len;
	unsigned    worker_threads_num;
	unsigned    worker_jobs_min;
	unsigned    worker_jobs_max;
	const char *db_file;
	int64_t     owner_id;
} Config;

int  config_load_from_env(Config *c);
void config_dump(const Config *c);


#endif
