#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

// Project 3: Memory Management
#include "lib/kernel/hash.h"

enum vm_type
{
	// 초기화되지 않은 페이지
	// 아직 구체적인 타입이 지정되지 않은 상태로, 페이지 초기화 시 사용
	VM_UNINIT = 0,

	// 익명 페이지
	// 파일과 관련 없는 페이지로, 힙, 스택, 또는 스왑 영역에서 관리
	VM_ANON = 1,

	// 파일 기반 페이지
	// 메모리에 매핑된 파일 데이터를 나타냄, 파일 시스템 프로젝트에서 자주 사용
	VM_FILE = 2,

	// 페이지 캐시로 프로젝트 4에서 사용
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */

	// 사용자 정의 상태 플래그
	// 페이지의 추가 정보를 저장하는 보조 플래그로 사용
	VM_MARKER_0 = (1 << 3),

	// 또 다른 사용자 정의 상태 플래그
	VM_MARKER_1 = (1 << 4),

	// 플래그 값의 상한선
	// 추가 플래그를 정의할 때 값이 이 범위를 초과하지 않도록 제한
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

// uninit_page, file_page, anon_page, page cache(프로젝트4)의 부모 클래스
// 구조체의 기본 멤버는 수정하거나 삭제하지 말 것
struct page
{
	// 페이지의 동작을 나타내는 변수
	// 각 페이지 타입(익명, 파일, 초기화되지 않음 등)에 따른 동작(스왑 인/아웃, 제거 등)을 구현
	const struct page_operations *operations;

	// 사용자 프로세스의 가상 주소를 저장하는 변수
	// 페이지가 매핑된 가상 메모리 주소를 나타냄
	void *va;

	// 페이지에 매핑된 물리 메모리 프레임에 대한 참조
	// 프레임이 없는 경우 NULL(스왑 영역에 존재)
	struct frame *frame;

	/* Your implementation */
	// Project 3: Anonymous Page
	bool writable;
	struct hash_elem hash_elem;

	// 페이지 타입별 데이터 저장 공간
	// 메모리 사용을 최적화하기 위해 공용체(union) 사용
	// 페이지 타입에 따라 union 값중 하나 선택하여 사용
	union
	{
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame
{
	// 프레임에 매핑된 커널 가상 주소
	void *kva;

	// 프레임에 매핑된 struct page에 대한 참조
	// 프레임이 매핑된 가상 메모리 페이지를 나타냄
	struct page *page;

	// Project 3: Memory Management
	struct list_elem frame_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */

// 페이지 동작을 정의하는 함수 포인터 테이블로, C언어에서 인터페이스 역할을 수행
struct page_operations
{
	// 페이지를 스왑 영역에서 물리 메모리로 가져오는 함수
	// 성공하면 true, 실패하면 false 반환
	bool (*swap_in)(struct page *, void *);

	// 페이지를 물리 메모리에서 스왑 영역으로 내보내는 함수
	// 성공하면 true, 실패하면 false 반환
	bool (*swap_out)(struct page *);

	// 페이지를 제거하고 관련 리소스를 정리하는 함수
	void (*destroy)(struct page *);

	// 페이지의 타입을 나타내는 열거형(enum).
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

// Project 3: Memory Management
// 페이지들의 상태를 저장하는 subpage table
// hash 자료구조를 이용하여 구현
struct supplemental_page_table
{
	struct hash page_table;
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt,
						   void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
						 bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

#endif /* VM_VM_H */
