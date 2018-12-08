#ifndef FRAME_H
#define FRAME_H

#include "threads/thread.h"

struct frame_table_entry{
	uint32_t frame;
	struct thread* owner;
	struct 
	struct list_elem elem;
};

struct list frame_table;


#endif