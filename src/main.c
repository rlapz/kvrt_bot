#include <assert.h>
#include <errno.h>
#include <json.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/socket.h>

#include "config.h"
#include "cmd.h"
#include "ev.h"
#include "model.h"
#include "picohttpparser.h"
#include "sched.h"
#include "sqlite_pool.h"
#include "tg_api.h"
#include "thrd_pool.h"
#include "update.h"
#include "util.h"
#include "webhook.h"


/*
 * Client
 */
enum {
	_CLIENT_STATE_REQ_HEADER,
	_CLIENT_STATE_REQ_BODY,
	_CLIENT_STATE_RESP,
	_CLIENT_STATE_FINISH,
};

static const char *_client_state_str(int state);


typedef struct server Server;

typedef struct client {
	Server      *parent;
	json_object *body;
	size_t       body_len;
	EvCtx        ctx;
	DListNode    node;
	time_t       created_at;
	int          state;
	size_t       bytes;
	char         buffer[CFG_BUFFER_SIZE];
} Client;

static int _client_handle_state(Client *c);

static int _client_state_req_header(Client *c);
static int _client_state_req_body(Client *c);
static int _client_state_resp(Client *c);

static int  _client_header_parse(Client *c, size_t last_len);
static int  _client_header_validate(Client *c, HttpRequest *req, size_t *content_len);
static void _client_body_parse(Client *c);
static int  _client_resp_send(Client *c);


/*
 * Server
 */
typedef struct server_verif {
	size_t      api_secret_len;
	const char *hook_url;
	size_t      hook_url_len;
	size_t      hook_path_len;
} ServerVerif;

typedef struct server {
	const char *config_file;
	Config      config;
	ServerVerif verif;
	DList       clients;
} Server;

static int  _server_init(Server *s, const char config_file[]);
static void _server_deinit(Server *s);
static int  _server_init_chld(Server *s, const char api[], char *envp[]);
static int  _server_run(Server *s, char *envp[]);

static void _server_on_signal(void *udata, uint32_t signo, int err);
static void _server_on_timer(void *udata, int err);
static void _server_on_listener(void *udata, int fd);

static int  _server_add_client(Server *s, int fd);
static void _server_del_client(Server *s, Client *client);
static void _server_handle_client(EvCtx *ctx);
static void _server_handle_update(void *ctx, void *udata);


/* IMPL */


/*
 * Client
 */
static const char *
_client_state_str(int state)
{
	switch (state) {
	case _CLIENT_STATE_REQ_HEADER: return "request header";
	case _CLIENT_STATE_REQ_BODY: return "request body";
	case _CLIENT_STATE_RESP: return "response";
	case _CLIENT_STATE_FINISH: return "finish";
	}

	return "unkonwon";
}


static int
_client_handle_state(Client *c)
{
	log_debug("main: _client_handle_state: %p: fd: %d: state: %s", (void *)c,
		  c->ctx.fd, _client_state_str(c->state));

	int state = c->state;
	switch (state) {
	case _CLIENT_STATE_REQ_HEADER:
		state = _client_state_req_header(c);
		break;
	case _CLIENT_STATE_REQ_BODY:
		state = _client_state_req_body(c);
		break;
	case _CLIENT_STATE_RESP:
		state = _client_state_resp(c);
		break;
	}

	if (state == _CLIENT_STATE_FINISH)
		return 0;

	c->state = state;
	return 1;
}


