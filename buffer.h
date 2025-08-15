#ifndef BUFFER_H
#define BUFFER_H

#include "stdbool.h"
#include "string.h"
#include "lib/log.h"
#include <stdint.h>


typedef struct {
	uint32_t cursor_byte;
	uint32_t size_bytes;
	char *data;
} NeteBuffer;


void NeteBuffer_reset_cursor(NeteBuffer *buffer) {
	buffer->cursor_byte = 0;
}
bool NeteBuffer_write_bytes(NeteBuffer *buffer, void *data, uint32_t size) {
	if (data == NULL || size == 0) {
		return false;
	}
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(buffer->data + buffer->cursor_byte, data, size);
	buffer->cursor_byte += size;
	return true;
}
bool NeteBuffer_read_bytes(NeteBuffer *buffer, void *destination, uint32_t size) {
	if (destination == NULL || size == 0) {
		return false;
	}
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(destination, buffer->data + buffer->cursor_byte, size);
	buffer->cursor_byte += size;
	return true;
}
bool NeteBuffer_bytes_serialize (
	bool is_reading, NeteBuffer *buffer, void *other, uint32_t size
) {
	if (other == NULL || size == 0) {
		return false;
	}
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	if (is_reading) memcpy(other, buffer->data + buffer->cursor_byte, size);
	else memcpy(buffer->data + buffer->cursor_byte, other, size);
	buffer->cursor_byte += size;
	return true;
}

#define NETEBUFFER_WRITE_BYTES(buffer, data, size) \
	do { \
		if (!NeteBuffer_write_bytes(buffer, data, size)) { \
			DEBUG_BREAK; \
			return false; \
		} \
	} while (0)

#define NETEBUFFER_READ_BYTES(buffer, dest, size) \
	do { \
		if (!NeteBuffer_read_bytes(buffer, dest, size)) { \
			DEBUG_BREAK; \
			return fals|e; \
		} \
	} while (0)

#define NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, dest, size) \
	do { \
		if (!NeteBuffer_bytes_serialize(is_reading, buffer, dest, size)) { \
			DEBUG_BREAK; \
			return false; \
		} \
	} while (0)


/*
bool NeteBuffer_write_byte8(NeteBuffer *buffer, char value) {
	u16 size = sizeof(char);
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(buffer->data + buffer->cursor_byte, &value, size);
	buffer->cursor_byte += size;
	return true;
}
bool NeteBuffer_write_int16(NeteBuffer *buffer, u16 value) {
	u16 size = sizeof(u16);
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(buffer->data + buffer->cursor_byte, &value, size);
	buffer->cursor_byte += size;
	return true;
}
bool NeteBuffer_write_int32(NeteBuffer *buffer, uint32_t value) {
	u16 size = sizeof(uint32_t);
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(buffer->data + buffer->cursor_byte, &value, size);
	buffer->cursor_byte += size;
	return true;
}
bool NeteBuffer_write_int64(NeteBuffer *buffer, u64 value) {
	u16 size = sizeof(u64);
	if (buffer->cursor_byte + size > buffer->size_bytes) {
		return false;
	}
	memcpy(buffer->data + buffer->cursor_byte, &value, size);
	buffer->cursor_byte += size;
	return true;
}
*/


#endif // !BUFFER_H
