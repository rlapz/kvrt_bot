#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <threads.h>

#include "thrd_pool.h"

#include "util.h"


typedef struct thrd_pool_job {
	ThrdPoolFn  func;
	void       *ctx;
	void       *udata;
	DListNode   node;
} ThrdPoolJob;

typedef struct thrd_pool ThrdPool;

typedef struct thrd_pool_worker {
	unsigned  index;
	ThrdPool *parent;
	thrd_t    thread;
} ThrdPoolWorker;

typedef struct thrd_pool {
	volatile int    is_alive;
	DList           jobs_queue;
	ThrdPoolWorker *workers;
	unsigned        workers_len;
	cnd_t           cond_job;
	mtx_t           mtx_general;
} ThrdPool;


static ThrdPool _instance;

static void         _jobs_destroy(ThrdPool *t);
static int          _create_threads(ThrdPool *t);
static int          _jobs_enqueue(ThrdPool *t, ThrdPoolFn func, void *ctx, void *udata);
static ThrdPoolJob *_jobs_dequeue(ThrdPool *t);
static void         _stop(ThrdPool *t);
static int          _worker_fn(void *udata);


/*
 * Public
 */
int
thrd_pool_init(unsigned thrd_size)
{
	ThrdPool *const t = &_instance;
	if (thrd_size <= 1) {
		LOG_ERR(EINVAL, "thrd_pool", "thrd_size: %u", thrd_size);
		return -1;
	}

	if (mtx_init(&t->mtx_general, mtx_plain) != 0) {
		LOG_ERRN("thrd_pool", "%s", "mtx_init: failed to init");
		return -1;
	}

	if (cnd_init(&t->cond_job) != 0) {
		LOG_ERRN("thrd_pool", "%s", "cnd_init: failed to init");
		goto err0;
	}

	void *const workers = malloc(sizeof(ThrdPoolWorker) * thrd_size);
	if (workers == NULL) {
		LOG_ERRP("thrd_pool", "%s", "malloc: workers");
		goto err1;
	}

	dlist_init(&t->jobs_queue);

	t->workers = workers;
	t->workers_len = thrd_size;
	t->is_alive = 1;

	if (_create_threads(t) < 0)
		goto err2;

	return 0;

err2:
	free(workers);
err1:
	cnd_destroy(&t->cond_job);
err0:
	mtx_destroy(&t->mtx_general);
	return -1;
}


void
thrd_pool_deinit(void)
{
	ThrdPool *const t = &_instance;

	_stop(t);
	for (unsigned i = 0; i < t->workers_len; i++) {
		ThrdPoolWorker *const wrk = &t->workers[i];
		if (thrd_join(wrk->thread, NULL) != thrd_success) {
			LOG_ERRN("thrd_pool", "thrd_join: [%u:%p]: failed to join",
				wrk->index, (void*)wrk);
		}
	}

	free(t->workers);
	_jobs_destroy(t);
	cnd_destroy(&t->cond_job);
	mtx_destroy(&t->mtx_general);
}


int
thrd_pool_add_job(ThrdPoolFn func, void *ctx, void *udata)
{
	int ret;
	ThrdPool *const t = &_instance;
	mtx_lock(&t->mtx_general);

	ret = _jobs_enqueue(t, func, ctx, udata);
	if (ret < 0) {
		LOG_ERR(ret, "thrd_pool", "%s", "_jobs_enqueue");
		goto out0;
	}

	cnd_signal(&t->cond_job);

out0:
	mtx_unlock(&t->mtx_general);
	return ret;
}


/*
 * Private
 */
static void
_jobs_destroy(ThrdPool *t)
{
	const DListNode *node;
	while ((node = dlist_pop(&t->jobs_queue)) != NULL)
		free(FIELD_PARENT_PTR(ThrdPoolJob, node, node));
}


static int
_create_threads(ThrdPool *t)
{
	unsigned iter = 0;
	for (; iter < t->workers_len; iter++) {
		ThrdPoolWorker *const worker = &t->workers[iter];
		worker->parent = t;
		worker->index = iter;

		LOG_INFO("thrd_pool", "[%u:%p]", iter, (void *)worker);
		if (thrd_create(&worker->thread, _worker_fn, worker) != thrd_success) {
			LOG_ERRN("thrd_pool", "thrd_create: [%u:%p]: failed to create thread",
				iter, (void *)worker);
			goto err0;
		}
	}

	return 0;

err0:
	_stop(t);
	while (iter--)
		thrd_join(t->workers[iter].thread, NULL);

	return -1;
}


static int
_jobs_enqueue(ThrdPool *t, ThrdPoolFn func, void *ctx, void *udata)
{
	ThrdPoolJob *const job = malloc(sizeof(ThrdPoolJob));
	if (job == NULL)
		return -errno;

	*job = (ThrdPoolJob) {
		.func = func,
		.ctx = ctx,
		.udata = udata,
	};

	dlist_prepend(&t->jobs_queue, &job->node);
	return 0;
}


static ThrdPoolJob *
_jobs_dequeue(ThrdPool *t)
{
	const DListNode *const node = dlist_pop(&t->jobs_queue);
	if (node == NULL)
		return NULL;

	return FIELD_PARENT_PTR(ThrdPoolJob, node, node);
}


static void
_stop(ThrdPool *t)
{
	mtx_lock(&t->mtx_general);
	t->is_alive = 0;
	cnd_broadcast(&t->cond_job);
	mtx_unlock(&t->mtx_general);
}


static int
_worker_fn(void *udata)
{
	ThrdPoolWorker *const w = (ThrdPoolWorker *)udata;
	ThrdPool *const t = w->parent;
	ThrdPoolJob *job;
	ThrdPoolFn tmp_func;
	void *tmp_ctx, *tmp_udata;


	mtx_lock(&t->mtx_general);
	LOG_INFO("thrd_pool", "[%u:%p]: running...", w->index, udata);
	while (t->is_alive) {
		while ((job = _jobs_dequeue(t)) == NULL) {
			cnd_wait(&t->cond_job, &t->mtx_general);
			if (t->is_alive == 0)
				goto out0;
		}

		tmp_func = job->func;
		tmp_ctx = job->ctx;
		tmp_udata = job->udata;
		mtx_unlock(&t->mtx_general);

		assert(tmp_func != NULL);
		tmp_func(tmp_ctx, tmp_udata);
		free(job);

		mtx_lock(&t->mtx_general);
		cnd_signal(&t->cond_job);
	}

out0:
	LOG_INFO("thrd_pool", "[%u:%p]: stopped", w->index, udata);
	mtx_unlock(&t->mtx_general);
	return 0;
}
