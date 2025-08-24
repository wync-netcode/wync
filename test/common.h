#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "../wync.h"

#define BALL_MAX 4

typedef struct {
	int x;
	int y;
} Vector2i;

typedef struct {
	bool enabled;
	int input_move_direction;
	Vector2i position;
} Ball;

typedef struct {
	WyncCtx *wctx;
	int network_peer_id;
	Ball balls[BALL_MAX];
} GameState;

void ball_instance_set_position(WyncWrapper_UserCtx ctx, WyncWrapper_Data data) {
	if (ctx.type_size != sizeof(Ball) || data.data_size != sizeof(Vector2i))
	{ return; }
	Ball *ball_instance = (Ball *)ctx.ctx;
	memcpy(&ball_instance->position, data.data, sizeof(Vector2i));
}

WyncWrapper_Data ball_instance_get_position (WyncWrapper_UserCtx ctx) {
	if (ctx.type_size != sizeof(Ball)) { return (WyncWrapper_Data) { 0 }; }
	Ball *ball_instance = (Ball *)ctx.ctx;
	WyncWrapper_Data data;
	data.data_size = sizeof(Vector2i);
	data.data = calloc(data.data_size, 1); 
	memcpy(data.data, &ball_instance->position, data.data_size);
	return data;
}

void ball_instance_set_input(WyncWrapper_UserCtx ctx, WyncWrapper_Data data) {
	if (ctx.type_size != sizeof(Ball) || data.data_size != sizeof(int))
	{ return; }
	Ball *ball_instance = (Ball *)ctx.ctx;
	memcpy(&ball_instance->input_move_direction, data.data, sizeof(int));

	int debug_input_int = 0;
	memcpy(&debug_input_int, data.data, sizeof(int));
	printf("Hey, input got setted %d\n", debug_input_int);
}

WyncWrapper_Data ball_instance_get_input (WyncWrapper_UserCtx ctx) {
	if (ctx.type_size != sizeof(Ball)) { return (WyncWrapper_Data) { 0 }; }
	Ball *ball_instance = (Ball *)ctx.ctx;
	WyncWrapper_Data data;
	data.data_size = sizeof(int);
	data.data = calloc(data.data_size, 1); 
	memcpy(data.data, &ball_instance->input_move_direction, data.data_size);
	printf("Hey, input got GOTTEN %d\n", ball_instance->input_move_direction);
	return data;
}
