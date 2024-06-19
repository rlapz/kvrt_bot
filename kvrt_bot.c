#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "kvrt_bot.h"
#include "util.h"
#include "config.h"
#include "thrd_pool.h"
#include "picohttpparser.h"
#include "json.h"
//#include "update.h"


#define HTTP_REQUEST_HEADER_LEN (16)


enum {
	STATE_REQUEST_HEADER,
	STATE_REQUEST_BODY,
	STATE_RESPONSE,
	STATE_FINISH,
};


typedef struct {
	const char        *method;
	size_t             method_len;
	const char        *path;
	size_t             path_len;
	int                min_ver;
	struct phr_header  hdrs[HTTP_REQUEST_HEADER_LEN];
	size_t             hdr_len;
} HttpRequest;


static int  _create_listener(KvrtBot *k);
static int  _run_event_loop(KvrtBot *k);
static void _add_client(KvrtBot *k);
static void _del_client(KvrtBot *k, KvrtBotClient *client);
static void _handle_client_state(KvrtBot *k, KvrtBotClient *client);
static int  _state_request_header(KvrtBot *k, KvrtBotClient *client);
static int  _state_request_header_parse(KvrtBot *k, KvrtBotClient *client, char buffer[],
					size_t len, size_t last_len);
static int  _state_request_header_validate(const Config *c, const HttpRequest *req,
					   size_t *content_len);
static int  _state_request_body(KvrtBot *k, KvrtBotClient *client);
static int  _state_response_prepare(KvrtBot *k, KvrtBotClient *client);
static int  _state_response(KvrtBotClient *client);
static void _state_finish(KvrtBot *k, KvrtBotClient *client);
static void _request_handler_fn(void *json_obj, void *udata1);


/*
 * Public
 */
int
kvrt_bot_init(KvrtBot *k)
{
	if (log_init() < 0) {
		fprintf(stderr, "kvrt_bot: kvrt_bot_init: log_init: failed to initialize\n");
		return -1;
	}

	memset(k, 0, sizeof(*k));

	unsigned i = CFG_CLIENTS_MAX;
	while (i--) {
		k->clients.slots[i] = i;
		k->clients.list[i].fd = -1;
	}

	if (config_load_from_env(&k->config) < 0)
		goto err0;
	
	k->listen_fd = -1;
	k->event_fd = -1;
	return 0;

err0:
	log_deinit();
	return -1;
}


void
kvrt_bot_deinit(KvrtBot *k)
{
	unsigned i = CFG_CLIENTS_MAX;
	while (i--) {
		KvrtBotClient *const client = &k->clients.list[i];
		if (client->is_buffer_ready)
			buffer_deinit(&client->buffer_in);
		
		if (client->fd > 0)
			close(client->fd);
	}

	if (k->listen_fd > 0)
		close(k->listen_fd);

	if (k->event_fd > 0)
		close(k->event_fd);

	log_deinit();
}


int
kvrt_bot_run(KvrtBot *k)
{
	Config *const cfg = &k->config;
	config_dump(cfg);


	if (thrd_pool_create(&k->thrd_pool, cfg->worker_threads_num, cfg->worker_jobs_min,
			     cfg->worker_jobs_max) < 0) {
		return -1;
	}

	const int ret = _run_event_loop(k);

	thrd_pool_destroy(&k->thrd_pool);
	return ret;
}


void
kvrt_bot_stop(KvrtBot *k)
{
	k->is_alive = 0;
}


/*
 * Private
 */
static int
_create_listener(KvrtBot *k)
{
	const struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons((in_port_t)k->config.listen_port),
		.sin_addr.s_addr = inet_addr(k->config.listen_host),
	};

	const int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (fd < 0) {
		log_err(errno, "kvrt_bot: _create_listener: socket");
		return -1;
	}

	const int y = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) < 0) {
		log_err(errno, "kvrt_bot: _create_listener: setsockopt: SO_REUSEADDR");
		close(fd);
		return -1;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_err(errno, "kvrt_bot: _create_listener: bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 32) < 0) {
		log_err(errno, "kvrt_bot: _create_listener: listen");
		close(fd);
		return -1;
	}

	return fd;
}


