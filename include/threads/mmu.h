#ifndef THREAD_MMU_H
#define THREAD_MMU_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"

// PTE(Page Table Entry)를 순회하면서 각 PTE에 대해 실행할 함수의 형식
// 순회 함수(pml4_for_each)에서 사용
// uint64_t *pte: 현재 PTE의 포인터
// void *va: 해당 PTE가 매핑된 가상 주소
// void *aux: 추가적인 데이터를 전달하기 위한 포인터
typedef bool pte_for_each_func (uint64_t *pte, void *va, void *aux);

// 가상 주소 va에 해당하는 PTE를 찾아 반환
// create가 참이면, 해당 PTE가 없을 경우 생성
uint64_t *pml4e_walk (uint64_t *pml4, const uint64_t va, int create);

// 새로운 PML4 테이블을 생성
// 반환값: 생성된 PML4 테이블의 포인터
uint64_t *pml4_create (void);

// PML4와 연관된 모든 PTE를 순회하며, 각 PTE에 대해 주어진 함수를 실행
bool pml4_for_each (uint64_t *, pte_for_each_func *, void *);

// PML4 테이블을 삭제하고 관련 리소스를 해제
void pml4_destroy (uint64_t *pml4);

// 특정 PML4를 활성화 (현재 스레드의 페이지 테이블로 설정)
void pml4_activate (uint64_t *pml4);

// 가상 주소 upage에 매핑된 물리 주소를 반환
// 반환값: 매핑된 물리 주소(없으면 NULL)
void *pml4_get_page (uint64_t *pml4, const void *upage);

// 가상 주소 upage를 물리 주소 kpage에 매핑
// rw는 읽기/쓰기 권한 설정 여부를 나타냄
bool pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw);

// 특정 가상 주소 upage의 매핑을 해제
void pml4_clear_page (uint64_t *pml4, void *upage);

// 페이지가 수정되었는지(Dirty Bit) 확인
bool pml4_is_dirty (uint64_t *pml4, const void *upage);

// Dirty Bit를 설정하거나 해제
void pml4_set_dirty (uint64_t *pml4, const void *upage, bool dirty);

// 페이지가 접근되었는지(Accessed Bit) 확인
bool pml4_is_accessed (uint64_t *pml4, const void *upage);

// Accessed Bit를 설정하거나 해제
void pml4_set_accessed (uint64_t *pml4, const void *upage, bool accessed);

// 해당 PTE가 쓰기 가능한지 확인
#define is_writable(pte) (*(pte) & PTE_W)

// 해당 PTE가 사용자 공간과 관련된지 확인
#define is_user_pte(pte) (*(pte) & PTE_U)

// 해당 PTE가 커널 공간과 관련된지 확인
#define is_kern_pte(pte) (!is_user_pte (pte))

// PTE에 저장된 물리 주소를 반환
#define pte_get_paddr(pte) (pg_round_down(*(pte)))

// 세그먼트 디스크립터 테이블을 나타냄
struct desc_ptr {
	// 디스크립터 테이블의 크기
	uint16_t size;
	// 디스크립터 테이블의 시작 주소
	uint64_t address;
} __attribute__((packed));

#endif /* thread/mm.h */
