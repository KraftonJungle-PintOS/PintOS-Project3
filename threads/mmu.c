#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "intrinsic.h"

// 페이지 계층 순서 PML4 -> PDPT -> PD -> PTE

/* 함수 설명 */
// PD에서 PTE를 탐색하는 함수
/* 매개 변수 */
// pdp: 페이지 디렉터리 포인터 테이블(PDPT)의 시작 주소
// va: 찾고자 하는 가상 주소
// create: 1: PTE가 없으면 새로 생성, 0: PTE가 없으면 NULL 반환
/* 반환 값 */
// 가상 주소에 해당하는 페이지 테이블 엔트리(PTE)의 주소
// 찾거나 생성에 실패하면 NULL 반환
static uint64_t *
pgdir_walk (uint64_t *pdp, const uint64_t va, int create) {
	int idx = PDX (va);
	if (pdp) {
		uint64_t *pte = (uint64_t *) pdp[idx];
		if (!((uint64_t) pte & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page)
					pdp[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
				else
					return NULL;
			} else
				return NULL;
		}
		return (uint64_t *) ptov (PTE_ADDR (pdp[idx]) + 8 * PTX (va));
	}
	return NULL;
}

/* 함수 설명 */
// PDPT에서 PD를 탐색하는 함수
/* 매개 변수 */
// pdpe: 페이지 디렉터리 포인터 엔트리(PDPE) 테이블의 시작 주소
// va: 찾고자 하는 가상 주소
// create:
// 1: 필요한 경우 하위 테이블(PD 및 PTE)을 새로 생성
// 0: 필요한 테이블이 없으면 NULL 반환
/* 반환 값 */
// 가상 주소에 해당하는 페이지 테이블 엔트리(PTE)의 주소
// 찾거나 생성에 실패하면 NULL 반환
static uint64_t *
pdpe_walk (uint64_t *pdpe, const uint64_t va, int create) {
	uint64_t *pte = NULL;
	int idx = PDPE (va);
	int allocated = 0;
	if (pdpe) {
		uint64_t *pde = (uint64_t *) pdpe[idx];
		if (!((uint64_t) pde & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page) {
					pdpe[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
					allocated = 1;
				} else
					return NULL;
			} else
				return NULL;
		}
		pte = pgdir_walk (ptov (PTE_ADDR (pdpe[idx])), va, create);
	}
	if (pte == NULL && allocated) {
		palloc_free_page ((void *) ptov (PTE_ADDR (pdpe[idx])));
		pdpe[idx] = 0;
	}
	return pte;
}

/* 함수 설명 */
// PML4에서 PDPT를 탐색하는 함수
/* 매개 변수 */
// pml4e: PML4 엔트리 배열의 시작 주소
// va: 찾고자 하는 가상 주소
// create:
// 1: 필요한 경우 하위 테이블(PDPT 및 PTE)을 새로 생성
// 0: 필요한 테이블이 없으면 NULL 반환
/* 반환 값 */
// 가상 주소에 해당하는 페이지 테이블 엔트리(PTE)의 주소
// 찾거나 생성에 실패하면 NULL 반환
uint64_t *
pml4e_walk (uint64_t *pml4e, const uint64_t va, int create) {
	uint64_t *pte = NULL;
	int idx = PML4 (va);
	int allocated = 0;
	if (pml4e) {
		uint64_t *pdpe = (uint64_t *) pml4e[idx];
		if (!((uint64_t) pdpe & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page) {
					pml4e[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
					allocated = 1;
				} else
					return NULL;
			} else
				return NULL;
		}
		pte = pdpe_walk (ptov (PTE_ADDR (pml4e[idx])), va, create);
	}
	if (pte == NULL && allocated) {
		palloc_free_page ((void *) ptov (PTE_ADDR (pml4e[idx])));
		pml4e[idx] = 0;
	}
	return pte;
}

/* 함수 설명 */
// 새로운 PML4(Page Map Level 4) 테이블을 생성하는 함수
/* 반환 값 */
// 생성된 PML4 테이블의 시작 주소
// 메모리 할당이 실패하면 NULL을 반환
uint64_t *
pml4_create (void) {
	uint64_t *pml4 = palloc_get_page (0);
	if (pml4)
		memcpy (pml4, base_pml4, PGSIZE);
	return pml4;
}