static int
_client_state_req_header(Client *c)
{
	const size_t recvd = c->bytes;
	const size_t len = (CFG_BUFFER_SIZE - recvd);
	if (len == 0) {
		log_err(ENOMEM, "main: _client_state_req_header: %d: buffer full", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	}

	const ssize_t rv = recv(c->ctx.fd, c->buffer + recvd, len, 0);
	if (rv < 0) {
		if (errno == EAGAIN)
			return _CLIENT_STATE_REQ_HEADER;

		log_err(errno, "main: _client_state_req_header: %d: recv", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	}

	if (rv == 0) {
		log_err(0, "main: _client_state_req_header: %d: recv: EOF", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	}

	c->bytes = recvd + (size_t)rv;
	log_debug("main: _client_state_req_header: %d: %zu", c->ctx.fd, c->bytes);

	const int ret = _client_header_parse(c, recvd);
	switch (ret) {
	case -3:
		/* TODO: allocate new buffer */
		log_err(0, "main: _client_state_req_header: %d: _client_header_parse: buffer full", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	case -2:
		/* header: incomplete */
		return _CLIENT_STATE_REQ_HEADER;
	case -1:
		log_err(0, "main: _client_state_req_header: %d: _client_header_parse: invalid request header",
			c->ctx.fd);
		return _client_resp_send(c);
	case 0:
		_client_body_parse(c);
		return _client_resp_send(c);
	case 1:
		/* body: incomplete */
		return _client_state_req_body(c);
	}

	/* BUG: must not reach this line! */
	assert(0);
}


static int
_client_state_req_body(Client *c)
{
	size_t recvd = c->bytes;
	const size_t len = c->body_len;
	const ssize_t rv = recv(c->ctx.fd, c->buffer + recvd, len - recvd, 0);
	if (rv < 0) {
		if (errno == EAGAIN)
			return _CLIENT_STATE_REQ_BODY;

		log_err(errno, "main: _client_state_req_body: %d: recv", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	}

	if (rv == 0) {
		log_err(0, "main: _client_state_req_body: %d: recv: EOF", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	}

	recvd += (size_t)rv;
	c->bytes = recvd;
	if (recvd < len)
		return _CLIENT_STATE_REQ_BODY;

	if (recvd == len)
		_client_body_parse(c);

	return _client_resp_send(c);
}


static int
_client_state_resp(Client *c)
{
	int is_err = 1;
	const char *const buff = (c->body != NULL)? CFG_HTTP_RESPONSE_OK : CFG_HTTP_RESPONSE_ERROR;
	const size_t buff_len = (c->body != NULL)? sizeof(CFG_HTTP_RESPONSE_OK) - 1 :
						   sizeof(CFG_HTTP_RESPONSE_ERROR) - 1;

	size_t sent = c->bytes;
	const ssize_t sn = send(c->ctx.fd, buff + sent, buff_len - sent, 0);
	if (sn < 0) {
		if (errno == EAGAIN)
			return _CLIENT_STATE_RESP;

		log_err(errno, "main: _client_state_resp: %d: send", c->ctx.fd);
		goto out0;
	}

	if (sn == 0) {
		log_err(0, "main: _client_state_resp: %d: send: EOF", c->ctx.fd);
		goto out0;
	}

	sent += (size_t)sn;
	c->bytes = sent;
	if (sent < buff_len)
		return _CLIENT_STATE_RESP;

	is_err = 0;

out0:
	if (is_err) {
		json_object_put(c->body);
		c->body = NULL;
	}

	return _CLIENT_STATE_FINISH;
}


static int
_client_header_parse(Client *c, size_t last_len)
{
	const size_t len = c->bytes;
	HttpRequest req = { .hdr_len = HTTP_REQUEST_HEADER_LEN };
	const int ret = phr_parse_request(c->buffer, len, &req.method, &req.method_len,
					  &req.path, &req.path_len, &req.min_ver, req.hdrs, &req.hdr_len,
					  last_len);
	if (ret < 0)
		return ret;

	size_t content_len = 0;
	if (_client_header_validate(c, &req, &content_len) < 0)
		return -1;

	const size_t diff_len = len - (size_t)ret;
	if (diff_len > content_len)
		return -1;

	if (content_len >= CFG_BUFFER_SIZE)
		return -3;

	/* replace the header with its body... */
	memmove(c->buffer, c->buffer + ret, diff_len);
	c->buffer[diff_len] = '\0';
	c->body_len = content_len;

	/* body: complete */
	if ((content_len - diff_len) == 0)
		return 0;

	/* body: incomplete */
	c->bytes = diff_len;
	return 1;
}


static int
_client_header_validate(Client *c, HttpRequest *req, size_t *content_len)
{
	const Config *const cfg = &c->parent->config;
	const ServerVerif *const vf = &c->parent->verif;


#ifdef DEBUG
	log_debug("main: method: |%.*s|", (int)req->method_len, req->method);
	log_debug("main: path  : |%.*s|", (int)req->path_len, req->path);
	for (size_t i = 0; i < req->hdr_len; i++) {
		log_debug("main: Header: |%.*s:%.*s|", (int)req->hdrs[i].name_len, req->hdrs[i].name,
			  (int)req->hdrs[i].value_len, req->hdrs[i].value);
	}
#endif

	if (cstr_casecmp_n2(req->method, req->method_len, "POST", 4) == 0)
		return -1;
	if (cstr_casecmp_n2(req->path, req->path_len, cfg->hook_path, vf->hook_path_len) == 0)
		return -1;

	size_t flags = 0;
	const size_t eflags = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	const size_t found_len = __builtin_popcount(eflags);
	const size_t hdr_len = req->hdr_len;
	for (size_t i = 0, found = 0; (i < hdr_len) && (found < found_len); i++) {
		const struct phr_header *const hdr = &req->hdrs[i];
		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Host", 4)) {
			if (cstr_casecmp_n2(vf->hook_url, vf->hook_url_len, hdr->value, hdr->value_len) == 0)
				return -1;

			found++;
			flags |= (1 << 0);
			continue;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Content-Type", 12)) {
			if (cstr_casecmp_n2(hdr->value, hdr->value_len, "application/json", 16) == 0)
				return -1;

			found++;
			flags |= (1 << 1);
			continue;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Content-Length", 14)) {
			if ((hdr->value == NULL) || (hdr->value_len >= UINT64_DIGITS_LEN))
				return -1;

			uint64_t _clen;
			if (cstr_to_uint64_n(hdr->value, hdr->value_len, &_clen) < 0)
				return -1;

			/* TODO: properly handle content length; don't panic! */
			assert(_clen < SIZE_MAX);

			*content_len = (size_t)_clen;
			found++;
			flags |= (1 << 2);
			continue;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "X-Telegram-Bot-Api-Secret-Token", 31)) {
			if (val_cmp_n2(cfg->api_secret, vf->api_secret_len, hdr->value, hdr->value_len) == 0)
				return -1;

			found++;
			flags |= (1 << 3);
			continue;
		}
	}

	if (flags != eflags)
		return -1;

	/* TODO: add more validations */
	return 0;
}


static void
_client_body_parse(Client *c)
{
	json_object *const json = json_tokener_parse(c->buffer);
	if (json == NULL)
		log_err(0, "main: _client_body_parse: json_tokene_parse: failed to parse");

	dump_json_str("main: _client_body_parse", c->buffer);

	c->body = json;
}


static int
_client_resp_send(Client *c)
{
	c->ctx.event.events = EPOLLOUT;
	const int ret = ev_ctx_mod(&c->ctx);
	if (ret < 0) {
		log_err(ret, "main: _client_resp_send: ev_ctx_mod");
		return _CLIENT_STATE_FINISH;
	}

	c->bytes = 0;
	return _client_state_resp(c);;
}


/*
 * Server
 */
static int
_server_init(Server *s, const char config_file[])
{
	if (config_load(&s->config, config_file) < 0)
		return -1;

	dlist_init(&s->clients);

	s->verif.api_secret_len = strlen(s->config.api_secret);
	s->verif.hook_url_len = strlen(s->config.hook_url);
	s->verif.hook_path_len = strlen(s->config.hook_path);

	/* ommit "https://" */
	const char *const proto = strstr(s->config.hook_url, "https://");
	if (proto != NULL) {
		s->verif.hook_url = proto + 8;
		s->verif.hook_url_len -= 8;
	} else {
		s->verif.hook_url = s->config.hook_url;
	}

	s->config_file = config_file;
	return 0;
}


static void
_server_deinit(Server *s)
{
	const DListNode *node;
	while ((node = dlist_pop(&s->clients)) != NULL) {
		Client *const client = FIELD_PARENT_PTR(Client, node, node);
		close(client->ctx.fd);
		free(client);
	}
}


static int
_server_init_chld(Server *s, const char api[], char *envp[])
{
	const Config *const config = &s->config;
	if (chld_init(config->cmd_extern_root_dir, config->cmd_extern_log_file) < 0) {
		log_err(0, "main: _server_init_chld: chld_init: failed");
		return -1;
	}

	char buffer[PATH_MAX];
	if (config->import_sys_envp) {
		char **envp_ptr = envp;
		while (*envp_ptr != NULL) {
			if (chld_add_env(*envp_ptr) < 0)
				goto err0;

			envp_ptr++;
		}
	}

	const char *const root_dir = realpath(config->cmd_extern_root_dir, buffer);
	if (root_dir == NULL)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_ROOT_DIR, root_dir) < 0)
		goto err0;

	const char *const api_exe = realpath(config->cmd_extern_api, buffer);
	if (api_exe == NULL) {
		log_err(errno, "main: _server_init_chld: '%s'", config->cmd_extern_api);
		goto err0;
	}

	if (chld_add_env_kv(CFG_ENV_API, api_exe) < 0)
		goto err0;

	const char *const cfg_file = realpath(s->config_file, buffer);
	if (cfg_file == NULL) {
		log_err(errno, "main: _server_init_chld: '%s'", s->config_file);
		goto err0;
	}

	if (chld_add_env_kv(CFG_ENV_CONFIG_FILE, cfg_file) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_TELEGRAM_API, api) < 0)
		goto err0;

	if (snprintf(buffer, LEN(buffer), "%" PRIi64, config->owner_id) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_OWNER_ID, buffer) < 0)
		goto err0;

	if (snprintf(buffer, LEN(buffer), "%" PRIi64, config->bot_id) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_BOT_ID, buffer) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_BOT_USERNAME, config->bot_username) < 0)
		goto err0;

	return 0;

