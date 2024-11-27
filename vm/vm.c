/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

// Project 3: Memory Management
// 페이지 프레임 테이블을 전역으로 선언
struct list frame_table;

// 가상 메모리의 subsystem을 초기화 하는 함수
//  각 서브시스템의 초기화 코드를 호출하여, 가상 메모리 관련 기능들을 설정
void vm_init(void)
{
    // Anonymous page 초기화 함수
    vm_anon_init();
    // File-backed Page 초기화 함수
    vm_file_init();
#ifdef EFILESYS /* For project 4 */
    pagecache_init();
#endif
    register_inspect_intr();
    /* DO NOT MODIFY UPPER LINES. */

    // Project 3: Memory Management
    // 페이지 프레임 테이블 초기화
    list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
    int ty = VM_TYPE(page->operations->type);
    switch (ty)
    {
    case VM_UNINIT:
        return VM_TYPE(page->uninit.type);
    default:
        return ty;
    }
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
                                    vm_initializer *init, void *aux)
{

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    if (spt_find_page(spt, upage) == NULL)
    {
        /* TODO: Create the page, fetch the initialier according to the VM type,
         * TODO: and then create "uninit" page struct by calling uninit_new. You
         * TODO: should modify the field after calling the uninit_new. */

        /* TODO: Insert the page into the spt. */
    }
err:
    return false;
}

// Project 3: Memory Management
// 가상 주소 va와 일치하는 페이지를 해쉬 자료형 spt에서 찾는 함수
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
    // 검색 대상 키를 위한 임시 페이지 구조체 생성
    struct page temp;
    struct hash_elem *e;

    // 검색 키 설정: 가상 주소를 기반으로 검색
    temp.va = va;

    // 해시 테이블에서 검색
    e = hash_find(&spt->page_table, &temp.hash_elem);

    // 검색 결과가 존재하면 해당 페이지를 반환, 없으면 NULL 반환
    if (e != NULL)
    {
        return hash_entry(e, struct page, hash_elem);
    }
    return NULL; // 검색 실패
}

// Project 3: Memory Management
// 페이지를 해쉬 자료형 spt에 삽입하는 함수
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{
    // 삽입 성공 여부를 나타내는 변수
    bool succ = false;

    // 삽입하려는 페이지의 가상 주소가 이미 존재하는지 확인
    if (spt_find_page(spt, page->va) == NULL)
    {
        // 중복된 페이지가 없으면 해시 테이블에 삽입
        struct hash_elem *result = hash_insert(&spt->page_table, &page->hash_elem);
        if (result == NULL)
        {
            // 삽입 성공
            succ = true;
        }
    }

    return succ; // 삽입 성공 여부 반환
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
    struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
    struct frame *victim UNUSED = vm_get_victim();
    /* TODO: swap out the victim and return the evicted frame. */

    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/

// Project 3: Memory Management
// 새로운 페이지 프레임을 할당받고 반환하는 함수
static struct frame *
vm_get_frame(void)
{
    // 사용자 메모리 풀에서 물리 페이지 할당
    void *kpage = palloc_get_page(PAL_USER);

    // 물리 페이지 할당 실패 처리
    if (kpage == NULL)
    {
        PANIC("todo: handle page allocation failure"); // 현재는 PANIC으로 처리
    }

    // 프레임 구조체 할당
    struct frame *frame = malloc(sizeof(struct frame));
    if (frame == NULL)
    {
        PANIC("todo: handle frame allocation failure"); // 메모리 부족 시 PANIC 처리
    }

    // 프레임 초기화
    frame->kva = kpage; // 할당된 커널 가상 주소 저장
    frame->page = NULL; // 초기에는 연결된 페이지가 없음

    ASSERT(frame != NULL);
    ASSERT(frame->page == NULL);

    return frame; // 초기화된 프레임 반환
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;
    /* TODO: Validate the fault */
    /* TODO: Your code goes here */

    return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
// Project 3: Memory Management
bool vm_claim_page(void *va)
{
    // 현재 스레드의 보조 페이지 테이블 가져오기
    struct supplemental_page_table *spt = &thread_current()->spt;

    // 주어진 가상 주소에 해당하는 페이지를 검색
    struct page *page = spt_find_page(spt, va);
    if (page == NULL)
    {
        // 페이지가 존재하지 않으면 새 페이지 생성
        page = malloc(sizeof(struct page));
        if (page == NULL)
        {
            return false; // 메모리 할당 실패
        }
        // 새 페이지 초기화
        page->va = va;
        page->frame = NULL;
        page->operations = NULL; // 필요한 경우 초기화
        if (!spt_insert_page(spt, page))
        {
            free(page); // 삽입 실패 시 메모리 해제
            return false;
        }
    }

    // 페이지를 클레임 (프레임 할당 및 매핑 설정)
    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
// Project 3: Memory Management
static bool
vm_do_claim_page(struct page *page)
{
    // 새로운 물리 프레임을 할당
    struct frame *frame = vm_get_frame();
    if (frame == NULL)
    {
        return false; // 프레임 할당 실패
    }

    /* Set links */
    frame->page = page;  // 프레임에 페이지 연결
    page->frame = frame; // 페이지에 프레임 연결

    /* Insert page table entry to map page's VA to frame's PA */
    bool success = pml4_set_page(thread_current()->pml4, page->va, frame->kva, true);
    if (!success)
    {
        // 페이지 테이블에 매핑 실패
        palloc_free_page(frame->kva); // 물리 프레임 반환
        free(frame);                  // 프레임 구조체 반환
        return false;
    }

    // 페이지 데이터 로드 (swap_in)
    return swap_in(page, frame->kva);
}

// Project 3: Memory Management
// 실행중인 프로세스의 subpage table을 초기화 하는 함수
// process_create_initd 함수에서 호출되어 초기화 단계에서 호출됨
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
    hash_init(&spt->page_table, page_hash_func, page_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
    /* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
}
