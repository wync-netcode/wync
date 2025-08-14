#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// FIFORing

// Usage example:
// #define FIFORING_TYPE double
// #define FIFORING_PREFIX CustomName  (optional)
// #include "fiforing.h"
// double_FIFORing myfifo = double_FIFORing_create();

// user didn't specify type, using default
#ifndef FIFORING_TYPE
#define FIFORING_TYPE double
#endif

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)
#ifndef FIFORING_PREFIX
#define FIFORING_PREFIX TOKCAT(FIFORING_TYPE, _)
#endif
#define PRE(name) TOKCAT(FIFORING_PREFIX, name)

// my types
#define TYPE     FIFORING_TYPE
#define FIFORING PRE(FIFORing)     // example: double_FIFORing
#define i32 int32_t
#define u32 uint32_t
#define i64 int64_t
#define u64 uint64_t
#define OK 0


typedef struct {
    u32   capacity;
    u32   tail;
    u32   head;
    u32   size;
    TYPE *buffer;
} FIFORING;

void PRE(FIFORing_calloc) (FIFORING *ring, u32 p_capacity) {
    ring->capacity = p_capacity;
    ring->buffer = (TYPE*)calloc(sizeof(TYPE), ring->capacity);
}

FIFORING PRE(FIFORing_init) (u32 p_capacity) {
    FIFORING ring = { 0 };
    PRE(FIFORing_calloc) (&ring, p_capacity);
    return ring;
}

// @returns error
// @retval  0 OK
// @retval -1 error
int PRE(FIFORing_push_head) (FIFORING *ring, TYPE item) {
	if (ring->size >= ring->capacity) {
		return -1;
    } else if (ring->size == 0) {
		ring->head = 0;
		ring->tail = 0;
    } else {
		++ring->head;
		if (ring->head >= ring->capacity)
            { ring->head = 0; }
    }
	ring->buffer[ring->head] = item;
	++ring->size;
	return OK;
}

// Similar to push_head but doesn't insert
// so it reuses whatever it's already present in that slot
// @returns error
// @retval  0 OK
// @retval -1 error
i32 PRE(FIFORing_extend_head) (FIFORING *ring) {
	if (ring->size >= ring->capacity) {
		return -1;
    } else if (ring->size == 0) {
		ring->head = 0;
		ring->tail = 0;
    } else {
		++ring->head;
		if (ring->head >= ring->capacity)
            { ring->head = 0; }
    }
	++ring->size;
	return OK;
}

// @returns Popped item
TYPE *PRE(FIFORing_pop_tail) (FIFORING *ring) {
	if (ring->size <= 0)
        { return NULL; }
	
	TYPE *item = &ring->buffer[ring->tail];
	
	if (ring->size > 1) {
		++ring->tail;
		if (ring->tail >= ring->capacity) 
            { ring->tail = 0; }
    }
	
	--ring->size;
	return item;
}

TYPE *PRE(FIFORing_get_head) (FIFORING *ring) {
	return &ring->buffer[ring->head];
}

TYPE *PRE(FIFORing_get_tail) (FIFORING *ring) {
	return &ring->buffer[ring->tail];
}

void PRE(FIFORing_clear) (FIFORING *ring) {
    ring->tail = 0;
    ring->head = 0;
    ring->size = 0;
    memset(ring->buffer, 0, ring->capacity);
}

TYPE *PRE(FIFORing_get_relative_to_tail) (FIFORING *ring, u32 pos) {
	u32 index = (ring->tail + pos) % ring->capacity;
	return &ring->buffer[index];
}

// @returns error
// @retval  0 OK
// @retval -1 error
i32 PRE(FIFORing_remove_relative_to_tail) (FIFORING *ring, u32 pos) {
	if (ring->size <= 0) { return -1; }
	if (pos >= ring->size) { return -2; }

    u32 index_curr, index_next;
    for (u32 i = pos; i < ring->size; ++i) {
        index_curr = (ring->tail + i) % ring->capacity;
        if (i < (ring->size -1)) {
            index_next = (ring->tail + i + 1) % ring->capacity;
            ring->buffer[index_curr] = ring->buffer[index_next];
        }
        // garbage value remains on moved object previous position
    }

    ring->head = (ring->head -1) % ring->capacity;
	--ring->size;
	return OK;
}

#undef PRE
#undef FIFORING_TYPE
#undef FIFORING_PREFIX
#undef TYPE
#undef FIFORING

