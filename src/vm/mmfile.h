#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

struct lock file_lock;

struct mmfile_entry
{
  int mapid;
  struct file* mapped_file;
  void* addr;
  int pg_num;
  struct hash_elem elem;
};

// Function pointers
unsigned mmf_hash_func (const struct hash_elem *e, void * aux UNUSED);
bool mmf_less (const struct hash_elem * a, const struct hash_elem * b, void* aux UNUSED);

// insert an entry into the table
int mmf_insert (struct file* f, void *addr, int length);

void mmf_free_entry (struct mmfile_entry* mmf);

// Destroy the hash table
void mmf_destroy_table (struct hash *mmfiles);
