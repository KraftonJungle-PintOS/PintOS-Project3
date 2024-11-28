#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

/** #Project 2: Argument Passing **/
void argument_stack(char **argv, int argc, struct intr_frame *if_);

/** #Project 2: System Call **/
struct thread *get_child_process(int pid);
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
int process_close_file(int fd);

// Project 3: Anonymous Page
// lazy loading을 구현하기 위한 보조 구조체
// 페이지를 참조만 하고 아직 할당하지 않도록 준비하는 데이터 구조
// 페이지가 메모리에 실제로 로드되지 않은 상태에서도, 나중에 로딩할 정보(페이지 데이터의 파일 위치를 저장)를 저장
// 페이지가 아직 메모리에 존재하지 않기 때문에 Page Fault가 발생하며 발생시 struct aux 정보를 참고
struct aux{
    // 로드할 대상 파일에 대한 포인터
    struct file *file;
    // 파일에서 읽기 시작할 위치
    off_t offset;
    // 해당 페이지에서 파일로부터 읽어야 하는 바이트 수
    size_t page_read_bytes;
};

// Project 3: Memory Mapped Files
bool lazy_load_segment(struct page *page, void *aux);

#endif /* userprog/process.h */