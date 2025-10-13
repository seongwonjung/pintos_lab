/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "filesys/filesys.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);
static bool lazy_file_segment(struct page *page, void *aux);
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
    file_page->file_info = NULL;
    return true;
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
    struct file_page *file_page = &page->file;
    struct file_info *file_info = file_page->file_info;
    if (file_info != NULL)
    {
        // 파일을 수정 했다면(dirty_bit == true) 다시 쓰기
        if (pml4_is_dirty(thread_current()->pml4, page->va))
        {
            file_write_at(file_info->file, page->frame->kva, file_info->read_byte, file_info->ofs);
        }
        file_close(file_info->file);
        free(file_info);
        file_page->file_info = NULL;
    }

    free(page->frame);
    page->frame = NULL;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
    struct thread *cur = thread_current();
    int map_count = 0;
    uint8_t *upage = addr;
    while (length > 0)
    {
        size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct file_info *aux = (struct file_info *)malloc(sizeof(struct file_info));
        if (aux == NULL)
            return NULL;
        aux->file = file_reopen(file);
        aux->ofs = offset;
        aux->read_byte = page_read_bytes;
        aux->zero_byte = page_zero_bytes;
        aux->map_cnt = map_count++;
        if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_file_segment, aux))
        {

            file_close(aux->file);
            free(aux);
            return NULL;
        }
        /* Advance. */
        length -= page_read_bytes;
        offset += page_read_bytes;
        upage += PGSIZE;
    }
    return (void *)addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
    struct thread *cur = thread_current();
    // addr = 시작 페이지 주소
    struct page *upage = spt_find_page(&cur->spt, addr);
    int target_cnt = upage->file.file_info->map_cnt;
    while (upage != NULL)
    {
        // file이 매핑되어 있는지, map_cnt 가 일치하는지
        if (upage->file.file_info->file == NULL || upage->file.file_info->map_cnt != target_cnt)
            return;
        spt_remove_page(&cur->spt, upage);
        target_cnt++;
        addr += PGSIZE;
        upage = spt_find_page(&cur->spt, addr);
    }
}

static bool lazy_file_segment(struct page *page, void *aux)
{
    struct file_info *p_aux = aux;

    void *p_kva = page->frame->kva; // 물리 프레임의 커널 주소

    size_t page_read_byte = p_aux->read_byte; // 읽을 바이트 수
    size_t page_zero_byte = p_aux->zero_byte; // 제로 바이트 수

    if (page_read_byte > 0)
        file_read_at(p_aux->file, p_kva, page_read_byte, p_aux->ofs);

    // 남은 영역 0으로 채우기
    if (page_zero_byte > 0)
        memset(p_kva + page_read_byte, 0, page_zero_byte);

    page->file.file_info = p_aux;

    return true;
}