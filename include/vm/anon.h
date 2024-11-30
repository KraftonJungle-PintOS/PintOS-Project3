#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
// Project 3: Anonymous Page
#include "threads/vaddr.h"

struct page;
enum vm_type;

// Project 3: Anonymous Page
// 스왑 공간에서의 크기 설정
#define SLOT_SIZE (PGSIZE / DISK_SECTOR_SIZE)

// Project 3: Anonymous Page
// anon_page 구조체 수현, swap out 되었을때 스왑공간 주소 저장할 swap_slot 변수 선언
struct anon_page
{
    size_t swap_slot;
};

void vm_anon_init(void);
bool anon_initializer(struct page *page, enum vm_type type, void *kva);

#endif
