#include "mmfile.h"
#include "page.h"

static void mmf_hash_destroy_func (struct hash_elem *e, void *aux UNUSED);

unsigned mmf_hash_func (const struct hash_elem *a, void *aux UNUSED){
  struct mmfile_entry *me = hash_entry(a, struct mmfile_entry, elem);
  unsigned hash = hash_bytes(&me->mapid, sizeof(int));
  return hash;
}

bool mmf_descend (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  return hash_entry(a, struct mmfile_entry, elem)->mapid < hash_entry(b, struct mmfile_entry, elem)->mapid;
}

int
mmf_insert (struct file* f, void *addr, int length)
{
  struct thread *cur = thread_current ();
  struct mmfile_entry *mmf = malloc(sizeof(struct mmfile_entry));
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

    //##################################
    struct thread* cur = thread_current ();
    struct sup_pte* pte = malloc (sizeof *pte);

    if (pte == NULL) return -1;

    pte->user_vaddr=addr;
    pte->type=MMF;
    pte->file_info.file=f;
    pte->file_info.offset=offset;
    pte->file_info.read_length = chunk;
    pte->loaded = false;
    struct hash_elem *result=hash_insert(&cur->sup_page_table, &pte->elem);
    if (result == NULL)
      return -1;
    //##################################
    //if(!add_mmf_pte (f, offset, addr, chunk)) return -1;
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
        file_seek (spte_p->file_info.file, spte_p->file_info.offset);
        file_write (spte_p->file_info.file, spte_p->user_vaddr, spte_p->file_info.read_length);
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
