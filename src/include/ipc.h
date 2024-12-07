#ifndef __IPC_H__
#define __IPC_H__


typedef struct ipc {
	int __dummy;
} Ipc;

int  ipc_init(Ipc *i, const char name[]);
void ipc_deinit(Ipc *i);
int  ipc_run(Ipc *i);
void ipc_stop(Ipc *i);


#endif
