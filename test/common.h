#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "../wync.h"

#define GAME_TPS 30
#define BALL_MAX 4
#define CHUNK_MAX 4
#define OK 0

enum {
	LERP_DATA_TYPE_NONE,
	LERP_DATA_TYPE_VECTOR2I
};

typedef struct {
	int x;
	int y;
} Vector2i;

typedef struct {
	bool enabled;
	int input_move_direction;
	Vector2i position;

	uint wync_prop_id_pos;
	uint wync_prop_id_input;
} Ball;


#define CHUNK_WIDTH_BLOCKS  12
#define CHUNK_HEIGHT_BLOCKS 9

typedef struct {
	int type; 
} Block;

typedef struct {
	int actor_id; 
	int position;  // where in the world is located
	Block blocks[CHUNK_WIDTH_BLOCKS][CHUNK_HEIGHT_BLOCKS];  //: Array[Array[Block]]
} Chunk;

typedef struct {
	WyncCtx *wctx;
	int network_peer_id;
	Ball balls[BALL_MAX];
	Chunk chunks[BALL_MAX];
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

WyncWrapper_Data lerp_vector2i (
	WyncWrapper_Data from, WyncWrapper_Data to, float delta
) {
	Vector2i from_value, to_value;
	memcpy(&from_value, from.data, sizeof(Vector2i));
	memcpy(&to_value, to.data, sizeof(Vector2i));
	Vector2i result_value = (Vector2i) {
		(int)((float)from_value.x + (float)(to_value.x - from_value.x) * delta),
		(int)((float)from_value.y + (float)(to_value.y - from_value.y) * delta)
	};
	printf("lerp_vector2i a %d b %d delta is %f\n",
			from_value.x,
			to_value.y,
			delta);
	WyncWrapper_Data result;
	result.data_size = sizeof(Vector2i);
	result.data = malloc(sizeof(Vector2i));
	memcpy(result.data, &result_value, sizeof(Vector2i));
	return result;
}

// for testing delta props

enum {
	BLOCK_TYPE_AIR,
	BLOCK_TYPE_DIRT,
	BLOCK_TYPE_IRON,
	BLOCK_TYPE_GOLD,
	BLOCK_TYPE_AMOUNT,
};

enum {
	EVENT_NONE,
	// delta
	EVENT_DELTA_BLOCK_REPLACE,
};

// later filled on blueprint setup
static int BLUEPRINT_ID_BLOCK_GRID_DELTA = -1;

typedef struct {
	Vector2i pos; 
	uint block_type; 
} EventDeltaBlockReplace;

void chunk_instance_set_blocks(WyncWrapper_UserCtx ctx, WyncWrapper_Data data) {
	if (ctx.type_size != sizeof(Chunk)
	|| data.data_size != sizeof(Block[CHUNK_WIDTH_BLOCKS][CHUNK_HEIGHT_BLOCKS]))
	{ return; }
	Chunk *instance = (Chunk *)ctx.ctx;
	memcpy(&instance->blocks, data.data,
			sizeof(Block[CHUNK_WIDTH_BLOCKS][CHUNK_HEIGHT_BLOCKS]));
}
WyncWrapper_Data chunk_instance_get_blocks (WyncWrapper_UserCtx ctx) {
	if (ctx.type_size != sizeof(Chunk)) { return (WyncWrapper_Data) { 0 }; }
	Chunk *instance = (Chunk *)ctx.ctx;
	WyncWrapper_Data data;
	data.data_size = sizeof(Block[CHUNK_WIDTH_BLOCKS][CHUNK_HEIGHT_BLOCKS]);
	data.data = calloc(1, data.data_size);
	memcpy(data.data, &instance->blocks, data.data_size);
	return data;
}
void initialize_chunks(GameState *gs) {
	// chunks

	for (int32_t k = 0; k < CHUNK_MAX; ++k) {
		Chunk *chunk = &gs->chunks[k];

		chunk->actor_id = k;
		chunk->position = k;
		chunk->blocks[0][0].type = BLOCK_TYPE_DIRT;
	}
}

int blueprint_handle_event_delta_block_replace (
	WyncCtx *ctx,
	WyncWrapper_UserCtx user_ctx,
	WyncEvent_EventData event,
	bool requires_undo
) {
	// TODO: Maybe check event integrity before casting

	if (user_ctx.type_size != sizeof(Chunk)
		|| event.data.data_size != sizeof(EventDeltaBlockReplace))
		{ return -1; }

	Chunk *chunk = (Chunk *)user_ctx.ctx;
	EventDeltaBlockReplace *event_data = (EventDeltaBlockReplace*) event.data.data;
	Vector2i block_pos = event_data->pos;

	Block *block = &chunk->blocks[block_pos.x][block_pos.y];
	int event_id = 0;

	// create undo event

	if (requires_undo) {
		int prev_block_type = block->type;
		EventDeltaBlockReplace undo_event = { 0 };
		undo_event.pos = block_pos;
		undo_event.block_type = prev_block_type;

		uint wrapped_event_id;
		int error = WyncEventUtils_new_event_wrap_up(
			ctx,
			EVENT_DELTA_BLOCK_REPLACE,
			sizeof(undo_event),
			&undo_event,
			&wrapped_event_id
		);
		if (error != OK) {
			fprintf(stderr, "block replace, couldn't create event\n");
		}
		else {
			event_id = (int)wrapped_event_id;
		}
	}

	block->type = event_data->block_type;

	return event_id;
}

void setup_blueprints(WyncCtx *ctx) {
	uint blueprint_id = WyncDelta_create_blueprint (ctx);
	WyncDelta_blueprint_register_event(
		ctx,
		blueprint_id,
		EVENT_DELTA_BLOCK_REPLACE,
		blueprint_handle_event_delta_block_replace
	);
	BLUEPRINT_ID_BLOCK_GRID_DELTA = blueprint_id;
}
