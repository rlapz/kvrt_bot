#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <json.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>

#include "config.h"

#include "util.h"


#define _BUFFER_SIZE (1024 * 1024)


static int  _read_file(char buffer[], size_t size, const char path[]);
static int  _parse(Config *c, const char buffer[]);
static int  _parse_api(Config *c, json_object *obj);
static int  _parse_hook(Config *c, json_object *obj);
static int  _parse_tg(Config *c, json_object *obj);
static void _parse_listen(Config *c, json_object *obj);
static void _parse_sys(Config *c, json_object *obj);
static void _parse_cmd_extern(Config *c, json_object *obj);
static int  _validate(const Config *c);


/*
 * Public
 */
int
config_load_from_json(const char path[], Config **cfg)
{
	Config *const new_cfg = malloc(sizeof(Config));
	if (new_cfg == NULL) {
		log_err(errno, "config_load_from_env: malloc");
		return -1;
	}

	char buffer[_BUFFER_SIZE];
	if (_read_file(buffer, sizeof(buffer), path) < 0)
		goto err0;

	if (_parse(new_cfg, buffer) < 0)
		goto err0;

	*cfg = new_cfg;
	return 0;

err0:
	free(new_cfg);
	return -1;
}


void
config_dump(const Config *c)
{
	puts("---[CONFIG]---");

#ifdef DEBUG
	printf("Api Token            : %s\n", c->api.token);
	printf("Api Secret           : %s\n", c->api.secret);
#else
	printf("Api Token            : *****************\n");
	printf("Api Secret           : *****************\n");
#endif

	printf("Hook URL             : %s%s\n", c->hook.url, c->hook.path);

	printf("Listen Host          : %s\n", c->listen.host);
	printf("Listen Port          : %d\n", c->listen.port);
	printf("Worker Size          : %u\n", c->sys.worker_size);
	printf("Child Process Max    : %u\n", CFG_CHLD_ITEMS_SIZE);
	printf("DB file              : %s\n", c->sys.db_file);
	printf("DB pool connections  : %d\n", c->sys.db_pool_conn_size);
	printf("Owner ID             : %" PRIi64 "\n", c->tg.owner_id);
	printf("Bot ID               : %" PRIi64 "\n", c->tg.bot_id);
	printf("Bot username         : %s\n", c->tg.bot_username);
	printf("External cmd path    : %s\n", c->cmd_extern.path);
	printf("External cmd log file: %s\n", c->cmd_extern.log_file);
	puts("---[CONFIG]---");
}


/*
 * Private
 */
static int
_read_file(char buffer[], size_t size, const char path[])
{
	int ret = -1;
	const int fd = open(path, O_RDONLY);
	if (fd < 0) {
		log_err(errno, "config: _read_file: open: \"%s\"", path);
		return -1;
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		log_err(errno, "config: _read_file: fstat: \"%s\"", path);
		goto out0;
	}

	if ((size_t)st.st_size >= size) {
		log_err(0, "config: _read_file: \"%s\": file too big: max %zubytes", path, size);
		goto out0;
	}

	size_t total = 0;
	const size_t rsize = (size_t)st.st_size;
	while (total < rsize) {
		const ssize_t rd = read(fd, buffer + total, rsize - total);
		if (rd < 0) {
			log_err(errno, "config: _read_file: read: \"%s\"", path);
			goto out0;
		}

		if (rd == 0)
			break;

		total += (size_t)rd;
	}

	buffer[total] = '\0';
	ret = 0;

out0:
	close(fd);
	return ret;
}


static int
_parse(Config *c, const char buffer[])
{
	int ret = -1;
	json_object *const root_obj = json_tokener_parse(buffer);
	if (root_obj == NULL) {
		log_err(0, "config: _parse: json_tokener_parse: failed");
		return -1;
	}

	if (_parse_api(c, root_obj) < 0)
		goto out0;

	if (_parse_hook(c, root_obj) < 0)
		goto out0;

	if (_parse_tg(c, root_obj) < 0)
		goto out0;

	_parse_sys(c, root_obj);
	_parse_listen(c, root_obj);
	_parse_cmd_extern(c, root_obj);

	ret = _validate(c);

out0:
	json_object_put(root_obj);
	return ret;
}


static int
_parse_api(Config *c, json_object *obj)
{
	json_object *api_obj;
	if (json_object_object_get_ex(obj, "api", &api_obj) == 0) {
		log_err(0, "config: _parse_api: api: no such object");
		return -1;
	}

	json_object *token_obj;
	if (json_object_object_get_ex(api_obj, "token", &token_obj) == 0) {
		log_err(0, "config: _parse_api: api.token: no such object");
		return -1;
	}

	json_object *secret_obj;
	if (json_object_object_get_ex(api_obj, "secret", &secret_obj) == 0) {
		log_err(0, "config: _parse_api: api.secret: no such object");
		return -1;
	}

	const char *const token = json_object_get_string(token_obj);
	if (cstr_is_empty(token)) {
		log_err(0, "config: _parse_api: api.token: empty");
		return -1;
	}

	const char *const secret = json_object_get_string(secret_obj);
	if (cstr_is_empty(secret)) {
		log_err(0, "config: _parse_api: api.secret: empty");
		return -1;
	}

	c->api.token_len = cstr_copy_n(c->api.token, CFG_API_TOKEN_SIZE, token);
	c->api.secret_len = cstr_copy_n(c->api.secret, CFG_API_SECRET_SIZE, secret);
	return 0;
}


