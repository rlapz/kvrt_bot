#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <json.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "util.h"


static int  _load_json(const char path[]);
static int  _parse_json(char buffer[], size_t size, const char path[]);
static int  _parse_json_api(Config *c, json_object *root_obj);
static int  _parse_json_hook(Config *c, json_object *root_obj);
static int  _parse_json_tg(Config *c, json_object *root_obj);
static void _parse_json_sys(Config *c, json_object *root_obj);
static void _parse_json_listen(Config *c, json_object *root_obj);
static void _parse_json_cmd_extern(Config *c, json_object *root_obj);


/*
 * Public
 * TODO: validate all macro values
 */
int
config_load(Config *c, const char path[])
{
	if (CFG_LIST_ITEMS_SIZE & 1) {
		log_err(EINVAL, "config: config_load: invalid 'CFG_LIST_ITEMS_SIZE' value, must an even number!");
		return -1;
	}

	if (cstr_is_empty(path)) {
		log_err(EINVAL, "config: config_load: path is empty");
		return -1;
	}

	for (int i = 0; i < 3; i++) {
		size_t cfg_len = sizeof(*c);
		const int ret = file_read_all(path, (char *)c, &cfg_len);
		if ((ret == 0) && (cfg_len == sizeof(*c)))
			return 0;

		if ((ret == 0) || (ret == -ENOENT)) {
			if (_load_json(path) < 0)
				return -1;

			continue;
		}

		log_err(errno, "config: config_load: open: '%s'", path);
		break;
	}

	return -1;
}


void
config_dump(const Config *c)
{
	puts("---[CONFIG]---");

#ifdef DEBUG
	printf("Api Token            : %s\n", c->api_token);
	printf("Api Secret           : %s\n", c->api_secret);
#else
	printf("Api Token            : *****************\n");
	printf("Api Secret           : *****************\n");
#endif

	printf("Hook URL             : %s%s\n", c->hook_url, c->hook_path);

	printf("Listen Host          : %s\n", c->listen_host);
	printf("Listen Port          : %u\n", c->listen_port);
	printf("Worker Size          : %u\n", c->worker_size);
	printf("Child Process Max    : %u\n", CFG_CHLD_ITEMS_SIZE);
	printf("DB path              : %s\n", c->db_path);
	printf("DB pool connections  : %d\n", c->db_pool_conn_size);
	printf("Owner ID             : %" PRIi64 "\n", c->owner_id);
	printf("Bot ID               : %" PRIi64 "\n", c->bot_id);
	printf("Bot username         : %s\n", c->bot_username);
	printf("External cmd path    : %s\n", c->cmd_extern_path);
	printf("External cmd log file: %s\n", c->cmd_extern_log_file);
	puts("---[CONFIG]---");
}


/*
 * Private
 */
static int
_load_json(const char path[])
{
	char buffer[CFG_BUFFER_SIZE];
	size_t buffer_len = LEN(buffer);

	/* finding .bin extension */
	const char *const ext = strrchr(path, '.');
	if ((ext != NULL) && (cstr_casecmp(ext, ".bin") == 0)) {
		log_err(0, "config: _load_json: '%s': invalid file path", path);
		return -1;
	}

	char *const json_path = buffer;
	cstr_copy_n(json_path, (ext - path) + 1, path);

	int ret = file_read_all(json_path, buffer, &buffer_len);
	if (ret < 0) {
		log_err(ret, "config: _load_json: file_read_all: '%s'", json_path);
		return -1;
	}

	buffer[buffer_len] = '\0';
	return _parse_json(buffer, LEN(buffer), path);
}


static int
_parse_json(char buffer[], size_t size, const char path[])
{
	Config *const cfg = (Config *)buffer;
	size_t cfg_len = sizeof(*cfg);
	assert(cfg_len <= size);

	int ret = -1;
	json_object *const root_obj = json_tokener_parse(buffer);
	if (root_obj == NULL) {
		log_err(0, "config: _parse_json: json_tokener_parse: failed");
		return -1;
	}

#ifdef DEBUG
	memset(buffer, 0xa, size);
#endif

	if (_parse_json_api(cfg, root_obj) < 0)
		goto out0;
	if (_parse_json_hook(cfg, root_obj) < 0)
		goto out0;
	if (_parse_json_tg(cfg, root_obj) < 0)
		goto out0;

	_parse_json_sys(cfg, root_obj);
	_parse_json_listen(cfg, root_obj);
	_parse_json_cmd_extern(cfg, root_obj);

	ret = file_write_all(path, buffer, &cfg_len);
	if ((ret == 0) && (cfg_len != sizeof(*cfg))) {
		log_err(0, "config: _parse_json: file_write_all: '%s': invalid len", path);
		ret = -1;
	}

out0:
	json_object_put(root_obj);
	return ret;
}


