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

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}
