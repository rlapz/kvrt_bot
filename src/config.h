#ifndef __CONFIG_H__
#define __CONFIG_H__


#include <stdint.h>
#include <stddef.h>


/* default */
#define CFG_DEF_LISTEN_HOST         "127.0.0.1"
#define CFG_DEF_LISTEN_PORT         (22224)
#define CFG_DEF_SYS_WORKER_SIZE     4
#define CFG_DEF_SYS_DB_FILE         "./db.sqlite"
#define CFG_DEF_CMD_EXTERN_PATH     "./extern"
#define CFG_DEF_CMD_EXTERN_LOG_FILE "./extern/log.txt"
#define CFG_DEF_DB_CONN_POOL_SIZE   (16)

#define CFG_LOG_BUFFER_SIZE           (1024 * 1024)
#define CFG_BUFFER_SIZE               (1024 * 512)
#define CFG_EVENTS_SIZE               (128)
#define CFG_HTTP_RESPONSE_OK          "HTTP/1.1 200 OK\r\nContent-Length:0\r\n\r\n"
#define CFG_HTTP_RESPONSE_ERROR       "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n"
#define CFG_TELEGRAM_API              "https://api.telegram.org/bot"
#define CFG_MAX_CLIENTS               (128)
#define CFG_LIST_ITEMS_SIZE           (8)
#define CFG_LIST_TIMEOUT_S            (3600)
#define CFG_CONNECTION_TIMEOUT_S      (3)
#define CFG_CMD_BUILTIN_MAP_SIZE      (1024)
#define CFG_CHLD_ITEMS_SIZE           (256)
#define CFG_CHLD_ENVP_SIZE            (128)
#define CFG_CHLD_IMPORT_SYS_ENVP      (1)

#define CFG_ENV_ROOT_DIR            "ROOT_DIR"
#define CFG_ENV_TELEGRAM_API        "TG_API"
#define CFG_ENV_TELEGRAM_SECRET_KEY "TG_API_SECRET_KEY"
#define CFG_ENV_CMD_EXTERN_PATH     "CMD_PATH"
#define CFG_ENV_OWNER_ID            "OWNER_ID"
#define CFG_ENV_BOT_ID              "BOT_ID"
#define CFG_ENV_BOT_USERNAME        "BOT_USERNAME"
#define CFG_ENV_DB_PATH             "DB_PATH"

/* config.json */
#define CFG_API_TOKEN_SIZE           (64)
#define CFG_API_SECRET_SIZE          (64)
#define CFG_HOOK_URL_SIZE            (64)
#define CFG_HOOK_PATH_SIZE           (32)
#define CFG_TG_BOT_USERNAME_SIZE     (64)
#define CFG_LISTEN_HOST_SIZE         (64)
#define CFG_SYS_DB_FILE_SIZE         (256)
#define CFG_CMD_EXTERN_PATH_SIZE     (4096)
#define CFG_CMD_EXTERN_LOG_FILE_SIZE (4096)


typedef struct config {
	struct {
		char   token[CFG_API_TOKEN_SIZE];
		size_t token_len;
		char   secret[CFG_API_SECRET_SIZE];
		size_t secret_len;
	} api;
	struct {
		char   url[CFG_HOOK_URL_SIZE];
		size_t url_len;
		char   path[CFG_HOOK_PATH_SIZE];
		size_t path_len;
	} hook;
	struct {
		int64_t bot_id;
		int64_t owner_id;
		char    bot_username[CFG_TG_BOT_USERNAME_SIZE];
	} tg;
	struct {
		char host[CFG_LISTEN_HOST_SIZE];
		int  port;
	} listen;
	struct {
		unsigned worker_size;
		int      db_pool_conn_size;
		char     db_file[CFG_SYS_DB_FILE_SIZE];
	} sys;
	struct {
		char path[CFG_CMD_EXTERN_PATH_SIZE];
		char log_file[CFG_CMD_EXTERN_LOG_FILE_SIZE];
	} cmd_extern;
} Config;

int  config_load_from_json(const char path[], Config **cfg);
void config_dump(const Config *c);


#endif