static int
_parse_json_api(Config *c, json_object *root_obj)
{
	json_object *api_obj;
	if (json_object_object_get_ex(root_obj, "api", &api_obj) == 0) {
		log_err(0, "config: _parse_json_api: api: no such object");
		return -1;
	}

	json_object *token_obj;
	if (json_object_object_get_ex(api_obj, "token", &token_obj) == 0) {
		log_err(0, "config: _parse_json_api: api.token: no such object");
		return -1;
	}

	json_object *secret_obj;
	if (json_object_object_get_ex(api_obj, "secret", &secret_obj) == 0) {
		log_err(0, "config: _parse_json_api: api.secret: no such object");
		return -1;
	}

	const char *const token = json_object_get_string(token_obj);
	if (cstr_is_empty(token)) {
		log_err(0, "config: _parse_json_api: api.token: empty");
		return -1;
	}

	const char *const secret = json_object_get_string(secret_obj);
	if (cstr_is_empty(secret)) {
		log_err(0, "config: _parse_json_api: api.secret: empty");
		return -1;
	}

	char *const base_api = CSTR_CONCAT(CFG_TELEGRAM_API, token);
	if (base_api == NULL) {
		log_err(0, "config: _parse_json_api: api.url: failed to allocate");
		return -1;
	}

	cstr_copy_n(c->api_url, LEN(c->api_url), base_api);
	cstr_copy_n(c->api_token, LEN(c->api_token), token);
	cstr_copy_n(c->api_secret, LEN(c->api_secret), secret);

	free(base_api);
	return 0;
}


static int
_parse_json_hook(Config *c, json_object *root_obj)
{
	json_object *hook_obj;
	if (json_object_object_get_ex(root_obj, "hook", &hook_obj) == 0) {
		log_err(0, "config: _parse_json_hook: hook: no such object");
		return -1;
	}

	json_object *url_obj;
	if (json_object_object_get_ex(hook_obj, "url", &url_obj) == 0) {
		log_err(0, "config: _parse_json_api: hook.url: no such object");
		return -1;
	}

	json_object *path_obj;
	if (json_object_object_get_ex(hook_obj, "path", &path_obj) == 0) {
		log_err(0, "config: _parse_json_api: hook.path: no such object");
		return -1;
	}

	const char *const url = json_object_get_string(url_obj);
	if (cstr_is_empty(url)) {
		log_err(0, "config: _parse_json_api: hook.url: empty");
		return -1;
	}

	const char *const path = json_object_get_string(path_obj);
	if (cstr_is_empty(path)) {
		log_err(0, "config: _parse_json_api: hook.path: empty");
		return -1;
	}

	cstr_copy_n(c->hook_url, LEN(c->hook_url), url);
	cstr_copy_n(c->hook_path, LEN(c->hook_path), path);
	return 0;
}


static int
_parse_json_tg(Config *c, json_object *root_obj)
{
	json_object *tg_obj;
	if (json_object_object_get_ex(root_obj, "tg", &tg_obj) == 0) {
		log_err(0, "config: _parse_json_tg: tg: no such object");
		return -1;
	}

	json_object *bot_id_obj;
	if (json_object_object_get_ex(tg_obj, "bot_id", &bot_id_obj) == 0) {
		log_err(0, "config: _parse_json_tg: tg.bot_id: no such object");
		return -1;
	}

	json_object *owner_id_obj;
	if (json_object_object_get_ex(tg_obj, "owner_id", &owner_id_obj) == 0) {
		log_err(0, "config: _parse_json_tg: tg.owner_id: no such object");
		return -1;
	}

	json_object *bot_username_obj;
	if (json_object_object_get_ex(tg_obj, "bot_username", &bot_username_obj) == 0) {
		log_err(0, "config: _parse_json_tg: tg.bot_username: no such object");
		return -1;
	}

	c->bot_id = json_object_get_int64(bot_id_obj);
	if (c->bot_id == 0) {
		log_err(0, "config: _parse_json_tg: tg.bot_id: invalid value");
		return -1;
	}

	c->owner_id = json_object_get_int64(owner_id_obj);
	if (c->owner_id == 0) {
		log_err(0, "config: _parse_json_tg: tg.owner_id: invalid value");
		return -1;
	}

	const char *const bot_username = json_object_get_string(bot_username_obj);
	if (cstr_is_empty(bot_username)) {
		log_err(0, "config: _parse_json_tg: tg.bot_username: empty");
		return -1;
	}

	cstr_copy_n(c->bot_username, LEN(c->bot_username), bot_username);
	return 0;
}


