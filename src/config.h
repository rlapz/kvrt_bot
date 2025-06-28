#ifndef __CONFIG_H__
#define __CONFIG_H__


#include <stdint.h>
#include <stddef.h>


/* default */
#define CFG_DEF_LISTEN_HOST         "127.0.0.1"
#define CFG_DEF_LISTEN_PORT         (22224)
#define CFG_DEF_SYS_IMPORT_SYS_ENVP (0)
#define CFG_DEF_SYS_WORKER_SIZE     4
#define CFG_DEF_SYS_DB_PATH         "./db.sqlite"
#define CFG_DEF_CMD_EXTERN_API      "./extern/api"
#define CFG_DEF_CMD_EXTERN_ROOT_DIR "./extern"
#define CFG_DEF_CMD_EXTERN_LOG_FILE "./extern/log.txt"
#define CFG_DEF_DB_CONN_POOL_SIZE   (4)

#define CFG_LOG_BUFFER_SIZE      (1024 * 1024)
#define CFG_BUFFER_SIZE          (1024 * 512)
#define CFG_EVENTS_SIZE          (128)
#define CFG_HTTP_RESPONSE_OK     "HTTP/1.1 200 OK\r\nContent-Length:0\r\n\r\n"
#define CFG_HTTP_RESPONSE_ERROR  "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n"
#define CFG_TELEGRAM_API         "https://api.telegram.org/bot"
#define CFG_MAX_CLIENTS          (128)
#define CFG_LIST_ITEMS_SIZE      (8)
#define CFG_LIST_TIMEOUT_S       (3600)
#define CFG_CONNECTION_TIMEOUT_S (3)
#define CFG_DB_WAIT              (1000)
#define CFG_CHLD_ITEMS_SIZE      (256)
#define CFG_CHLD_ENVP_SIZE       (128)

#define CFG_ENV_API             "TG_API"
#define CFG_ENV_ROOT_DIR        "TG_ROOT_DIR"
#define CFG_ENV_CONFIG_FILE     "TG_CONFIG_FILE"
#define CFG_ENV_TELEGRAM_API    "TG_API_URL"
#define CFG_ENV_OWNER_ID        "TG_OWNER_ID"
#define CFG_ENV_BOT_ID          "TG_BOT_ID"
#define CFG_ENV_BOT_USERNAME    "TG_BOT_USERNAME"

/* config.json */
#define CFG_API_URL_SIZE             (4096)
#define CFG_API_TOKEN_SIZE           (64)
#define CFG_API_SECRET_SIZE          (64)
#define CFG_HOOK_URL_SIZE            (4096)
#define CFG_HOOK_PATH_SIZE           (64)
#define CFG_BOT_USERNAME_SIZE        (64)
#define CFG_LISTEN_HOST_SIZE         (64)
#define CFG_DB_FILE_SIZE             (4096)
#define CFG_CMD_EXTERN_API_SIZE      (4096)
#define CFG_CMD_EXTERN_ROOT_DIR      (4096)
#define CFG_CMD_EXTERN_LOG_FILE_SIZE (4096)


typedef struct config {
	char     api_url[CFG_API_URL_SIZE];
	char     api_token[CFG_API_TOKEN_SIZE];
	char     api_secret[CFG_API_SECRET_SIZE];
	char     hook_url[CFG_HOOK_URL_SIZE];
	char     hook_path[CFG_HOOK_PATH_SIZE];
	int64_t  bot_id;
	int64_t  owner_id;
	char     bot_username[CFG_BOT_USERNAME_SIZE];
	char     listen_host[CFG_LISTEN_HOST_SIZE];
	uint16_t listen_port;
	uint16_t import_sys_envp;
	uint16_t worker_size;
	uint16_t db_pool_conn_size;
	char     db_path[CFG_DB_FILE_SIZE];
	char     cmd_extern_api[CFG_CMD_EXTERN_API_SIZE];
	char     cmd_extern_root_dir[CFG_CMD_EXTERN_ROOT_DIR];
	char     cmd_extern_log_file[CFG_CMD_EXTERN_LOG_FILE_SIZE];
} Config;

int  config_load(Config *c, const char path[]);
void config_dump(const Config *c);


#endif
