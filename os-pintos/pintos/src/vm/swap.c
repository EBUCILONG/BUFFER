#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/swap.h"


struct block *swap_disk;

struct bitmap *swap_table;

static size_t NUM_SECTORS_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

size_t swap_page_num (void){
	return block_size (swap_disk) / NUM_SECTORS_PAGE;
}

void swap_init (void){
	swap_disk = block_get_role (BLOCK_SWAP);
	if (swap_disk == NULL) ASSERT (0);
	// printf("swap_page_num = %d\n", swap_page_num());
	swap_table = bitmap_create (swap_page_num ());
	ASSERT (!swap_table == NULL);
	bitmap_set_all (swap_table, true);
}

size_t swap_out (void* page_idx){
	size_t free_page = bitmap_scan_and_flip (swap_table, 0, 1, true);
	if(free_page == BITMAP_ERROR) return SIZE_MAX;
	for (int i = 0; i < NUM_SECTORS_PAGE; i++)
		block_write (swap_disk, free_page * NUM_SECTORS_PAGE + i, page_idx + i * BLOCK_SECTOR_SIZE);
	return free_page;
}

void swap_in (size_t aim_swap_page, void* page_idx){
	ASSERT (!(aim_swap_page == BITMAP_ERROR || page_idx == NULL));
	for (int i = 0; i < NUM_SECTORS_PAGE; i++)
		block_read (swap_disk, aim_swap_page * NUM_SECTORS_PAGE + i, page_idx + i * BLOCK_SECTOR_SIZE);

	bitmap_flip (swap_table, aim_swap_page);
}

void swap_clear (size_t aim_swap_page){
	bitmap_flip (swap_table, aim_swap_page);
}
