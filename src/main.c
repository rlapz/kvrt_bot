#include <assert.h>
#include <errno.h>
#include <json.h>
#include <stdio.h>
#include <stdint.h>
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
static int  _client_resp_prep(Client *c);


/*
 * Server
 */
typedef struct server {
	const Config *cfg;
	char         *base_api;
	DList         clients;
} Server;

static int  _server_init(Server *s, const Config *cfg);
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
		log_err(0, "main: _client_state_req_header: %d: _client_header_parse: buffer full", c->ctx.fd);
		return _CLIENT_STATE_FINISH;
	case -2:
		return _CLIENT_STATE_REQ_HEADER;
	case -1:
		log_err(0, "main: _client_state_req_header: %d: _client_header_parse: invalid request header",
			c->ctx.fd);
		if (_client_resp_prep(c) < 0)
			return _CLIENT_STATE_FINISH;

		return _CLIENT_STATE_RESP;
	case 0:
		_client_body_parse(c);
		if (_client_resp_prep(c) < 0)
			return _CLIENT_STATE_FINISH;

		return _CLIENT_STATE_RESP;
	}

	/* body: incomplete */
	return _CLIENT_STATE_REQ_BODY;
}


static int
_client_state_req_body(Client *c)
{
	size_t recvd = c->bytes;
	const size_t len = c->body_len;
	if (recvd < len) {
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
	}

	if (recvd < len)
		return _CLIENT_STATE_REQ_BODY;

	if (recvd == len)
		_client_body_parse(c);

	if (_client_resp_prep(c) < 0)
		return _CLIENT_STATE_FINISH;

	return _CLIENT_STATE_RESP;
}


static int
_client_state_resp(Client *c)
{
	int is_err = 1;
	const char *const buff = (c->body != NULL)? CFG_HTTP_RESPONSE_OK : CFG_HTTP_RESPONSE_ERROR;
	const size_t buff_len = (c->body != NULL)? sizeof(CFG_HTTP_RESPONSE_OK) - 1 :
						   sizeof(CFG_HTTP_RESPONSE_ERROR) - 1;

	size_t sent = c->bytes;
	if (sent < buff_len) {
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
	}

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

	size_t content_len;
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
	const char *secret = NULL;
	size_t secret_len = 0;
	const char *scontent_len = NULL;
	size_t scontent_len_len = 0;
	const char *host = NULL;
	size_t host_len = 0;
	const char *content_type = NULL;
	size_t content_type_len = 0;
	const Config *cfg = c->parent->cfg;


#ifdef DEBUG
	log_debug("main: method: |%.*s|", (int)req->method_len, req->method);
	log_debug("main: path  : |%.*s|", (int)req->path_len, req->path);
	for (size_t i = 0; i < req->hdr_len; i++) {
		log_debug("main: Header: |%.*s:%.*s|", (int)req->hdrs[i].name_len, req->hdrs[i].name,
			  (int)req->hdrs[i].value_len, req->hdrs[i].value);
	}
#endif

	if ((req->method_len == 4) && strncasecmp(req->method, "POST", 4) != 0)
		return -1;

	if ((req->path_len == cfg->hook.path_len) &&
	    (strncasecmp(cfg->hook.path, req->path, req->path_len) != 0)) {
		return -1;
	}

	const size_t hdr_len = req->hdr_len;
	for (size_t i = 0, found = 0; (i < hdr_len) && (found < 4); i++) {
		const struct phr_header *const hdr = &req->hdrs[i];
		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Host", 4)) {
			if (host == NULL)
				found++;

			host = hdr->value;
			host_len = hdr->value_len;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Content-Type", 12)) {
			if (content_type == NULL)
				found++;

			content_type = hdr->value;
			content_type_len = hdr->value_len;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "Content-Length", 14)) {
			if (scontent_len == NULL)
				found++;

			scontent_len = hdr->value;
			scontent_len_len = hdr->value_len;
		}

		if (cstr_casecmp_n2(hdr->name, hdr->name_len, "X-Telegram-Bot-Api-Secret-Token", 31)) {
			if (secret == NULL)
				found++;

			secret = hdr->value;
			secret_len = hdr->value_len;
		}
	}

	if (cstr_cmp_n2(cfg->api.secret, cfg->api.secret_len, secret, secret_len) == 0)
		return -1;

	if ((scontent_len == NULL) || (scontent_len_len >= UINT64_DIGITS_LEN))
		return -1;

	/* ommit "https://" */
	const char *hook_url = cfg->hook.url;
	size_t hook_url_len = cfg->hook.url_len;
	{
		const char *const proto = strstr(cfg->hook.url, "https://");
		if (proto != NULL) {
			hook_url = proto + 8;
			hook_url_len = hook_url_len - 8;
		}
	}

	if (cstr_casecmp_n2(hook_url, hook_url_len, host, host_len) == 0)
		return -1;

	if (cstr_casecmp_n2(content_type, content_type_len, "application/json", 16) == 0)
		return -1;

	uint64_t clen;
	if (cstr_to_uint64_n(scontent_len, scontent_len_len, &clen) < 0)
		return -1;

	assert(clen < SIZE_MAX);

	*content_len = (size_t)clen;
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
_client_resp_prep(Client *c)
{
	c->ctx.event.events = EPOLLOUT;
	const int ret = ev_ctx_mod(&c->ctx);
	if (ret < 0) {
		log_err(ret, "main: _client_resp_prep: ev_ctx_mod");
		return -1;
	}

	c->bytes = 0;
	return 0;
}


