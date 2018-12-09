#ifndef FRAME_H
#define FRAME_H

#include "threads/thread.h"

struct frame_table_entry {
  uint32_t *frame;
  struct thread* owner;
  uint32_t *pg_info;
  void* vaddr;
  struct list_elem elem;
};

struct list frame_table;

/* Wrap up palloc_get_page () and palloc_free_page ()
** When the page allocation happens, automatically modify the frame table*/
void frame_init (); // Initialize the data structure, lock etc.
void *allocate_frame (enum palloc_flags flags); // Wrap up palloc_get_page ()
void free_frame (void *page); // Wrap up palloc_free_page ()
struct frame_table_entry * get_frame (void *frame);

/* frame table management functionalities */
void change_owner (void* kpage, uint32_t * pte, void * upage);

/* evict a frame to be freed and write the content to swap slot or file*/
void *evict_frame (void);


#endif
