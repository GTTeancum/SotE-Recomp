#pragma once
#include "recomp.h"
/* Only the scheduled guest executes game code. Host events never execute it. */
typedef struct xb_thread xb_thread;
typedef void (*xb_entry)(uint8_t*, recomp_context*);
void xb_runtime_init(uint8_t* memory);
xb_thread* xb_thread_create(xb_entry entry, uint32_t arg, uint32_t stack, int priority);
void xb_thread_start(xb_thread* thread);
void xb_thread_join(xb_thread* thread);
void xb_thread_stop(xb_thread* thread);
void xb_thread_destroy(xb_thread* thread);
void xb_thread_priority(xb_thread* thread, int priority);
int xb_thread_get_priority(xb_thread* thread);
void xb_thread_yield(void);
void xb_wait_frame(void);
void xb_frame_tick(void);
void xb_queue_create(uint32_t queue, uint32_t messages, int capacity);
int xb_queue_send(uint32_t queue, uint32_t message, int block, int jam);
int xb_queue_recv(uint32_t queue, uint32_t output, int block);
unsigned xb_runtime_selftest(uint8_t* memory);
