/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &file_ops;

    struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
    struct thread *cur = thread_current();

    uintptr_t upage = (uintptr_t)addr;
    size_t remaining_bytes = length;
    while (upage < (uintptr_t)addr + length)
    {
        size_t page_read_bytes = remaining_bytes < PGSIZE ? remaining_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct segment_info *aux = (struct segment_info *)malloc(sizeof(struct segment_info));
        aux->file = file_reopen(file);
        aux->ofs = offset;
        aux->read_byte = page_read_bytes;
        aux->zero_byte = page_zero_bytes;

        if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_segment, aux))
        {
            file_close(aux->file);
            free(aux);
            supplemental_page_table_kill(&cur->spt);
            return false;
        }
        /* Advance. */
        remaining_bytes -= page_read_bytes;
        offset += page_read_bytes;
        upage += PGSIZE;
    }
}

/* Do the munmap */
void do_munmap(void *addr)
{
}
