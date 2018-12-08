#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/kernel/list.h"

struct file_descriptor{
	int fid;
	int owner;
	struct file* sys_file;
	struct list_elem elem;
};

void syscall_init (void);
struct lock file_lock;
/* be the struct keep track of all the opened file */
struct list fd_list;


#endif /* userprog/syscall.h */