static void
_parse_json_sys(Config *c, json_object *root_obj)
{
	uint16_t import_envp = CFG_DEF_SYS_IMPORT_SYS_ENVP;
	uint16_t worker_size = CFG_DEF_SYS_WORKER_SIZE;
	const char *db_file = CFG_DEF_SYS_DB_PATH;
	uint16_t db_pool_conn_size = CFG_DEF_DB_CONN_POOL_SIZE;

	json_object *sys_obj;
	if (json_object_object_get_ex(root_obj, "sys", &sys_obj) == 0)
		goto out0;

	json_object *tmp_obj;
	if (json_object_object_get_ex(sys_obj, "import_sys_envp", &tmp_obj) != 0) {
		const char *const bool_type = (const char *)json_object_get_string(tmp_obj);
		import_envp = cstr_to_bool(bool_type);
	}

	if (json_object_object_get_ex(sys_obj, "worker_size", &tmp_obj) != 0)
		worker_size = (unsigned)json_object_get_uint64(tmp_obj);

	if (json_object_object_get_ex(sys_obj, "db_file", &tmp_obj) != 0) {
		const char *const _db_file = json_object_get_string(tmp_obj);
		if (cstr_is_empty(_db_file) == 0)
			db_file = _db_file;
	}

	if (json_object_object_get_ex(sys_obj, "db_pool_conn_size", &tmp_obj) != 0) {
		const int sz = json_object_get_int(tmp_obj);
		if (sz > 0)
			db_pool_conn_size = sz;
	}

out0:
	c->import_sys_envp = import_envp;
	c->worker_size = worker_size;
	c->db_pool_conn_size = db_pool_conn_size;
	cstr_copy_n(c->db_path, LEN(c->db_path), db_file);
}


static void
_parse_json_listen(Config *c, json_object *root_obj)
{
	const char *host = CFG_DEF_LISTEN_HOST;
	uint16_t port = CFG_DEF_LISTEN_PORT;

	json_object *listen_obj;
	if (json_object_object_get_ex(root_obj, "listen", &listen_obj) == 0)
		goto out0;

	json_object *tmp_obj;
	if (json_object_object_get_ex(listen_obj, "host", &tmp_obj) != 0) {
		const char *const _host = json_object_get_string(tmp_obj);
		if (_host[0] != '\0')
			host = _host;
	}

	if (json_object_object_get_ex(listen_obj, "port", &tmp_obj) != 0)
		port = (uint16_t)json_object_get_uint64(tmp_obj);

out0:
	cstr_copy_n(c->listen_host, LEN(c->listen_host), host);
	c->listen_port = port;
}


static void
_parse_json_cmd_extern(Config *c, json_object *root_obj)
{
	const char *cmd_extern_path = CFG_DEF_CMD_EXTERN_PATH;
	const char *cmd_extern_log_file = CFG_DEF_CMD_EXTERN_LOG_FILE;

	json_object *cmd_extern_obj;
	if (json_object_object_get_ex(root_obj, "cmd_extern", &cmd_extern_obj) == 0)
		goto out0;

	json_object *tmp_obj;
	if (json_object_object_get_ex(cmd_extern_obj, "path", &tmp_obj) != 0) {
		const char *const _path = json_object_get_string(tmp_obj);
		if (cstr_is_empty(_path) == 0)
			cmd_extern_path = _path;
	}

	if (json_object_object_get_ex(cmd_extern_obj, "log_file", &tmp_obj) != 0) {
		const char *const _file = json_object_get_string(tmp_obj);
		if (cstr_is_empty(_file) == 0)
			cmd_extern_log_file = _file;
	}

out0:
	cstr_copy_n(c->cmd_extern_path, LEN(c->cmd_extern_path), cmd_extern_path);
	cstr_copy_n(c->cmd_extern_log_file, LEN(c->cmd_extern_log_file), cmd_extern_log_file);
}