/*
 * Server
 */
static int
_server_init(Server *s, const Config *cfg)
{
	char *const base_api = CSTR_CONCAT(CFG_TELEGRAM_API, cfg->api.token);
	if (base_api == NULL)
		return -1;

	dlist_init(&s->clients);
	s->base_api = base_api;
	s->cfg = cfg;
	return 0;
}


static void
_server_deinit(Server *s)
{
	DListNode *node;
	while ((node = dlist_pop(&s->clients)) != NULL) {
		Client *const client = FIELD_PARENT_PTR(Client, node, node);
		close(client->ctx.fd);
		free(client);
	}

	free(s->base_api);
}


static int
_server_init_chld(Server *s, const char api[], char *envp[])
{
	const Config *const cfg = s->cfg;
	if (chld_init(cfg->cmd_extern.path, cfg->cmd_extern.log_file) < 0) {
		log_err(0, "main: _server_init_chld: chld_init: failed");
		return -1;
	}

#if (CFG_CHLD_IMPORT_SYS_ENVP == 1)
	char **envp_ptr = envp;
	while (*envp_ptr != NULL) {
		if (chld_add_env(*envp_ptr) < 0)
			goto err0;

		envp_ptr++;
	}
#else
	(void)envp;
#endif

	char buff[4096];
	const char *const root_dir = getcwd(buff, LEN(buff));
	if (root_dir == NULL)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_ROOT_DIR, root_dir) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_TELEGRAM_API, api) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_TELEGRAM_SECRET_KEY, cfg->api.secret) < 0)
		goto err0;

	char owner_id[24];
	if (snprintf(owner_id, LEN(owner_id), "%" PRIi64, cfg->tg.owner_id) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_OWNER_ID, owner_id) < 0)
		goto err0;

	char bot_id[24];
	if (snprintf(bot_id, LEN(bot_id), "%" PRIi64, cfg->tg.bot_id) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_BOT_ID, bot_id) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_BOT_USERNAME, cfg->tg.bot_username) < 0)
		goto err0;

	if (chld_add_env_kv(CFG_ENV_DB_PATH, cfg->sys.db_file) < 0)
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
	const Config *const cfg = s->cfg;


	config_dump(cfg);
	tg_api_init(s->base_api);

	int ret = sqlite_pool_init(cfg->sys.db_file, cfg->sys.db_pool_conn_size);
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

	ret = ev_timer_init(&timer, _server_on_timer, s, 1);
	if (ret < 0)
		goto out2;

	ret = ev_listener_init(&listener, cfg->listen.host, cfg->listen.port, _server_on_listener, s);
	if (ret < 0)
		goto out3;

	ret = _server_init_chld(s, s->base_api, envp);
	if (ret < 0)
		goto out4;

	ret = sched_init(&sched, 1);
	if (ret < 0)
		goto out5;

	ret = thrd_pool_init(cfg->sys.worker_size);
	if (ret < 0)
		goto out6;

	ret = cmd_init();
	if (ret < 0)
		goto out7;

	/* register events */
	ret = ev_ctx_add(&signale.ctx);
	if (ret < 0) {
		log_err(ret, "main; _server_run: ev_ctx_add: signal");
		goto out8;
	}

	ret = ev_ctx_add(&timer.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: timer");
		goto out8;
	}

	ret = ev_ctx_add(&listener.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: listener");
		goto out8;
	}

	ret = ev_ctx_add(&sched.ctx);
	if (ret < 0) {
		log_err(ret, "main: _server_run: ev_ctx_add: sched");
		goto out8;
	}

	ret = ev_run();
	if (ret < 0)
		log_err(ret, "main: _server_run: ev_run");

out8:
	cmd_deinit();
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
	Server *const s = (Server *)udata;
	if (err != 0) {
		log_err(err, "main: _server_on_timer");
		return;
	}

	DListNode *node = s->clients.first;
	while (node != NULL) {
		Client *const client = FIELD_PARENT_PTR(Client, node, node);
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
	const Config *const cfg = s->cfg;

	Update update = {
		.id_bot = cfg->tg.bot_id,
		.id_owner = cfg->tg.owner_id,
		.username = cfg->tg.bot_username,
	};

	update_handle(&update, json);
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
_handle_args(char *argv[], const Config *cfg)
{
	const char *const argv1 = argv[1];
	if (strcmp(argv1, "help") == 0) {
		_print_help(argv[0]);
		return EXIT_SUCCESS;
	}

	if (strcmp(argv1, "webhook-set") == 0)
		return -webhook_set(cfg);
	if (strcmp(argv1, "webhook-del") == 0)
		return -webhook_del(cfg);
	if (strcmp(argv1, "webhook-info") == 0)
		return -webhook_info(cfg);

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

	Config *cfg;
	if (config_load_from_json("./config.json", &cfg) < 0)
		goto out0;

	if (argc > 1) {
		ret = _handle_args(argv, cfg);
		goto out1;
	}

	Server srv;
	ret = _server_init(&srv, cfg);
	if (ret < 0)
		goto out1;

	ret = _server_run(&srv, envp);
	_server_deinit(&srv);

out1:
	free(cfg);
out0:
	log_deinit();
	return -ret;
}
