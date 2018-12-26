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

#include "vm/frame.h"

/* Protect the frame list entry */
static struct lock frame_lock;

/* Protect the eviction process */
static struct lock eviction_lock;

/* Get the frame according to the given frame pointer */
static struct frame_table_entry *get_frame (void *);

/* Select a frame to evict */
static struct frame_table_entry *pick_victim (void);

/* save evicted frame's content for later swap in */
static bool bookkeep_eviction (struct frame_table_entry *);


/* init the frame table and necessary data structure */
void
frame_init ()
{
  list_init (&frame_table);
  lock_init (&frame_lock);
  lock_init (&eviction_lock);
}

/* allocate a page from USER_POOL, and add an entry to frame table */
void *
allocate_frame (enum palloc_flags flags)
{
  void *frame = NULL;

  /* trying to allocate a page from user pool */
  if (flags & PAL_USER)
    {
      if (flags & PAL_ZERO)
        frame = palloc_get_page (PAL_USER | PAL_ZERO);
      else
        frame = palloc_get_page (PAL_USER);
    }

  /* if succeed, add to frame list
     otherwise, should evict one page to swap, but fail the allocator
     for now */
  if (frame != NULL)
  {
    struct frame_table_entry *vf;
    vf = calloc(1, sizeof (struct frame_table_entry));
    if(vf == NULL)
      return NULL;
    vf->owner = thread_current();
    vf->frame = frame;
    lock_acquire (&frame_lock);
    list_push_back (&frame_table, &vf->elem);
    lock_release (&frame_lock);
  }
  else
  {
    frame = evict_frame ();
    ASSERT(frame != NULL);
  }
  return frame;
}

void
free_frame (void *frame)
{
  struct frame_table_entry *vf;
  struct list_elem *e;
  lock_acquire(&frame_lock);
  for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    vf = list_entry (e, struct frame_table_entry, elem);
    if(vf->frame == frame)
    {
      list_remove(e);
      free(vf);
      break;
    }
  }
  lock_release(&frame_lock);
  palloc_free_page (frame);
}

/* set the pte attribute to PTE in corresponding entry of FRAME */
void
change_owner (void *kpage, uint32_t *pte, void *upage)
{
  struct frame_table_entry *vf;
  vf = get_frame (kpage);
  if (vf != NULL)
    {
      vf->pte = pte;
      vf->uva = upage;
    }
}

/* evict a frame and save its content for later swap in */
void *
evict_frame ()
{
  bool result;
  struct frame_table_entry *vf;
  // struct thread *t = thread_current ();

  lock_acquire (&eviction_lock);

  vf = pick_victim ();
  ASSERT (vf != NULL);

  result = save_evicted_frame (vf);
  ASSERT (result != NULL);

  vf->owner = thread_current ();
  vf->pte = NULL;
  vf->uva = NULL;

  lock_release (&eviction_lock);

  return vf->frame;
}

/* select a frame to evict */
static struct frame_table_entry *
pick_victim ()
{
  struct frame_table_entry *vf = NULL;
  struct frame_table_entry *victim = NULL;
  struct thread *t;
  struct list_elem *e = list_begin(&frame_table);
  struct list_elem *next;

  int round_cnt = 1;

  // Clock approximate LRU Algorithm
  while(true)
  {
    (e == list_back(frame_table))?(next = list_begin(&frame_table)):(next = list_next (e));
    vf = list_entry (e, struct frame_table_entry, elem);
    t = vf->owner;
    bool visit_recent = pagedir_is_accessed (t->pagedir, vf->uva);
    if(!visit_recent)
    {
      victim = vf;
      break;
    }
    else
    {
      pagedir_set_accessed (t->pagedir, vf->uva, false);
    }
    e = next;
    if(e == list_begin(&frame_table))
    {
      round_cnt++;
      if(round_cnt > 3)
        return NULL; // Cannot pick a victim
    }
  }
  return victim;
}

/* save evicted frame's content for later swap in */
static bool
bookkeep_eviction (struct frame_table_entry *vf)
{
  struct thread *t = vf->owner;
  struct sup_pte *spte;

  /* Retrieve the entry from the hash table */
  spte = get_addr_pte (&t->sup_page_table, vf->uva);

  if (!spte)
    {
      spte = calloc(1, sizeof(struct sup_pte));
      spte->type = SWAP;
      spte->user_vaddr = vf->uva;
      if (!insert_sup_pte (&t->sup_page_table, spte))
        return false;
    }

  size_t swap_idx;

  if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) && (spte->type == MMF))
    {
	  write_mmf_back (spte);
    }
  else if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) || (spte->type != FILE))
    {
      swap_idx = swap_out (spte->user_vaddr);
      spte->type = spte->type | SWAP;
    }

  memset (vf->frame, 0, PGSIZE);

  spte->loaded = false;
  spte->swap_index = swap_idx;
  spte->writable = *(vf->pte) & PTE_W;

  pagedir_clear_page (t->pagedir, spte->uvaddr);

  return true;
}

/* Get the vm_frame struct, whose frame contribute equals the given frame, from
 * the frame list frame_table. */
static struct frame_table_entry *
get_frame (void *frame)
{
  struct frame_table_entry *vf;
  struct list_elem *e = list_begin (&frame_table);

  lock_acquire (&frame_lock);
  for (; e != list_end(&frame_table); e = list_next (e))
    {
      vf = list_entry (e, struct frame_table_entry, elem);
      if (vf->frame == frame)
        return vf;
    }
  lock_release (&frame_lock);

  return NULL;
}
