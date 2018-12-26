#include "mmfile.h"

static void mmf_hash_destroy_func (struct hash_elem *e, void *aux UNUSED);

int
mmf_insert (struct file* f, void *addr, int length)
{
  struct thread *cur = thread_current ();
  struct mmfile_entry *mmf = calloc (1, sizeof(struct mmfile_entry));
  if(!mmf) return -1;
  mmf->mapid = cur->mapid;
  cur->mapid++;
  mmf->mapped_file = f;
  mmf->addr = addr;

  // Count the page and create corresponding suppl_page_table_entry
  int pg_num = 0;
  int offset = 0;
  int remain = length;
  uint32_t chunk;
  while (offset < length)
  {
    if(remain < PGSIZE)
      chunk = PGSIZE;
    else
      chunk = length - remain;
    if(!add_mmf_pte (f, offset, addr, chunk)) return -1;
    pg_num++;
    remain -= chunk;
    offset += PGSIZE;
    addr += PGSIZE;
  }
  mmf->pg_num = pg_num;
  if(!hash_insert (&cur->mmfiles, &mmf->elem)) return -1;
  return mmf->mapid;
}

void mmf_free_entry (struct mmfile_entry *mmf)
{
  struct thread *cur = thread_current ();
  struct sup_pte spte;
  struct sup_pte *spte_p;
  struct hash_table;
  struct hash_elem *e;
  int offset = 0;
  int pg_num = mmf->pg_num;
  while (pg_num > 0)
  {
    spte.user_vaddr = mmf->addr + offset;
    e = hash_delete (&cur->sup_page_table, &spte.elem);
    if(e != NULL)
    {
      spte_p = hash_entry (e, struct sup_pte, elem);
      if (pagedir_is_dirty (cur->pagedir, spte_p->user_vaddr) && spte_p->loaded)
      {
        lock_acquire (&file_lock);
        file_seek (spte_p->mmf_info.file, spte_p->mmf_info.offset);
        file_write (spte_p->mmf_info.file, spte_p->user_vaddr, spte_p->mmf_info.read_position);
        lock_release (&file_lock);
      }
      free (spte_p);
    }
    offset += PGSIZE;
  }
  lock_acquire (&file_lock);
  file_close (mmf->mapped_file);
  free (mmf);
  lock_release (&file_lock);
  pg_num--;
}

void mmf_destroy_table (struct hash *mmfiles)
{
  hash_destroy (mmfiles, mmf_hash_destroy_func);
}

static void
mmf_hash_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  struct mmfile_entry *mmf = hash_entry (e, struct mmfile_entry, elem);
  mmf_free_entry (mmf);
}
