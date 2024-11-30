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
/* 스왑 디스크에서 데이터를 읽어와 페이지를 메모리에 로드하는 함수 */
static bool anon_swap_in(struct page *page, void *kva)
{
	// 현재 페이지의 anon_page 정보를 가져옴
	struct anon_page *anon_page = &page->anon;

	// 스왑 슬롯 번호를 가져옴
	size_t slot = anon_page->swap_slot;

	// 슬롯 번호를 통해 디스크의 시작 섹터를 계산
	size_t sector = slot * SLOT_SIZE;

	// 스왑 슬롯이 유효하지 않거나 사용 중이지 않은 슬롯이면 실패 처리
	if (slot == BITMAP_ERROR || !bitmap_test(swap_table, slot))
		return false;

	// 스왑 테이블에서 해당 슬롯을 비어있는 상태로 표시
	bitmap_set(swap_table, slot, false);

	// 디스크에서 데이터를 읽어와 메모리(kva)에 복사
	for (size_t i = 0; i < SLOT_SIZE; i++)
		disk_read(swap_disk, sector + i, kva + DISK_SECTOR_SIZE * i);

	// 스왑 슬롯 번호를 초기화 (더 이상 사용되지 않음을 표시)
	sector = BITMAP_ERROR;

	return true; // 성공적으로 데이터 로드
}

// Project 3: Swap In/Out
/* 페이지 데이터를 메모리에서 스왑 디스크로 이동하는 함수 */
static bool anon_swap_out(struct page *page)
{
	// 현재 페이지의 anon_page 정보를 가져옴
	struct anon_page *anon_page = &page->anon;

	// 스왑 테이블에서 비어있는 슬롯을 검색하고 해당 슬롯을 사용 중으로 표시
	size_t free_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);

	// 스왑 슬롯을 찾지 못한 경우(디스크 공간 부족) 실패 처리
	if (free_idx == BITMAP_ERROR)
		return false;

	// 디스크에서 데이터 저장을 시작할 섹터 번호 계산
	size_t sector = free_idx * SLOT_SIZE;

	// 페이지의 데이터를 디스크로 저장
	for (size_t i = 0; i < SLOT_SIZE; i++)
		disk_write(swap_disk, sector + i, page->va + DISK_SECTOR_SIZE * i);

	// anon_page에 스왑 슬롯 번호 저장 (디스크에서 저장된 위치 기록)
	anon_page->swap_slot = free_idx;

	// 페이지와 프레임의 연결 해제
	page->frame->page = NULL;
	page->frame = NULL;

	// 페이지 테이블에서 가상 주소 제거
	pml4_clear_page(thread_current()->pml4, page->va);

	return true; // 스왑 아웃 성공
}

// Project 3: Swap In/Out
/* 익명 페이지(Anonymous Page)를 삭제하는 함수 */
static void anon_destroy(struct page *page)
{
	// 페이지의 anon_page 정보를 가져옴
	struct anon_page *anon_page = &page->anon;

	// 점유 중인 스왑 슬롯 초기화
	if (anon_page->swap_slot != BITMAP_ERROR)
	{
		// 스왑 테이블에서 해당 스왑 슬롯의 점유 상태를 해제
		bitmap_reset(swap_table, anon_page->swap_slot);
	}

	// 점유 중인 프레임 삭제
	if (page->frame)
	{
		// 프레임 리스트에서 현재 페이지와 연결된 프레임을 제거
		list_remove(&page->frame->frame_elem);

		// 프레임과 페이지의 연결을 해제
		page->frame->page = NULL;

		// 프레임 메모리 해제
		free(page->frame);

		// 페이지의 프레임 포인터 초기화
		page->frame = NULL;
	}
}
