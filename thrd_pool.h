#ifndef __THRD_POOL_H__
#define __THRD_POOL_H__


#include <threads.h>


typedef void (*ThrdPoolFn) (void *ctx, void *udata);

typedef struct thrd_pool_job ThrdPoolJob;
typedef struct thrd_pool_worker ThrdPoolWorker;

typedef struct thrd_pool {
	volatile int    is_alive;
	ThrdPoolJob    *job_first;
	ThrdPoolJob    *job_last;
	ThrdPoolJob    *jobs_pool;
	unsigned        jobs_len;
	unsigned        jobs_cap;
	unsigned        jobs_max;
	ThrdPoolWorker *workers;
	unsigned        workers_len;
	cnd_t           cond_job;
	mtx_t           mtx_general;
} ThrdPool;

int  thrd_pool_create(ThrdPool *t, unsigned thrd_size, void *thrd_ctx_arr, size_t thrd_ctx_blk_size,
		      unsigned jobs_init_size, unsigned jobs_max_size);
void thrd_pool_destroy(ThrdPool *t);
int  thrd_pool_add_job(ThrdPool *t, ThrdPoolFn func, void *udata);


#endif