/* 함수 설명 */
// 페이지 테이블(PT)의 모든 엔트리를 순회하며, PTE에 대해 주어진 함수(FUNC)를 호출하는 함수
/* 매개 변수 */
// pt: 페이지 테이블(PT)의 시작 주소
// func: 각 PTE에 대해 호출할 함수의 포인터
// aux: 함수(FUNC)에 추가로 전달할 사용자 정의 데이터
// pml4_index: 현재 PT가 속한 PML4 인덱스
// pdp_index: 현재 PT가 속한 PDPT 인덱스
// pdx_index: 현재 PT가 속한 PD 인덱스
/* 반환 값 */
// 모든 엔트리에 대해 함수가 성공적으로 실행되면 true
// 하나라도 실패하면 false
static bool
pt_for_each (uint64_t *pt, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index, unsigned pdx_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = &pt[i];
		if (((uint64_t) *pte) & PTE_P) {
			void *va = (void *) (((uint64_t) pml4_index << PML4SHIFT) |
								 ((uint64_t) pdp_index << PDPESHIFT) |
								 ((uint64_t) pdx_index << PDXSHIFT) |
								 ((uint64_t) i << PTXSHIFT));
			if (!func (pte, va, aux))
				return false;
		}
	}
	return true;
}

/* 함수 설명 */
// 페이지 디렉터리(PD)의 모든 엔트리를 순회하며, PTE에 대해 주어진 함수(FUNC)를 호출하는 함수
/* 매개 변수 */
// pdp: 페이지 디렉터리(PD)의 시작 주소
// func: 각 PTE에 대해 호출할 함수의 포인터
// aux: 함수(FUNC)에 추가로 전달할 사용자 정의 데이터
// pml4_index: 현재 PD가 속한 PML4 인덱스
// pdp_index: 현재 PD가 속한 PDPT 인덱스
/* 반환 값 */
// 모든 엔트리에 대해 함수가 성공적으로 실행되면 true
// 하나라도 실패하면 false
static bool
pgdir_for_each (uint64_t *pdp, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			if (!pt_for_each ((uint64_t *) PTE_ADDR (pte), func, aux,
					pml4_index, pdp_index, i))
				return false;
	}
	return true;
}

/* 함수 설명 */
// 페이지 디렉터리 포인터 테이블(PDPT)의 모든 엔트리를 순회하며, PTE에 대해 주어진 함수(FUNC)를 호출하는 함수
/* 매개 변수 */
// pdp: 페이지 디렉터리 포인터 테이블(PDPT)의 시작 주소
// func: 각 PTE에 대해 호출할 함수의 포인터
// aux: 함수(FUNC)에 추가로 전달할 사용자 정의 데이터
// pml4_index: 현재 PDPT가 속한 PML4 인덱스
/* 반환 값 */
// 모든 엔트리에 대해 함수가 성공적으로 실행되면 true
// 하나라도 실패하면 false
static bool
pdp_for_each (uint64_t *pdp,
		pte_for_each_func *func, void *aux, unsigned pml4_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pde) & PTE_P)
			if (!pgdir_for_each ((uint64_t *) PTE_ADDR (pde), func,
					 aux, pml4_index, i))
				return false;
	}
	return true;
}

/* 함수 설명 */
// PML4 테이블의 모든 엔트리를 순회하며, PTE에 대해 주어진 함수(FUNC)를 호출하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// func: 각 PTE에 대해 호출할 함수의 포인터
// aux: 함수(FUNC)에 추가로 전달할 사용자 정의 데이터
/* 반환 값 */
// 모든 엔트리에 대해 함수가 성공적으로 실행되면 true
// 하나라도 실패하면 false
bool
pml4_for_each (uint64_t *pml4, pte_for_each_func *func, void *aux) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pdpe = ptov((uint64_t *) pml4[i]);
		if (((uint64_t) pdpe) & PTE_P)
			if (!pdp_for_each ((uint64_t *) PTE_ADDR (pdpe), func, aux, i))
				return false;
	}
	return true;
}

/* 함수 설명 */
// 페이지 테이블(PT)과 해당 엔트리를 해제하는 함수
/* 매개 변수 */
// pt: 페이지 테이블(PT)의 시작 주소
static void
pt_destroy (uint64_t *pt) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pt[i]);
		if (((uint64_t) pte) & PTE_P)
			palloc_free_page ((void *) PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pt);
}

/* 함수 설명 */
// 페이지 디렉터리(PD)와 해당 엔트리를 해제하는 함수
/* 매개 변수 */
// pdp: 페이지 디렉터리(PD)의 시작 주소
static void
pgdir_destroy (uint64_t *pdp) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			pt_destroy (PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pdp);
}

/* 함수 설명 */
// 페이지 디렉터리 포인터 테이블(PDPT)과 해당 엔트리를 해제하는 함수
/* 매개 변수 */
// pdpe: 페이지 디렉터리 포인터 테이블(PDPT)의 시작 주소
static void
pdpe_destroy (uint64_t *pdpe) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdpe[i]);
		if (((uint64_t) pde) & PTE_P)
			pgdir_destroy ((void *) PTE_ADDR (pde));
	}
	palloc_free_page ((void *) pdpe);
}