static int
_run_event_loop(KvrtBot *k)
{
	int ret = -1;
	Event event = { .events = EPOLLIN };
	Event events[CFG_EVENTS_MAX];


	const int event_fd = epoll_create1(0);
	if (event_fd < 0) {
		log_err(errno, "kvrt_bot: _run_event_loop: epoll_create1");
		return -1;
	}

	k->event_fd = event_fd;

	const int fd = _create_listener(k);
	if (fd < 0)
		goto out0;

	k->listen_fd = fd;

	event.data.ptr = k;
	if (epoll_ctl(event_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
		log_err(errno, "kvrt_bot: _run_event_loop: epoll_ctl: listener");
		goto out1;
	}

	k->is_alive = 1;
	while (k->is_alive) {
		const int num = epoll_wait(event_fd, events, CFG_EVENTS_MAX, -1);
		if (num < 0) {
			if (errno == EINTR)
				goto out2;

			log_err(errno, "kvrt_bot: _run_event_loop: epoll_wait");
			goto out1;
		}

		for (int i = 0; i < num; i++) {
			if (events[i].data.ptr == k) {
				_add_client(k);
				continue;
			}

			_handle_client_state(k, (KvrtBotClient *)events[i].data.ptr);
		}
	}

out2:
	ret = 0;
out1:
	close(fd);
	k->listen_fd = -1;
out0:
	close(event_fd);
	k->event_fd = -1;
	return ret;
}


static void
_add_client(KvrtBot *k)
{
	const int cfd = accept(k->listen_fd, NULL, NULL);
	if (cfd < 0) {
		log_err(errno, "kvrt_bot: _add_client: accept");
		return;
	}

	KvrtBotClientStack *const st = &k->clients;
	if (st->count == CFG_CLIENTS_MAX) {
		log_err(ENOMEM, "kvrt_bot: _add_client: pending connection(s): %u", st->count);
		goto err0;
	}

	const unsigned slot = st->slots[st->count];
	KvrtBotClient *const cl = &st->list[slot];
	if (cl->is_buffer_ready == 0) {
		if (buffer_init(&cl->buffer_in, CFG_CLIENT_BUFFER_IN_MIN, CFG_CLIENT_BUFFER_IN_MAX) < 0) {
			log_err(errno, "kvrt_bot: _add_client: buffer_init");
			goto err0;
		}

		log_debug("kvrt_bot: _add_client: buffer_init: %p", (void *)cl);
		cl->is_buffer_ready = 1;
	}

	cl->event.events = EPOLLIN;
	cl->event.data.ptr = cl;
	if (epoll_ctl(k->event_fd, EPOLL_CTL_ADD, cfd, &cl->event) < 0) {
		log_err(errno, "kvrt_bot: _add_client: epoll_ctl");
		goto err0;
	}

	log_debug("kvrt_bot: _add_client: %u, %p", slot, (void *)cl);

	cl->fd = cfd;
	cl->slot = slot;
	cl->req_body = NULL;
	cl->is_req_valid = 0;
	cl->bytes = 0;
	cl->state = STATE_REQUEST_HEADER;
	st->count++;
	return;

err0:
	close(cfd);
}


static void
_del_client(KvrtBot *k, KvrtBotClient *client)
{
	KvrtBotClientStack *const st = &k->clients;
	assert(st->count > 0);

	log_debug("kvrt_bot: _del_client: %u, %p", client->slot, (void *)client);

	st->count--;
	st->slots[st->count] = client->slot;

	close(client->fd);
	client->fd = -1;
}


static void
_handle_client_state(KvrtBot *k, KvrtBotClient *client)
{
	switch (client->state) {
	case STATE_REQUEST_HEADER:
		client->state = _state_request_header(k, client);
		break;
	case STATE_REQUEST_BODY:
		client->state = _state_request_body(k, client);
		break;
	case STATE_RESPONSE:
		client->state = _state_response(client);
		break;
	case STATE_FINISH:
		_state_finish(k, client);
		break;
	default:
		fprintf(stderr, "kvrt_bot: _handle_client_state: invalid state");
		assert(0);
	}
}


static int
_state_request_header(KvrtBot *k, KvrtBotClient *client)
{
	const size_t recvd = client->bytes;
	if (buffer_resize(&client->buffer_in, recvd) < 0) {
		log_err(errno, "kvrt_bot: _state_request_header: buffer_resize");
		return STATE_FINISH;
	}

	char *const buf = client->buffer_in.mem;
	const size_t buf_size = client->buffer_in.size;
	const ssize_t rv = recv(client->fd, buf + recvd, buf_size - recvd, 0);
	if (rv < 0) {
		if (errno == EAGAIN)
			return STATE_REQUEST_HEADER;
		
		log_err(errno, "kvrt_bot: _state_request_header: recv");
		return STATE_FINISH;
	}

	client->bytes = recvd + (size_t)rv;

	const int ret = _state_request_header_parse(k, client, buf, client->bytes, recvd);
	if (ret == -2) {
		if (rv == 0) {
			log_err(0, "kvrt_bot: _state_request_header: recv: EOF");
			return STATE_FINISH;
		}

		/* incomplete */
		return STATE_REQUEST_HEADER;
	}

	if (ret == -1) {
		log_err(0, "kvrt_bot: _state_request_header: _state_request_header_parse: invalid request");
		return _state_response_prepare(k, client);
	}

	log_debug("remn_len: %zu", client->req_remn_len);

	if (client->req_remn_len == 0) {
		buf[client->bytes] = '\0';
		client->is_req_valid = 1;
		return _state_response_prepare(k, client);
	}

	if (buffer_resize(&client->buffer_in, client->req_remn_len) < 0) {
		log_err(errno, "kvrt_bot: _state_request_header: buffer_resize");
		return STATE_FINISH;
	}

	client->bytes = 0;
	return STATE_REQUEST_BODY;
}


static int
_state_request_header_parse(KvrtBot *k, KvrtBotClient *client, char buffer[], size_t len,
			    size_t last_len)
{
	HttpRequest req = { .hdr_len = HTTP_REQUEST_HEADER_LEN };
	const int ret = phr_parse_request(buffer, len, &req.method, &req.method_len,
					  &req.path, &req.path_len, &req.min_ver, req.hdrs,
					  &req.hdr_len, last_len);
	if (ret < 0)
		return ret;
	
	size_t content_len;
	if (_state_request_header_validate(&k->config, &req, &content_len) < 0)
		return -1;
	
	const size_t diff_len = len - (size_t)ret;
	if (diff_len > content_len)
		return -1;

	client->req_body = buffer + ret;
	client->req_remn_len = content_len - diff_len;
	return 0;
}


static int
_state_request_header_validate(const Config *c, const HttpRequest *req, size_t *content_len)
{
	char buffer[32];
	const char *secret = NULL;
	size_t secret_len = 0;
	const char *scontent_len = NULL;
	size_t scontent_len_len = 0; 


#ifdef DEBUG
	for (size_t i = 0; i < req->hdr_len; i++) {
		log_debug("Header: |%.*s:%.*s|", (int)req->hdrs[i].name_len, req->hdrs[i].name,
			  (int)req->hdrs[i].value_len, req->hdrs[i].value);
	}
#endif

	if (req->method_len != 4)
		return -1;
	
	if (strncasecmp(req->method, "POST", 4) != 0)
		return -1;

	if (c->hook_path_len != req->path_len)
		return -1;
	
	if (strncasecmp(c->hook_path, req->path, c->hook_path_len) != 0)
		return -1;

	const size_t hdr_len = req->hdr_len;
	for (size_t i = 0; i < hdr_len; i++) {
		const struct phr_header *const hdr = &req->hdrs[i];
		if (hdr->name_len == 14) {
			if (strncasecmp(hdr->name, "Content-Length", 14) == 0) {
				scontent_len = hdr->value;
				scontent_len_len = hdr->value_len;
			}
		}

		if (hdr->name_len == 31) {
			if (strncasecmp(hdr->name, "X-Telegram-Bot-Api-Secret-Token", 31) == 0) {
				secret = hdr->value;
				secret_len = hdr->value_len;
			}
		}
	}


	if (secret == NULL)
		return -1;
	
	if ((secret_len != c->api_secret_len) || (strncmp(c->api_secret, secret, secret_len) != 0))
		return -1;
	
	if ((scontent_len == NULL) || (scontent_len_len >= sizeof(buffer)))
		return -1;
	
	cstr_copy_n2(buffer, sizeof(buffer), scontent_len, scontent_len_len);
	*content_len = (size_t)strtol(buffer, NULL, 10);
	if (errno != 0)
		return -1;
	
	/* TODO: add more validations */
	return 0;
}


static int
_state_request_body(KvrtBot *k, KvrtBotClient *client)
{
	char *const buf = client->req_body;
	const size_t buf_len = client->req_remn_len;
	size_t recvd = client->bytes;
	if (recvd < buf_len) {
		const ssize_t rv = recv(client->fd, buf + recvd, buf_len - recvd, 0);
		if (errno == EAGAIN)
			return STATE_REQUEST_BODY;
		
		if (rv < 0) {
			log_err(errno, "kvrt_bot: _state_request_body: recv");
			return STATE_FINISH;
		}

		if (rv == 0) {
			log_err(0, "kvrt_bot: _state_request_body: recv: EOF");
			return STATE_FINISH;
		}

		recvd += (size_t)rv;
		client->bytes = recvd;
	}

	if (recvd < buf_len)
		return STATE_REQUEST_BODY;
	
	if (recvd == buf_len) {
		buf[recvd] = '\0';
		client->is_req_valid = 1;
	}

	return _state_response_prepare(k, client);
}


static int
_state_response_prepare(KvrtBot *k, KvrtBotClient *client)
{
	client->event.events = EPOLLOUT;
	client->event.data.ptr = client;
	if (epoll_ctl(k->event_fd, EPOLL_CTL_MOD, client->fd, &client->event) < 0) {
		log_err(errno, "kvrt_bot: _state_response_prepare: epoll_ctl");
		return STATE_FINISH;
	}

	client->bytes = 0;
	return STATE_RESPONSE;
}


static int
_state_response(KvrtBotClient *client)
{
	const char *buf = CFG_EVENT_RESPONSE_OK;
	size_t len = sizeof(CFG_EVENT_RESPONSE_OK) - 1;
	if (client->is_req_valid == 0) {
		buf = CFG_EVENT_RESPONSE_ERR;
		len = sizeof(CFG_EVENT_RESPONSE_ERR) - 1;
	}

	size_t sent = client->bytes;
	if (sent < len) {
		const ssize_t sn = send(client->fd, buf + sent, len - sent, 0);
		if (sn < 0) {
			if (errno == EAGAIN)
				return STATE_RESPONSE;
			
			log_err(errno, "kvrt_bot: _state_response: send");
			return STATE_FINISH;
		}

		if (sn == 0) {
			log_err(0, "kvrt_bot: _state_response: send: EOF");
			return STATE_FINISH;
		}

		sent += (size_t)sn;
		client->bytes = sent;
	}

	if (sent < len)
		return STATE_RESPONSE;

	if (sent != len)
		log_err(0, "kvrt_bot: _state_response: send: invalid len: %zu:%zu", sent, len);

	return STATE_FINISH;
}


static void
_state_finish(KvrtBot *k, KvrtBotClient *client)
{
	if (client->is_req_valid == 0)
		goto out0;

	char *const body = strdup(client->req_body);
	if (body == NULL) {
		log_err(errno, "kvrt_bot: _state_response: strdup: body");
		goto out0;
	}

	if (thrd_pool_add_job(&k->thrd_pool, _request_handler_fn, body, NULL) < 0) {
		log_err(errno, "kvrt_bot: _state_response: thrd_pool_add_job");
		free(body);
	}

out0:
	_del_client(k, client);
}


static void
_request_handler_fn(void *json_obj, void *udata1)
{
	char *const body = (char *)json_obj;

	/* TODO */
	puts(body);
	free(body);
}