err0:
	chld_deinit();
	log_err(0, "main: _server_init_chld: chld_add_env*: failed");
	return -1;
}


static int
_server_run(Server *s, char *envp[])
{
	EvSignal signale;
	EvTimer timer;
	EvListener listener;
	Sched sched;
	const Config *const config = &s->config;


	config_dump(config);
	tg_api_init(config->api_url);

	int ret = sqlite_pool_init(config->db_path, config->db_pool_conn_size);
	if (ret < 0)
		return ret;

	if (model_init() < 0)
		goto out0;

	ret = ev_init();
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_init");
		goto out0;
	}

	ret = ev_signal_init(&signale, _server_on_signal, s);
	if (ret < 0)
		goto out1;

	ret = ev_timer_init(&timer, _server_on_timer, s, 5);
	if (ret < 0)
		goto out2;

	ret = ev_listener_init(&listener, config->listen_host, config->listen_port, _server_on_listener, s);
	if (ret < 0)
		goto out3;

	ret = _server_init_chld(s, config->api_url, envp);
	if (ret < 0)
		goto out4;

	ret = sched_init(&sched, 1);
	if (ret < 0)
		goto out5;

	ret = thrd_pool_init(config->worker_size);
	if (ret < 0)
		goto out6;

	ret = cmd_init();
	if (ret < 0)
		goto out7;

	/* register events */
	ret = ev_ctx_add(&signale.ctx);
	if (ret < 0) {
		log_err(ret, "main; _server_run: ev_ctx_add: signal");
		goto out7;
	}

	ret = ev_ctx_add(&timer.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: timer");
		goto out7;
	}

	ret = ev_ctx_add(&listener.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: listener");
		goto out7;
	}

	ret = ev_ctx_add(&sched.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: sched");
		goto out7;
	}

	ret = ev_run();
	if (ret < 0)
		log_err(ret, "main: _server_run: ev_run");

