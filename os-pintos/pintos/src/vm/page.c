#include "vm/page.h"
#include "threads/pte.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include <string.h>
#include "userprog/syscall.h"
#include "vm/swap.h"


void grow_stack (void* user_vaddr){
	void *new_page = allocate_frame (PAL_USER | PAL_ZERO);
	if (!new_page)
		return;
	bool result = pagedir_set_page (thread_current ()->pagedir, pg_round_down (user_vaddr), new_page, true);
	if (!result)
		free_frame (new_page);
}

/*used by the hash table in the thread*/
unsigned
sup_pte_hash (const struct hash_elem *ht, void *aux UNUSED)
{
	const struct sup_pte *pte;
	pte = hash_entry (ht, struct sup_pte, elem);
	return hash_bytes (&pte->user_vaddr, sizeof pte->user_vaddr);
}

bool
sup_pte_less (const struct hash_elem *hta,
const struct hash_elem *htb,
void *aux UNUSED)
{
	const struct sup_pte *ptea;
	const struct sup_pte *pteb;
	ptea = hash_entry (hta, struct sup_pte, elem);
	pteb = hash_entry (htb, struct sup_pte, elem);
	return ptea->user_vaddr < pteb->user_vaddr;
}

/*retrieve the sup_pte from the hash table*/
struct sup_pte*
get_addr_pte (struct hash* ht, void* uvaddr){
	struct sup_pte pte;
	pte.user_vaddr = uvaddr;
	struct hash_elem *elem;
	elem = hash_find (ht, &pte.elem);
	if (elem == NULL)
		return NULL;
	else return hash_entry (elem, struct sup_pte, elem);
}

bool load_back (struct sup_pte* pte){
	if (pte->type == FILE){
		struct thread* cur = thread_current ();
		file_seek (pte->file_info.file, pte->file_info.offset);
		uint8_t *newpage = allocate_frame (PAL_USER);
		if (!newpage) return false;
		uint32_t result = file_read (pte->file_info.file, newpage, pte->file_info.read_length);
		if (result != pte->file_info.read_length){
			free_frame (newpage);
			return false;
		}
		memset (newpage + pte->file_info.read_length, 0, pte->file_info.empty_length);
		pagedir_set_page (cur->pagedir, pte->user_vaddr, newpage, pte->file_info.writable);
		pte->loaded = true;
		return true;
	}
	else if (/*pte->type & 0x4*/pte->type == 0x4 || pte->type == (0x4|0x1)){
		struct thread* cur = thread_current ();
		file_seek (pte->file_info.file, pte->file_info.offset);
		uint8_t *newpage = allocate_frame (PAL_USER);
		if (!newpage) return false;
		uint32_t result = file_read (pte->file_info.file, newpage, pte->file_info.read_length);
		if (result != pte->file_info.read_length){
			free_frame (newpage);
			return false;
		}
		memset (newpage + pte->file_info.read_length, 0, PGSIZE - pte->file_info.read_length);
		pagedir_set_page (cur->pagedir, pte->user_vaddr, newpage, true);
		pte->loaded = true;
		if(pte->type & 0x1) pte->type = MMF;
		return true;
	}
	else if (/*pte->type & 0x1*/pte->type == 0x1 || pte->type == (0x2|0x1)){
		struct thread* cur = thread_current ();
		uint8_t* newpage = allocate_frame (PAL_USER);
		if (newpage == NULL)
			return false;
		pagedir_set_page (cur->pagedir, pte->user_vaddr, newpage, pte->writable);
		swap_in (pte->swap_index, pte->user_vaddr);
		if (pte->type == SWAP)
			hash_delete (&cur->sup_page_table, &pte->elem);
		if (pte->type == (0x2|0x1)){
			pte->type = FILE;
			pte->loaded = true;
		}
		return true;
	}
}

/* used by hash func to free a hash table */
void
free_sup_page_entry (struct hash_elem* elem, void* aux UNUSED){
	struct sup_pte* pte;
	pte = hash_entry (elem, struct sup_pte, elem);
	if (pte->type & SWAP)
		swap_clear (pte->swap_index);
	free (pte);
}

void
free_sup_page_table (struct hash* table){
	hash_destroy (table, free_sup_page_entry);
}

/* insert a sup_pte into a hash table */
bool
insert_sup_pte (struct hash* ht, struct sup_pte* pte){
	if (pte == NULL)
		return false;
	hash_insert (ht, &pte->elem);
	return true;
}

/* fuck bool
add_mmf_pte (struct file *file, off_t offset, uint8_t *user_page, uint32_t read_length){
	struct thread* cur = thread_current ();
	struct sup_pte* pte = malloc (sizeof *pte);

	if (pte == NULL)
		return false;

	pte->user_vaddr = user_page;
	pte->type = MMF;
	pte->file_info.file = file;
	pte->file_info.offset = offset;
	pte->file_info.read_length = read_length;
	pte->loaded = false;

	struct hash_elem *result = hash_insert (&cur->sup_page_table, &pte->elem);
	if (result == NULL)
		return false;
	return true;
}*/

void write_mmf_back (struct sup_pte* pte){
	if (pte->type != MMF)
		return;
	file_seek (pte->file_info.file, pte->file_info.offset);
	file_write (pte->file_info.file, pte->user_vaddr, pte->file_info.read_length);
}

// void fix_page (struct hash* ht, void* uvaddr){
// 	struct sup_pte *pte = get_addr_pte (ht, uvaddr);
// 	if(!pte) return;
// 	fix_frame (pte->)
// }
// void unfix_page (struct hash* ht, void* uvaddr){

// }