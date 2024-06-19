#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <threads.h>

#include "thrd_pool.h"
#include "util.h"


typedef struct thrd_pool_job {
	ThrdPoolFn            func;
	void                 *udata;
	struct thrd_pool_job *next;
} ThrdPoolJob;


typedef struct thrd_pool_worker {
	ThrdPool *parent;
	void     *context;
	thrd_t    thread;
} ThrdPoolWorker;


static int          _create_threads(ThrdPool *t, void *ctx, size_t ctx_nmemb);
void                _stop(ThrdPool *t);
static int          _jobs_init(ThrdPool *t, unsigned init_size, unsigned max_size);
static void         _jobs_deinit(ThrdPool *t);
static int          _jobs_enqueue(ThrdPool *t, ThrdPoolFn func, void *udata);
static ThrdPoolJob *_jobs_dequeue(ThrdPool *t);
static void         _jobs_pool_push(ThrdPool *t, ThrdPoolJob *job);
static ThrdPoolJob *_jobs_pool_pop(ThrdPool *t);
static void         _jobs_pool_destroy(ThrdPool *t);
static int          _worker_fn(void *udata);


/*
 * Public
 */
int
thrd_pool_create(ThrdPool *t, unsigned thrd_size, void *thrd_ctx, size_t thrd_ctx_nmemb,
		 unsigned jobs_init_size, unsigned jobs_max_size)
{
	if (thrd_size <= 1) {
		log_err(EINVAL, "thrd_pool: thrd_pool_create: _jobs_init: thrd_size: %u", thrd_size);
		return -1;
	}

	if (_jobs_init(t, jobs_init_size, jobs_max_size) < 0) {
		log_err(ENOMEM, "thrd_pool: thrd_pool_create: _jobs_init: %zu:%zu", jobs_init_size, jobs_max_size);
		return -1;
	}
	
	if (mtx_init(&t->mtx_general, mtx_plain) != 0) {
		log_err(0, "thrd_pool: thrd_pool_create: mtx_init: failed to init");
		goto err0;
	}

	if (cnd_init(&t->cond_job) != 0) {
		log_err(0, "thrd_pool: thrd_pool_create: cnd_init: failed to init");
		goto err1;
	}

	t->workers = malloc(sizeof(ThrdPoolWorker) * thrd_size);
	if (t->workers == NULL) {
		log_err(errno, "thrd_pool: thrd_pool_create: malloc: workers: failed to allocate");
		goto err2;
	}

	t->is_alive = 0;
	t->workers_len = thrd_size;
	if (_create_threads(t, thrd_ctx, thrd_ctx_nmemb) < 0)
		goto err3;

	return 0;

err3:
	free(t->workers);
err2:
	cnd_destroy(&t->cond_job);
err1:
	mtx_destroy(&t->mtx_general);
err0:
	_jobs_deinit(t);
	return -1;
}


void
thrd_pool_destroy(ThrdPool *t)
{
	_stop(t);
	if (t->workers != NULL) {
		for (unsigned i = 0; i < t->workers_len; i++) {
			if (thrd_join(t->workers[i].thread, NULL) != 0)
				log_err(0, "thrd_pool: thrd_pool_destroy: thrd_join: failed to join %u", i);
		}

		free(t->workers);
	}

	_jobs_deinit(t);
	cnd_destroy(&t->cond_job);
	mtx_destroy(&t->mtx_general);
}


int
thrd_pool_add_job(ThrdPool *t, ThrdPoolFn func, void *udata)
{
	int ret = -1;
	mtx_lock(&t->mtx_general); /* LOCK */

	ret = _jobs_enqueue(t, func, udata);
	if (ret < 0) {
		log_err(ENOMEM, "thrd_pool: thrd_pool_add_job: _jobs_enqueue");
		goto out0;
	}

	cnd_signal(&t->cond_job);

out0:
	mtx_unlock(&t->mtx_general); /* UNLOCK */
	return ret;
}


/*
 * Private
 */
static int
_create_threads(ThrdPool *t, void *ctx, size_t ctx_nmemb)
{
	ThrdPoolWorker *wrk;
	char *const ctx_mem = (char *)ctx;
	unsigned iter = 0;


	t->is_alive = 1;
	while (iter < t->workers_len) {
		wrk = &t->workers[iter];
		wrk->parent = t;
		wrk->context = &ctx_mem[iter * ctx_nmemb];

		log_debug("thrd_pool: ctx: %p", wrk->context);

		if (thrd_create(&t->workers[iter].thread, _worker_fn, wrk) != 0) {
			log_err(0, "thrd_pool: _create_threads: thrd_create: failed to create thread: %u", iter);
			goto err0;
		}

		iter++;
	}

	return 0;

err0:
	_stop(t);
	while (iter--)
		thrd_join(t->workers[iter].thread, NULL);

	return -1;
}


