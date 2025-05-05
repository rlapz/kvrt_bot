#ifndef __THRD_POOL_H__
#define __THRD_POOL_H__


#include <threads.h>


typedef void (*ThrdPoolFn) (void *ctx, void *udata);

int  thrd_pool_init(unsigned thrd_size);
void thrd_pool_deinit(void);
int  thrd_pool_add_job(ThrdPoolFn func, void *ctx, void *udata);


#endif
