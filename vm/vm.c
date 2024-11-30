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

// Project 3: Anonymous Page
// 초기화되지 않은 페이지(uninit 페이지)를 생성하고, 이를 보조 페이지 테이블(SPT)에 추가하는 함수
// 페이지 폴트 시 호출되어 데이터를 메모리에 로드
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
                                    vm_initializer *init, void *aux)
{

    ASSERT(VM_TYPE(type) != VM_UNINIT)
    // 보조 테이블의 정보를 가져옴
    struct supplemental_page_table *spt = &thread_current()->spt;

    // upage 주소를 가지는 페이지가 보조 테이블 내에 존재하는지 확인
    // 만약 없다면 보조 테이블에 정보 추가
    /* Check wheter the upage is already occupied or not. */
    if (spt_find_page(spt, upage) == NULL)
    {
        // 페이지 구조체 생성
        struct page *page = malloc(sizeof(struct page));
        if (page == NULL)
            return false; // 메모리 할당 실패

        // 페이지 초기화자 설정 (페이지 타입에 따라 선택)
        bool (*initializer)(struct page *, enum vm_type, void *) = NULL;
        switch (VM_TYPE(type))
        {
        case VM_ANON:
            initializer = anon_initializer; // 익명 페이지 초기화 함수
            break;
        case VM_FILE:
            initializer = file_backed_initializer; // 파일 기반 페이지 초기화 함수
            break;
        default:
            free(page); // 잘못된 타입이면 메모리 해제
            return false;
        }

        // 초기화되지 않은 페이지 생성
        uninit_new(page, upage, init, type, aux, initializer);

        // writable 초기화
        page->writable = writable;

        // 보조 페이지 테이블에 삽입
        if (!spt_insert_page(spt, page))
        {
            free(page); // 삽입 실패 시 메모리 해제
            return false;
        }

        return true; // 성공적으로 페이지 생성 및 삽입
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
    temp.va = pg_round_down(va);

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
    return hash_insert(&spt->page_table, &page->hash_elem) ? false : true; // 존재하지 않으면 삽입
}

/* Project 3: Memory Management */
/* Clock(Second-Chance) 알고리즘을 기반으로 페이지 교체 정책을 구현 */

// Project 3: Swap In/Out
/* Frame 테이블에서 교체할 frame(victim)을 선택하는 함수 */
static struct frame *
vm_get_victim(void)
{
    static struct list_elem *clock_ptr = NULL;

    if (clock_ptr == NULL || clock_ptr == list_end(&frame_table))
    {
        // clock_ptr 초기화 또는 끝에 도달한 경우 처음으로 이동
        clock_ptr = list_begin(&frame_table);
    }

    // Clock 알고리즘을 사용하여 victim을 선택
    while (true)
    {
        struct frame *frame = list_entry(clock_ptr, struct frame, frame_elem);

        // 페이지의 접근 비트를 확인
        if (!pml4_is_accessed(thread_current()->pml4, frame->page->va))
        {
            // 접근 비트가 0인 경우 교체 대상으로 선택
            return frame;
        }
        else
        {
            // 접근 비트를 초기화하고 다음 프레임으로 이동
            pml4_set_accessed(thread_current()->pml4, frame->page->va, false);
            clock_ptr = list_next(clock_ptr);

            // 끝에 도달한 경우 처음으로 다시 순회
            if (clock_ptr == list_end(&frame_table))
                clock_ptr = list_begin(&frame_table);
        }
    }
}

// Project 3: Swap In/Out
/* victim frame을 스왑 공간으로 내보내고 해당 frame을 반환 */
static struct frame *
vm_evict_frame(void)
{
    struct frame *victim = vm_get_victim();

    // victim의 페이지가 존재하면 스왑 아웃
    if (victim && victim->page)
    {
        if (!swap_out(victim->page))
        {
            PANIC("Failed to swap out page.");
        }
    }

    return victim; // 스왑 아웃된 frame 반환
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
    // 프레임 구조체 할당
    struct frame *frame = malloc(sizeof(struct frame));
    ASSERT(frame != NULL);

    frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

    if (frame->kva == NULL)
        frame = vm_evict_frame();
    else
        list_push_back(&frame_table, &frame->frame_elem);

    frame->page = NULL; // 초기에는 연결된 페이지가 없음

    ASSERT(frame->page == NULL);

    return frame; // 초기화된 프레임 반환
}

// Project 3: Stack Growth
// 이 함수는 스택이 늘어나는 경우 호출되며, 주어진 주소에 새 페이지를 할당하여 스택을 확장
static void
vm_stack_growth(void *addr UNUSED)
{
    // 페이지 경계로 주소 정렬
    // 주소를 페이지 크기(4KB)에 맞게 아래쪽으로 정렬합니다.
    void *aligned_addr = pg_round_down(addr);

    // 새로운 페이지를 스택에 할당
    // 익명 페이지(VM_ANON)를 생성하고, 해당 페이지를 스택용으로 표시(VM_MARKER_0)
    // true: 페이지를 쓰기 가능으로 설정
    if (!vm_alloc_page(VM_ANON | VM_MARKER_0, aligned_addr, true))
    {
        // 페이지 할당에 실패하면 커널 패닉 발생
        PANIC("Stack growth failed: Could not allocate a new page.");
    }

    // 방금 할당한 페이지를 즉시 클레임
    // 페이지를 활성화하여 실제로 사용할 수 있도록 설정
    if (!vm_claim_page(aligned_addr))
    {
        // 클레임에 실패하면 커널 패닉 발생
        PANIC("Stack growth failed: Could not claim the new page.");
    }
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

// Project 3: Anonymous Page
// 페이지 폴트가 발생했을 때 호출되며, 페이지 폴트를 처리하는 함수
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;

    // 주소가 null이거나 페이지 폴트가 적합하지 않은 경우 false 반환
    if (addr == NULL || is_kernel_vaddr(addr) || !not_present)
        return false;

    // 보조 페이지 테이블에서 페이지를 찾기
    struct page *page = spt_find_page(spt, addr);

    // Project 3: Stack Growth
    // 페이지가 NULL이고 주소가 유효한 스택 확장 범위 내라면 스택 확장 시도
    if (page == NULL && addr >= f->rsp - 8 && addr < (void *)USER_STACK)
    {
        vm_stack_growth(addr);
        return true; // 스택 확장 성공
    }

    // 페이지가 존재하지 않거나, 쓰기 권한 검사 실패 시 false 반환
    if (page == NULL || (write && !page->writable))
        return false;

    // 페이지를 실제로 메모리에 매핑
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
// 가상주소에 맞는 page가 spt에 매핑되어있는지 확인하고 vm_do_claim_page를 호출하여 실제 물리 메모리에 매핑하는 함수
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
// page에 대한 새로운 물리 frame을 할당 받고 swap_in를 호출하여 할당받은 프레임에 page 데이터를 로드하는 함수
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

// Project 3: Anonymous Page
// 부모 페이지의 subpage table을 자식 페이지의 subpage table에 복제하는 함수
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
                                  struct supplemental_page_table *src)
{
    struct hash_iterator iter;
    struct page *dst_page;
    struct aux *aux;

    hash_first(&iter, &src->page_table);

    while (hash_next(&iter))
    {
        struct page *src_page = hash_entry(hash_cur(&iter), struct page, hash_elem);
        enum vm_type type = src_page->operations->type;
        void *upage = src_page->va;
        bool writable = src_page->writable;

        switch (type)
        {
        case VM_UNINIT: // src 타입이 initialize 되지 않았을 경우
            if (!vm_alloc_page_with_initializer(page_get_type(src_page), upage, writable, src_page->uninit.init, src_page->uninit.aux))
                goto err;
            break;

        case VM_FILE: // src 타입이 FILE인 경우
            if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, &src_page->file))
                goto err;

            dst_page = spt_find_page(dst, upage); // 대응하는 물리 메모리 데이터 복제
            if (!file_backed_initializer(dst_page, type, NULL))
                goto err;

            dst_page->frame = src_page->frame;
            if (!pml4_set_page(thread_current()->pml4, dst_page->va, src_page->frame->kva, src_page->writable))
                goto err;
            break;

        case VM_ANON:                                  // src 타입이 anon인 경우
            if (!vm_alloc_page(type, upage, writable)) // UNINIT 페이지 생성 및 초기화
                goto err;
            break;

        default:
            goto err;
        }
    }

    return true;

err:
    return false;
}

// Project 3: Anonymous Page
// 보조 페이지 테이블(supplemental page table)에 의해 점유된 리소스를 해제하는 함수
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
    hash_clear(&spt->page_table, hash_destructor); // 해시 테이블의 모든 요소 제거
}