void
_stop(ThrdPool *t)
{
	mtx_lock(&t->mtx_general); /* LOCK */
	t->is_alive = 0;
	cnd_broadcast(&t->cond_job);
	mtx_unlock(&t->mtx_general); /* UNLOCK */
}


static int
_jobs_init(ThrdPool *t, unsigned init_size, unsigned max_size)
{
	if ((max_size == 0) || init_size > max_size)
		return -1;

	t->jobs_pool = NULL;
	for (unsigned i = 0; i < init_size; i++) {
		ThrdPoolJob *const job = malloc(sizeof(ThrdPoolJob));
		if (job == NULL) {
			_jobs_pool_destroy(t);
			return -1;
		}

		_jobs_pool_push(t, job);
	}

	t->jobs_len = 0;
	t->jobs_cap = init_size;
	t->jobs_max = max_size;
	t->job_first = NULL;
	t->job_last = NULL;
	return 0;
}


static void
_jobs_deinit(ThrdPool *t)
{
	ThrdPoolJob *job;
	while ((job = _jobs_dequeue(t)) != NULL);

	_jobs_pool_destroy(t);
	t->job_first = NULL;
	t->job_last = NULL;
}


static int
_jobs_enqueue(ThrdPool *t, ThrdPoolFn func, void *udata)
{
	/* reuse allocated memory if any */
	ThrdPoolJob *job = _jobs_pool_pop(t);
	if (job == NULL) {
		if (t->jobs_cap > t->jobs_max)
			return -1;
		
		job = malloc(sizeof(ThrdPoolJob));
		if (job == NULL)
			return -1;
		
		t->jobs_cap++;
	}

	if (t->job_last == NULL)
		t->job_first = job;
	else
		t->job_last->next = job;

	job->func = func;
	job->udata = udata;
	job->next = NULL;

	t->job_last = job;
	t->jobs_len++;
	return 0;
}


static ThrdPoolJob *
_jobs_dequeue(ThrdPool *t)
{
	ThrdPoolJob *const job = t->job_first;
	if ((job == NULL) || (t->jobs_len == 0))
		return NULL;

	t->job_first = job->next;
	if (t->job_first == NULL)
		t->job_last = NULL;

	_jobs_pool_push(t, job);
	t->jobs_len--;
	return job;
}


static inline void
_jobs_pool_push(ThrdPool *t, ThrdPoolJob *job)
{
	job->next = t->jobs_pool;
	t->jobs_pool = job;
}


static inline ThrdPoolJob *
_jobs_pool_pop(ThrdPool *t)
{
	ThrdPoolJob *const job = t->jobs_pool;
	if (job != NULL)
		t->jobs_pool = job->next;

	return job;
}


static void
_jobs_pool_destroy(ThrdPool *t)
{
	ThrdPoolJob *job;
	while ((job = _jobs_pool_pop(t)) != NULL)
		free(job);
	
	t->jobs_pool = NULL;
}


static int
_worker_fn(void *udata)
{
	ThrdPoolWorker *const w = (ThrdPoolWorker *)udata;
	ThrdPool *const t = w->parent;
	ThrdPoolJob *job;
	ThrdPoolFn tmp_func;
	void *tmp_udata;


	log_debug("worker_fn: ctx: %p: %p", udata, (void *)w->context);


	mtx_lock(&t->mtx_general); /* LOCK */
	while (t->is_alive) {
		while ((job = _jobs_dequeue(t)) == NULL) {
			cnd_wait(&t->cond_job, &t->mtx_general);
			if (t->is_alive == 0)
				goto out0;
		}

		/* copy job's data to prevent clobbered by other thread */
		tmp_func = job->func;
		tmp_udata = job->udata;

		mtx_unlock(&t->mtx_general); /* UNLOCK */

		assert(tmp_func != NULL);
		tmp_func(w->context, tmp_udata);

		mtx_lock(&t->mtx_general); /* LOCK */
		cnd_signal(&t->cond_job);
	}

out0:
	mtx_unlock(&t->mtx_general); /* UNLOCK */
	return 0;
}
