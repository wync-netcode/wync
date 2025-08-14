// Generic RingBuffer implementation
// PREFIX: Con -> "my CONtainers library"

// Usage example:
// #define TYPE uint
// #define RINGBUFFER_PREFIX CustomName (optional)
// #define RINGBUFFER_ENABLE_SORT       (optional)
// #include "ringbuffer.h"

// user didn't specify type, using default
#ifndef RINGBUFFER_TYPE
#define RINGBUFFER_TYPE double
#endif

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)

#ifndef RINGBUFFER_PREFIX
#define RINGBUFFER_PREFIX TOKCAT(RINGBUFFER_TYPE, _)
#endif

#define PRE(name) TOKCAT(RINGBUFFER_PREFIX, name)

#define TYPE RINGBUFFER_TYPE
#define OK 0

#include "stdlib.h"
#include "string.h"
#include "time.h"

// NOTE: DEBUG flag only: assert(denominator is power of 2)
// denominator must be a power of 2
#define FAST_MODULUS(numerator, denominator) ((numerator) & (denominator - 1))


typedef struct {
    size_t head_pointer;
    size_t size;
    TYPE *buffer;
} PRE(RinBuf);


// @param   p_size MUST be a power of two
// @returns        New RingBuffer
PRE(RinBuf) PRE(RinBuf_create) (size_t p_size, TYPE default_value) {
    PRE(RinBuf) ring = { 0 };
    ring.size = p_size;
    ring.buffer = (TYPE *)malloc(sizeof(TYPE) * ring.size);
    for (size_t i = 0; i < ring.size; ++i) {
        ring.buffer[i] = default_value;
    }
    return ring;
}


// @param i    position in the ring
// @param item 
void PRE(RinBuf_insert_at) (PRE(RinBuf) *r, size_t index, TYPE item) {
    if (r->size == 0) return;
    r->buffer[FAST_MODULUS(index, r->size)] = item;
}


TYPE *PRE(RinBuf_get_absolute) (PRE(RinBuf) *r, size_t index) {
    if (r->size == 0) return NULL;
    return &r->buffer[index];
}


TYPE *PRE(RinBuf_get_at) (PRE(RinBuf) *r, size_t position) {
    if (r->size == 0) return NULL;
    return &r->buffer[FAST_MODULUS(position, r->size)];
}


void PRE(RinBuf_clear) (PRE(RinBuf) *r) {
    r->head_pointer = 0;
    memset(r->buffer, 0, r->size);
}


#ifdef RINGBUFFER_ENABLE_SORT

typedef struct {
    size_t a;
    size_t b;
} PRE(RinBuf_Pair);

static PRE(RinBuf_Pair) PRE(RinBuf_partition) (TYPE *buffer, size_t from, size_t to) {

    size_t left = from;
    size_t eq = from;
    size_t right = to;
    TYPE pivot = buffer[(from + to) / 2];

    while (eq <= right) {

        if (buffer[eq] < pivot) {
            TYPE swap = buffer[eq];
            buffer[eq] = buffer[left];
            buffer[left] = swap;

            ++left;
            ++eq;
        } else if (buffer[eq] > pivot) {
            TYPE swap = buffer[eq];
            buffer[eq] = buffer[right];
            buffer[right] = swap;

            --right;
        } else {
            ++eq;
        }
    }

    return (PRE(RinBuf_Pair)){left, right};
}

void PRE(RinBuf_sort_range) (TYPE *buffer, size_t from, size_t to) {
    if (from >= 0 && from < to) {
        PRE(RinBuf_Pair) partition_index = PRE(RinBuf_partition)(buffer, from, to);
        PRE(RinBuf_sort_range) (buffer, from, partition_index.a -1);
        PRE(RinBuf_sort_range) (buffer, partition_index.b +1, to);
    }
}

// sorts it and repositions head to latest
void PRE(RinBuf_sort) (PRE(RinBuf) *r) {
    r->head_pointer = r->size-1;
    PRE(RinBuf_sort_range) (r->buffer, 0, r->size-1);
}

#undef TYPE
#undef RINGBUFFER_ENABLE_SORT
#endif // !RINGBUFFER_ENABLE_SORT
