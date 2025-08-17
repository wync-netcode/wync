#ifndef WYNC_LOG_H
#define WYNC_LOG_H

#include "stdbool.h"
#include "stdio.h" // printf
#include "stdlib.h" // abort

#define ANSI_NRM  "\x1B[0m"
#define ANSI_RED  "\x1B[31m"
#define ANSI_GRN  "\x1B[32m"
#define ANSI_GRAY "\x1b[90m"

bool BREAK_enable = true; // For manual disabling in GDB
int LOG_caller_id = 0;

#define LOG_CALLER_SERVER 0
#define LOG_CALLER_CLIENT 1

#define LOG_SET_CALLER_ID(caller_id) \
	do { LOG_caller_id = (caller_id); } while (0)

#define ABORT abort();


#define DEBUG_BREAK do { if (BREAK_enable) { asm("int3"); } } while(0)

#define LOG_OUT(...) \
	do { \
		printf(__VA_ARGS__); \
		printf(" %s%s|%s:%d%s\n", ANSI_GRAY, __func__, __FILE__, __LINE__, ANSI_NRM); \
	} while (0)

#define LOG_ERR(...) \
	do { \
		fprintf(stderr, "%s", ANSI_RED); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, " %s%s|%s:%d%s\n", ANSI_GRAY, __func__, __FILE__, __LINE__, ANSI_NRM); \
	} while (0)

#define LOG_OUT_INTERNAL(is_client, ...) \
	do { \
		printf("%s: ", (is_client) ? "clien" : "serve"); \
		printf(__VA_ARGS__); \
		printf(" %s%s|%s:%d%s\n", ANSI_GRAY, __func__, __FILE__, __LINE__, ANSI_NRM); \
	} while (0)

#define LOG_ERR_INTERNAL(is_client, ...) \
	do { \
		fprintf(stderr, "%s", ANSI_RED); \
		fprintf(stderr, "%s: ", (is_client) ? "clien" : "serve"); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "%s", ANSI_GRAY); \
		fprintf(stderr, " %s|%s:%d%s\n", __func__, __FILE__, __LINE__, ANSI_NRM); \
	} while (0)

#define LOG_WAR_INTERNAL(is_client, ...) \
	do { \
		fprintf(stderr, "%s", ANSI_RED); \
		fprintf(stderr, "%s: ", (is_client) ? "clien" : "serve"); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "%s", ANSI_GRAY); \
		fprintf(stderr, " %s|%s:%d%s\n", __func__, __FILE__, __LINE__, ANSI_NRM); \
	} while (0)

#define LOG_OUT_STATIC(...) LOG_OUT_INTERNAL(LOG_caller_id, __VA_ARGS__)
#define LOG_ERR_STATIC(...) LOG_ERR_INTERNAL(LOG_caller_id, __VA_ARGS__)

#define LOG_OUT_GS(gs, ...) LOG_OUT_INTERNAL((gs->net.is_client), __VA_ARGS__)
#define LOG_ERR_GS(gs, ...) LOG_ERR_INTERNAL((gs->net.is_client), __VA_ARGS__)

#define LOG_OUT_C(ctx, ...) LOG_OUT_INTERNAL(((ctx)->common.is_client), __VA_ARGS__)
#define LOG_ERR_C(ctx, ...) LOG_ERR_INTERNAL(((ctx)->common.is_client), __VA_ARGS__)
#define LOG_WAR_C(ctx, ...) LOG_WAR_INTERNAL(((ctx)->common.is_client), __VA_ARGS__)

#endif // !WYNC_LOG_H