static int
_parse_hook(Config *c, json_object *obj)
{
	json_object *hook_obj;
	if (json_object_object_get_ex(obj, "hook", &hook_obj) == 0) {
		log_err(0, "config: _parse_hook: hook: no such object");
		return -1;
	}

	json_object *url_obj;
	if (json_object_object_get_ex(hook_obj, "url", &url_obj) == 0) {
		log_err(0, "config: _parse_api: hook.url: no such object");
		return -1;
	}

	json_object *path_obj;
	if (json_object_object_get_ex(hook_obj, "path", &path_obj) == 0) {
		log_err(0, "config: _parse_api: hook.path: no such object");
		return -1;
	}

	const char *const url = json_object_get_string(url_obj);
	if (cstr_is_empty(url)) {
		log_err(0, "config: _parse_api: hook.url: empty");
		return -1;
	}

	const char *const path = json_object_get_string(path_obj);
	if (cstr_is_empty(path)) {
		log_err(0, "config: _parse_api: hook.path: empty");
		return -1;
	}

	c->hook.url_len = cstr_copy_n(c->hook.url, CFG_HOOK_URL_SIZE, url);
	cstr_to_lower_n(c->hook.url, c->hook.url_len);

	c->hook.path_len = cstr_copy_n(c->hook.path, CFG_HOOK_PATH_SIZE, path);
	return 0;
}


static int
_parse_tg(Config *c, json_object *obj)
{
	json_object *tg_obj;
	if (json_object_object_get_ex(obj, "tg", &tg_obj) == 0) {
		log_err(0, "config: _parse_tg: tg: no such object");
		return -1;
	}

	json_object *bot_id_obj;
	if (json_object_object_get_ex(tg_obj, "bot_id", &bot_id_obj) == 0) {
		log_err(0, "config: _parse_tg: tg.bot_id: no such object");
		return -1;
	}

	json_object *owner_id_obj;
	if (json_object_object_get_ex(tg_obj, "owner_id", &owner_id_obj) == 0) {
		log_err(0, "config: _parse_tg: tg.owner_id: no such object");
		return -1;
	}

	json_object *bot_username_obj;
	if (json_object_object_get_ex(tg_obj, "bot_username", &bot_username_obj) == 0) {
		log_err(0, "config: _parse_tg: tg.bot_username: no such object");
		return -1;
	}

	c->tg.bot_id = json_object_get_int64(bot_id_obj);
	if (c->tg.bot_id == 0) {
		log_err(0, "config: _parse_tg: tg.bot_id: invalid value");
		return -1;
	}

	c->tg.owner_id = json_object_get_int64(owner_id_obj);
	if (c->tg.owner_id == 0) {
		log_err(0, "config: _parse_tg: tg.owner_id: invalid value");
		return -1;
	}

	const char *const bot_username = json_object_get_string(bot_username_obj);
	if (cstr_is_empty(bot_username)) {
		log_err(0, "config: _parse_tg: tg.bot_username: empty");
		return -1;
	}

	const size_t len = cstr_copy_n(c->tg.bot_username, CFG_TG_BOT_USERNAME_SIZE, bot_username);
	cstr_to_lower_n(c->tg.bot_username, len);
	return 0;
}


static void
_parse_listen(Config *c, json_object *obj)
{
	const char *host = CFG_DEF_LISTEN_HOST;
	int port = CFG_DEF_LISTEN_PORT;

	json_object *listen_obj;
	if (json_object_object_get_ex(obj, "listen", &listen_obj) == 0)
		goto out0;

	json_object *tmp_obj;
	if (json_object_object_get_ex(listen_obj, "host", &tmp_obj) != 0) {
		const char *const _host = json_object_get_string(tmp_obj);
		if (_host[0] != '\0')
			host = _host;
	}

	if (json_object_object_get_ex(listen_obj, "port", &tmp_obj) != 0)
		port = (int)json_object_get_int(tmp_obj);

out0:
	cstr_copy_n(c->listen.host, CFG_LISTEN_HOST_SIZE, host);
	c->listen.port = port;
}


static void
_parse_sys(Config *c, json_object *obj)
{
	unsigned worker_size = CFG_DEF_SYS_WORKER_SIZE;
	const char *db_file = CFG_DEF_SYS_DB_FILE;
	int db_pool_conn_size = CFG_DEF_DB_CONN_POOL_SIZE;

	json_object *sys_obj;
	if (json_object_object_get_ex(obj, "sys", &sys_obj) == 0)
		goto out0;

	json_object *tmp_obj;
	if (json_object_object_get_ex(sys_obj, "worker_size", &tmp_obj) != 0)
		worker_size = (unsigned)json_object_get_uint64(tmp_obj);

	if (json_object_object_get_ex(obj, "db_file", &tmp_obj) != 0) {
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
	c->sys.worker_size = worker_size;
	c->sys.db_pool_conn_size = db_pool_conn_size;
	cstr_copy_n(c->sys.db_file, CFG_SYS_DB_FILE_SIZE, db_file);
}


static void
_parse_cmd_extern(Config *c, json_object *obj)
{
	const char *cmd_extern_path = CFG_DEF_CMD_EXTERN_PATH;
	const char *cmd_extern_log_file = CFG_DEF_CMD_EXTERN_LOG_FILE;

	json_object *cmd_extern_obj;
	if (json_object_object_get_ex(obj, "cmd_extern", &cmd_extern_obj) == 0)
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
	cstr_copy_n(c->cmd_extern.path, CFG_CMD_EXTERN_PATH_SIZE, cmd_extern_path);
	cstr_copy_n(c->cmd_extern.log_file, CFG_CMD_EXTERN_LOG_FILE_SIZE, cmd_extern_log_file);
}


static int
_validate(const Config *c)
{
	/* TODO */
	(void)c;
	return 0;
}