/* 함수 설명 */
// PML4 테이블과 해당 엔트리를 해제하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
void
pml4_destroy (uint64_t *pml4) {
	if (pml4 == NULL)
		return;
	ASSERT (pml4 != base_pml4);

	/* if PML4 (vaddr) >= 1, it's kernel space by define. */
	uint64_t *pdpe = ptov ((uint64_t *) pml4[0]);
	if (((uint64_t) pdpe) & PTE_P)
		pdpe_destroy ((void *) PTE_ADDR (pdpe));
	palloc_free_page ((void *) pml4);
}

/* 함수 설명 */
// PML4를 활성화하여 CPU의 페이지 디렉터리 기준 레지스터(CR3)에 로드하는 함수
/* 매개 변수 */
// pml4: 활성화할 PML4 테이블의 시작 주소
void
pml4_activate (uint64_t *pml4) {
	lcr3 (vtop (pml4 ? pml4 : base_pml4));
}

/* 함수 설명 */
// PML4에서 주어진 가상 주소(UADDR)에 해당하는 물리 주소를 반환하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// uaddr: 찾고자 하는 사용자 가상 주소
/* 반환 값 */
// 가상 주소에 매핑된 물리 주소의 커널 가상 주소, 또는 NULL (매핑되지 않은 경우)
void *
pml4_get_page (uint64_t *pml4, const void *uaddr) {
	ASSERT (is_user_vaddr (uaddr));

	uint64_t *pte = pml4e_walk (pml4, (uint64_t) uaddr, 0);

	if (pte && (*pte & PTE_P))
		return ptov (PTE_ADDR (*pte)) + pg_ofs (uaddr);
	return NULL;
}

/* 함수 설명 */
// 가상 페이지(UPAGE)를 물리 페이지(KPAGE)에 매핑하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// upage: 가상 페이지 주소
// kpage: 물리 페이지의 커널 가상 주소
// rw: true면 읽기/쓰기, false면 읽기 전용으로 매핑
/* 반환 값 */
// 매핑 성공 시 true, 실패 시 false
bool
pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw) {
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (pg_ofs (kpage) == 0);
	ASSERT (is_user_vaddr (upage));
	ASSERT (pml4 != base_pml4);

	uint64_t *pte = pml4e_walk (pml4, (uint64_t) upage, 1);

	if (pte)
		*pte = vtop (kpage) | PTE_P | (rw ? PTE_W : 0) | PTE_U;
	return pte != NULL;
}

/* 함수 설명 */
// 가상 페이지(UPAGE)를 "존재하지 않음" 상태로 설정하여 이후 접근 시 페이지 폴트를 발생시키는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// upage: 제거할 가상 페이지 주소
void
pml4_clear_page (uint64_t *pml4, void *upage) {
	uint64_t *pte;
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (is_user_vaddr (upage));

	pte = pml4e_walk (pml4, (uint64_t) upage, false);

	if (pte != NULL && (*pte & PTE_P) != 0) {
		*pte &= ~PTE_P;
		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) upage);
	}
}

/* 함수 설명 */
// 가상 페이지(VPAGE)가 수정되었는지(Dirty Bit가 설정되었는지) 확인하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// vpage: 확인할 가상 페이지 주소
/* 반환 값 */
// 페이지가 수정된 경우 true, 그렇지 않으면 false
bool
pml4_is_dirty (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_D) != 0;
}

/* 함수 설명 */
// 가상 페이지(VPAGE)의 Dirty Bit를 설정 또는 해제하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// vpage: 설정할 가상 페이지 주소
// dirty: true면 Dirty Bit를 설정, false면 해제
void
pml4_set_dirty (uint64_t *pml4, const void *vpage, bool dirty) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (dirty)
			*pte |= PTE_D;
		else
			*pte &= ~(uint32_t) PTE_D;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}

/* 함수 설명 */
// 가상 페이지(VPAGE)가 최근에 접근되었는지(Accessed Bit가 설정되었는지) 확인하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// vpage: 확인할 가상 페이지 주소
/* 반환 값 */
// 페이지가 접근된 경우 true, 그렇지 않으면 false
bool
pml4_is_accessed (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_A) != 0;
}

/* 함수 설명 */
// 가상 페이지(VPAGE)의 Accessed Bit를 설정 또는 해제하는 함수
/* 매개 변수 */
// pml4: PML4 테이블의 시작 주소
// vpage: 설정할 가상 페이지 주소
// accessed: true면 Accessed Bit를 설정, false면 해제
void
pml4_set_accessed (uint64_t *pml4, const void *vpage, bool accessed) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (accessed)
			*pte |= PTE_A;
		else
			*pte &= ~(uint32_t) PTE_A;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}
