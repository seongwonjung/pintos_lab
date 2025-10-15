/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_bitmap;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
    /* TODO: Set up the swap_disk. */
    swap_disk = disk_get(1, 1);
    if (swap_disk != NULL)
        swap_bitmap = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;
    anon_page->swap_slot = -1;

    return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva)
{
    struct anon_page *anon_page = &page->anon;
    if (anon_page->swap_slot == -1)
        return true;

    // anon_page->swap_slot 에서 읽어오기
    size_t idx = anon_page->swap_slot;
    for (int i = 0; i < 8; i++)
        disk_read(swap_disk, idx + i, kva + (i * DISK_SECTOR_SIZE));
    // 비트맵 다시 사용 가능 상태로 돌려놓기
    bitmap_set_multiple(swap_bitmap, idx, 8, false);
    anon_page->swap_slot = -1;
    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    // 1. 비트맵에서 비어있는 스왑 슬롯(페이지 단위) 1개를 찾는다
    size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 8, false);
    if (idx == BITMAP_ERROR)
    {
        // 만약 디스크에 더 이상 비어있는 슬롯이 없다면, 커널 패닉
        PANIC("There are no more empty slots on the disk");
    }
    // 2. 찾은 슬롯에 페이지의 내용(frame->kva)을 쓴다
    for (int i = 0; i < 8; i++)
        disk_write(swap_disk, idx + i, page->frame->kva + (i * DISK_SECTOR_SIZE));
    // 3. 페이지 구조체에 스왑 슬롯 인덱스를 저장
    anon_page->swap_slot = idx;
    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    // 페이지가 스왑 디스크에 있다면, 해당 스왑 슬롯을 비워줍니다.
    if (anon_page->swap_slot != -1)
    {
        bitmap_set_multiple(swap_bitmap, anon_page->swap_slot, 8, false);
    }
}
