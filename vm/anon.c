/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
// Project 3: Anonymous Page
#include "lib/kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
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

// Project 3: Anonymous Page
// 디스크의 스왑 공간을 bitmap 자료형을 이용하여 전역으로 선언
struct bitmap *swap_table;
// 스왑공간의 최대 slot 개수를 가리키는 변수
size_t slot_max;

// Project 3: Anonymous Page
// 스왑 공간 초기화 함수
void vm_anon_init(void)
{
	// 디스크의 스왑 영역 정보를 가져옴
	swap_disk = disk_get(1, 1);
	// 디스크의 스왑 영역에서 슬롯의 크기만큼 나눠 최대 slot 개수 설정
	slot_max = disk_size(swap_disk) / SLOT_SIZE;
	// 스왑 공간을 나타내는 비트맵을 bitmap_create 함수를 이용하여 설정
	swap_table = bitmap_create(slot_max);
}

// Project 3: Anonymous Page
// 익명 페이지(anonymous page) subpage table 초기화 함수
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	// 페이지의 초기 상태를 0으로 설정
	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));

	// 페이지의 동작(operations)을 익명 페이지 전용 동작으로 설정
	page->operations = &anon_ops;

	// 페이지의 익명 페이지 구조체를 초기화
	struct anon_page *anon_page = &page->anon;

	// 스왑 슬롯을 초기화 값으로 설정
	anon_page->swap_slot = BITMAP_ERROR;

	return true; // 초기화 성공
}



// Project 3: Swap In/Out
static bool anon_swap_in(struct page *page, void *kva) {
    struct anon_page *anon_page = &page->anon;
    size_t slot = anon_page->swap_slot;
    size_t sector = slot * SLOT_SIZE;

    if (slot == BITMAP_ERROR || !bitmap_test(swap_table, slot))
        return false;

    bitmap_set(swap_table, slot, false);

    for (size_t i = 0; i < SLOT_SIZE; i++)
        disk_read(swap_disk, sector + i, kva + DISK_SECTOR_SIZE * i);

    sector = BITMAP_ERROR;

    return true;
}

// Project 3: Swap In/Out
static bool anon_swap_out(struct page *page) {
    struct anon_page *anon_page = &page->anon;

    size_t free_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);

    if (free_idx == BITMAP_ERROR)
        return false;

    size_t sector = free_idx * SLOT_SIZE;

    for (size_t i = 0; i < SLOT_SIZE; i++)
        disk_write(swap_disk, sector + i, page->va + DISK_SECTOR_SIZE * i);

    anon_page->swap_slot = free_idx;

    page->frame->page = NULL;
    page->frame = NULL;
    pml4_clear_page(thread_current()->pml4, page->va);

    return true;
}

// Project 3: Swap In/Out
static void anon_destroy(struct page *page) {
    struct anon_page *anon_page = &page->anon;

    /** Project 3: Swap In/Out - 점거중인 bitmap 삭제 */
    if (anon_page->swap_slot != BITMAP_ERROR)
        bitmap_reset(swap_table, anon_page->swap_slot);

    /** Project 3: Anonymous Page - 점거중인 frame 삭제 */
    if (page->frame) {
        list_remove(&page->frame->frame_elem);
        page->frame->page = NULL;
        free(page->frame);
        page->frame = NULL;
    }
}
