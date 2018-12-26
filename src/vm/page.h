#ifndef PAGE_H
#define PAGE_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

typedef struct fp{
	struct 	 file* file;
	off_t 	 offset;
	uint32_t read_length;
	uint32_t empty_length;
	bool 	 writable;
} file_page;

typedef struct mp{
	struct file* file;
	off_t offset;
	uint32_t read_position;
} mmf_page;

enum pte_type {
	SWAP = 0x1,
	FILE = 0x2,
	MMF =  0x4
};

struct sup_pte{
	uint32_t* user_vaddr;
	enum pte_type type;
	file_page file_info;
	mmf_page mmf_info;
	bool loaded;

	size_t swap_index;
	bool writable;

	struct hash_elem elem;
};


void grow_stack (void* user_vaddr);
unsigned sup_pte_hash (const struct hash_elem *ht, void *aux UNUSED);
bool sup_pte_less (const struct hash_elem *hta, const struct hash_elem *htb, void *aux UNUSED);
struct sup_pte* get_addr_pte (struct hash* ht, void* uvaddr);
bool load_back (struct sup_pte* pte);
void free_sup_page_table (struct hash* table);
bool insert_sup_pte (struct hash* ht, struct sup_pte* pte);
bool add_file_pte (struct file *file, off_t offset, uint8_t *user_page, uint32_t read_length, uint32_t empty_length, bool writable);
bool add_mmf_pte (struct file *file, off_t offset, uint8_t *user_page, uint32_t read_length);










#endif