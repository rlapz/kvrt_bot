#include <errno.h>
#include <assert.h>
#include <stdatomic.h>
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
	atomic_int      is_alive;
	DList           jobs_queue;
	ThrdPoolWorker *workers;
	unsigned        workers_len;
	cnd_t           cond;
	mtx_t           mutex;
} ThrdPool;


static ThrdPool _instance;

static void _jobs_destroy(ThrdPool *t);
static int  _create_threads(ThrdPool *t);
static void _stop(ThrdPool *t);
static int  _worker_fn(void *udata);


/*
 * Public
 */
int
thrd_pool_create(unsigned thrd_size)
{
	ThrdPool *const t = &_instance;
	if (thrd_size <= 1) {
		LOG_ERR(EINVAL, "thrd_pool", "thrd_size: %u", thrd_size);
		return -1;
	}

	if (mtx_init(&t->mutex, mtx_plain) != 0) {
		LOG_ERRN("thrd_pool", "%s", "mtx_init: failed to init");
		return -1;
	}

	if (cnd_init(&t->cond) != 0) {
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
	atomic_store(&t->is_alive, 1);
	if (_create_threads(t) < 0)
		goto err2;

	return 0;

err2:
	free(workers);
err1:
	cnd_destroy(&t->cond);
err0:
	mtx_destroy(&t->mutex);
	return -1;
}


void
thrd_pool_destroy(void)
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
	cnd_destroy(&t->cond);
	mtx_destroy(&t->mutex);
}


int
thrd_pool_add_job(ThrdPoolFn func, void *ctx, void *udata)
{
	ThrdPool *const t = &_instance;
	if (func == NULL) {
		LOG_ERRN("thrd_pool", "%s", "func == NULL");
		return -1;
	}

	if (atomic_load_explicit(&t->is_alive, memory_order_relaxed) == 0) {
		LOG_ERRN("thrd_pool", "%s", "is_alive == 0");
		return -1;
	}

	ThrdPoolJob *const job = malloc(sizeof(ThrdPoolJob));
	if (job == NULL) {
		LOG_ERRP("thrd_pool", "%s", "malloc");
		return -1;
	}

	job->func = func;
	job->ctx = ctx;
	job->udata = udata;

	mtx_lock(&t->mutex);

	dlist_prepend(&t->jobs_queue, &job->node);

	cnd_signal(&t->cond);
	mtx_unlock(&t->mutex);
	return 0;
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


static void
_stop(ThrdPool *t)
{
	mtx_lock(&t->mutex);
	atomic_store(&t->is_alive, 0);
	cnd_broadcast(&t->cond);
	mtx_unlock(&t->mutex);
}


static int
_worker_fn(void *udata)
{
	ThrdPoolWorker *const w = (ThrdPoolWorker *)udata;
	ThrdPool *const t = w->parent;


	mtx_lock(&t->mutex);
	LOG_INFO("thrd_pool", "[%u:%p]: running...", w->index, udata);

	while (atomic_load_explicit(&t->is_alive, memory_order_relaxed)) {
		const DListNode *const node = dlist_pop(&t->jobs_queue);
		if (node == NULL) {
			cnd_wait(&t->cond, &t->mutex);
			continue;
		}

		// let another jobs flow
		mtx_unlock(&t->mutex);

		ThrdPoolJob *const job = FIELD_PARENT_PTR(ThrdPoolJob, node, node);
		const ThrdPoolFn func = job->func;
		assert(func != NULL);

		func(job->ctx, job->udata);
		free(job);

		mtx_lock(&t->mutex);
		cnd_signal(&t->cond);
	}

	LOG_INFO("thrd_pool", "[%u:%p]: stopped", w->index, udata);
	mtx_unlock(&t->mutex);
	return 0;
}
