#include <stdint.h>
#ifndef i16
#define i16 int16_t
#endif
#ifndef u16
#define u16 uint16_t
#endif
#ifndef i32
#define i32 int32_t
#endif
#ifndef u32
#define u32 uint32_t
#endif
#ifndef i64
#define i64 int64_t
#endif
#ifndef u64
#define u64 uint64_t
#endif

#ifndef MACRO_TYPES_H
#define MACRO_TYPES_H

#define SIGN(x) ((x) < 0 ? -1 : ((x) > 0 ? +1 : 0))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#endif