out7:
	thrd_pool_deinit();
out6:
	sched_deinit(&sched);
out5:
	chld_wait_all();
	chld_deinit();
out4:
	ev_listener_deinit(&listener);
out3:
	ev_timer_deinit(&timer);
out2:
	ev_signal_deinit(&signale);
out1:
	ev_deinit();
out0:
	sqlite_pool_deinit();
	return ret;
}


static void
_server_on_signal(void *udata, uint32_t signo, int err)
{
	if (err != 0) {
		log_err(err, "main: _server_on_signal");
		return;
	}

	putchar('\n');
	log_info("main: _server_on_signal: signo: %u", signo);

	ev_stop();
	(void)udata;
}


static void
_server_on_timer(void *udata, int err)
{
	const Server *const s = (Server *)udata;
	if (err != 0) {
		log_err(err, "main: _server_on_timer");
		return;
	}

	const DListNode *node = s->clients.first;
	while (node != NULL) {
		const Client *const client = FIELD_PARENT_PTR(Client, node, node);
		node = node->next;

		const time_t now = time(NULL);
		const time_t elapsed_s = now - client->created_at;
		if (elapsed_s >= CFG_CONNECTION_TIMEOUT_S) {
			log_info("main: _server_on_timer: client: %p: fd: %d: timed out. Closing...",
				 (void *)client, client->ctx.fd);

			shutdown(client->ctx.fd, SHUT_RDWR);
			continue;
		}

		break;
	}

	chld_reap();
}


