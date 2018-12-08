#ifndef SWAP_H
#define SWAP_H

void swap_init (void);
size_t swap_out (void* page_idx);
void swap_in (size_t aim_swap_page, void* page_idx);
void swap_clear (size_t aim_swap_page);

#endif