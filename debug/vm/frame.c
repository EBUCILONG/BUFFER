#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "threads/pte.h"
#include "vm/swap.h"

#include "threads/interrupt.h"
#include "threads/intr-stubs.h"

#include "vm/frame.h"

static struct lock frame_lock;

//fuck static struct lock eviction_lock;
static struct frame_table_entry *pick_victim (void);
static bool bookkeep_eviction (struct frame_table_entry *);


void
frame_init ()
{
  list_init (&frame_table);
  lock_init (&frame_lock);
}

/* allocate a page from USER_POOL, and add an entry to frame table */
void *
allocate_frame (enum palloc_flags flags)
{
  void *frame = NULL;

  if (flags & PAL_USER){
      if (flags & PAL_ZERO)
        frame = palloc_get_page (PAL_USER | PAL_ZERO);
      else
        frame = palloc_get_page (PAL_USER);
  }

  if (frame != NULL){
    struct frame_table_entry *fte = malloc(sizeof *fte);
    fte->owner = thread_current();
    fte->frame = frame;
    lock_acquire (&frame_lock);
    list_push_back (&frame_table, &fte->elem);
    lock_release (&frame_lock);
  }
  else{
    frame = evict_frame ();
    ASSERT(frame != NULL);
  }
  return frame;
}

void
free_frame (void *frame)
{
  struct frame_table_entry *fte;
  struct list_elem *e;

  lock_acquire(&frame_lock);
  for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
    fte = list_entry (e, struct frame_table_entry, elem);
    if(fte->frame == frame){
      list_remove(e);
      free(fte);
      break;
    }
  }
  lock_release(&frame_lock);
  palloc_free_page (frame);
}


/* evict a frame and save its content for later swap in */
void *
evict_frame ()
{
  bool result;
  struct frame_table_entry *fte;
  // struct thread *t = thread_current ();

  enum intr_level old_level = intr_disable ();

  fte = pick_victim ();
  ASSERT (fte != NULL);

  result = bookkeep_eviction (fte);
  ASSERT (result != NULL);

  fte->owner = thread_current ();

  intr_set_level (old_level);

  return fte->frame;
}

/* select a frame to evict */
static struct frame_table_entry *
pick_victim ()
{
  struct frame_table_entry *fte = NULL;
  struct frame_table_entry *victim = NULL;
  struct thread *t;
  struct list_elem *e = list_begin(&frame_table);
  struct list_elem *next;

  int round_cnt = 1;

  // Clock approximate LRU Algorithm
  while(true){
    (e == list_back(&frame_table))?(next = list_begin(&frame_table)):(next = list_next (e));
    fte = list_entry (e, struct frame_table_entry, elem);
    t = fte->owner;

    if(!pagedir_is_accessed (t->pagedir, fte->vaddr)){
      victim = fte;
      break;
    }
    else{
      pagedir_set_accessed (t->pagedir, fte->vaddr, false);
    }
    e = next;
    if(e == list_begin(&frame_table)){
      round_cnt++;
      if(round_cnt > 3)
        return NULL; // Cannot pick a victim
    }
  }
  return victim;
}

/* save evicted frame's content for later swap in */
static bool
bookkeep_eviction (struct frame_table_entry *fte)
{
  struct thread *t = fte->owner;
  struct sup_pte *spte = get_addr_pte (&t->sup_page_table, fte->vaddr);
  size_t swap_idx;

  if (!spte) {
      spte = malloc(sizeof(struct sup_pte));
      spte->type = SWAP;
      spte->user_vaddr = fte->vaddr;
      if (!insert_sup_pte (&t->sup_page_table, spte))
        return false;
  }

  if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) && spte->type == MMF)
	  write_mmf_back (spte);
  else if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) || spte->type != FILE){
      spte->type = spte->type|SWAP;
      swap_idx = swap_out (spte->user_vaddr);
      spte->swap_index = swap_idx;

  }

  spte->loaded = false;
  spte->writable = *(fte->pg_info) & PTE_W;

  pagedir_clear_page (t->pagedir, spte->user_vaddr);
  memset (fte->frame, 0, PGSIZE);

  return true;
}

/* Get the vm_frame struct, whose frame contribute equals the given frame, from
 * the frame list frame_table. */
struct frame_table_entry *
get_frame (void *frame)
{

  struct frame_table_entry *fte;
  struct list_elem *e = list_begin (&frame_table);

  lock_acquire (&frame_lock);
  for (; e != list_end(&frame_table); e = list_next (e)){
      fte = list_entry (e, struct frame_table_entry, elem);
      if (fte->frame == frame){
        lock_release (&frame_lock);
        return fte;
      }
  }  
  lock_release (&frame_lock);
  return NULL;
}