static void
_server_on_listener(void *udata, int fd)
{
	Server *const s = (Server *)udata;
	if (fd < 0) {
		log_err(fd, "main: _server_on_listener");
		return;
	}

	if (_server_add_client(s, fd) < 0)
		close(fd);
}


static int
_server_add_client(Server *s, int fd)
{
	if (s->clients.len == CFG_MAX_CLIENTS) {
		log_err(0, "main: _server_add_client: client full: %u", s->clients.len);
		return -1;
	}

	Client *const client = malloc(sizeof(Client));
	if (client == NULL) {
		log_err(errno, "main: _server_add_client: malloc");
		return -1;
	}

	log_info("main: _server_add_client: %p: fd: %d", (void *)client, fd);
	*client = (Client) {
		.state = _CLIENT_STATE_REQ_HEADER,
		.parent = s,
		.created_at = time(NULL),
		.ctx = (EvCtx) {
			.fd = fd,
			.callback_fn = _server_handle_client,
			.event = (Event) {
				.events = EPOLLIN,
			},
		},
	};

	const int ret = ev_ctx_add(&client->ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_add_client: ev_ctx_add");
		free(client);
		return -1;
	}

	dlist_append(&s->clients, &client->node);
	return 0;
}


static void
_server_del_client(Server *s, Client *client)
{
	log_info("main: _server_del_client: %p: fd: %d", (void *)client, client->ctx.fd);
	const int ret = ev_ctx_del(&client->ctx);
	assert(ret == 0);
	(void)ret;

	close(client->ctx.fd);
	dlist_remove(&s->clients, &client->node);
	free(client);
}


static void
_server_handle_client(EvCtx *ctx)
{
	Client *const c = FIELD_PARENT_PTR(Client, ctx, ctx);
	Server *const s = c->parent;
	if (_client_handle_state(c))
		return;

	if (c->body == NULL)
		goto out0;

	if (thrd_pool_add_job(_server_handle_update, s, c->body) == 0)
		c->body = NULL;

out0:
	json_object_put(c->body);
	_server_del_client(s, c);
}


static void
_server_handle_update(void *ctx, void *udata)
{
	Server *const s = (Server *)ctx;
	json_object *const json = (json_object *)udata;
	const Config *const config = &s->config;

	const Update update = {
		.id_bot = config->bot_id,
		.id_owner = config->owner_id,
		.username = config->bot_username,
		.resp = json,
	};

	update_handle(&update);
	json_object_put(json);
}


/*
 * Main
 */
static void
_print_help(const char name[])
{
	fprintf(stderr, "Available arguments: \n"
			"  %s webhook-set\n"
			"  %s webhook-del\n"
			"  %s webhook-info\n"
			"  %s help\n",
		name, name, name, name);
}


static int
_handle_args(char *argv[], const char config_file[])
{
	const char *const argv1 = argv[1];
	if (strcmp(argv1, "help") == 0) {
		_print_help(argv[0]);
		return EXIT_SUCCESS;
	}

	Config config;
	if (config_load(&config, config_file) < 0)
		return EXIT_FAILURE;

	if (strcmp(argv1, "webhook-set") == 0)
		return -webhook_set(&config);
	if (strcmp(argv1, "webhook-del") == 0)
		return -webhook_del(&config);
	if (strcmp(argv1, "webhook-info") == 0)
		return -webhook_info(&config);

	_print_help(argv[0]);

	return EXIT_FAILURE;
}


int
main(int argc, char *argv[], char *envp[])
{
	int ret = EXIT_FAILURE;
	if (log_init(CFG_LOG_BUFFER_SIZE) < 0) {
		fprintf(stderr, "main: log_init: failed\n");
		return ret;
	}

	const char *const config_file = "./config.json.bin";
	if (argc > 1) {
		ret = _handle_args(argv, config_file);
		goto out0;
	}

	Server srv;
	ret = _server_init(&srv, config_file);
	if (ret < 0)
		goto out0;

	ret = _server_run(&srv, envp);
	_server_deinit(&srv);

out0:
	log_deinit();
	return -ret;
}
