#include "assert.h"
#include "common.h"
#include "simpletest.h"
#include "../wync.h"
#include "../src/wync_private.h"
#include <stdio.h>
#define WYNC_TESTING

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // nanosleep
#else
#include <unistd.h> // usleep
#endif

void sleep_ms(int milliseconds){ // cross-platform sleep function
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    if (milliseconds >= 1000)
      sleep(milliseconds / 1000);
    usleep((milliseconds % 1000) * 1000);
#endif
}


#define FAKENET_LATENCY_MS 33
GameState server_gs;
GameState client_gs;


void util_reset_state(void) {
	memset(&server_gs, 0, sizeof(GameState));
	memset(&client_gs, 0, sizeof(GameState));
}


void util_send_packets_to (int from_net_peer_id, WyncCtx *from, WyncCtx *to)
{
	sleep_ms(FAKENET_LATENCY_MS);
	WyncFlow_prepare_packet_iterator(from);

	WyncPacketOut packet_wync = { 0 };
	
	while (WyncFlow_get_next_reliable_packet(from, &packet_wync) == OK) {
		WyncFlow_feed_packet(
			to,
			from_net_peer_id,
			packet_wync.data_size,
			packet_wync.data
		);
	}
	while (WyncFlow_get_next_unreliable_packet(from, &packet_wync) == OK) {
		WyncFlow_feed_packet(
			to,
			from_net_peer_id,
			packet_wync.data_size,
			packet_wync.data
		);
	}

	WyncFlow_packet_cleanup(from);
}

void util_setup_server_and_client (void) {
	server_gs.network_peer_id = 0;
	server_gs.wctx = WyncInit_create_context();
	WyncFlow_server_setup(server_gs.wctx);
	WyncClock_set_debug_time_offset(server_gs.wctx, 1000);
	WyncClock_set_ticks(server_gs.wctx, 320);
	WyncClock_client_set_physics_ticks_per_second(server_gs.wctx, GAME_TPS);

	client_gs.network_peer_id = 1;
	client_gs.wctx = WyncInit_create_context();
	WyncFlow_client_setup(client_gs.wctx);
	WyncClock_set_debug_time_offset(client_gs.wctx, 2000);
	WyncClock_set_ticks(client_gs.wctx, 640);
	WyncClock_client_set_physics_ticks_per_second(client_gs.wctx, GAME_TPS);
}

void util_client_joins_server (void) {
	WyncJoin_set_my_nete_peer_id(client_gs.wctx, client_gs.network_peer_id);
	WyncJoin_set_server_nete_peer_id(client_gs.wctx, server_gs.network_peer_id);

	WyncJoin_service_wync_try_to_connect(client_gs.wctx);
	util_send_packets_to(client_gs.network_peer_id, client_gs.wctx, server_gs.wctx);
	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);
}


void util_force_WyncWrapper_client_filter_prop_ids (WyncCtx *ctx) {
	ctx->common.was_any_prop_added_deleted = true;
	WyncWrapper_client_filter_prop_ids (ctx);
}


void util_force_WyncWrapper_server_filter_prop_ids (WyncCtx *ctx) {
	ctx->common.was_any_prop_added_deleted = true;
	WyncWrapper_server_filter_prop_ids(ctx);
}


void util_setup_lerp_types(WyncCtx *ctx) {
	WyncLerp_register_lerp_type(
		ctx,
		LERP_DATA_TYPE_VECTOR2I,
		lerp_vector2i
	);
}


void util_simulate_game_engine_logic_cycle (void) {
	for (int i = 0; i < 33; ++i) {
		// server
		// ----------------------------------------------------------

		WyncFlow_server_tick_start(server_gs.wctx);
		// Let Wync know the latency of each peer each frame
		WyncPeer_ids peer_ids;
		WyncJoin_active_peers_setup_iteration(server_gs.wctx);
		while (WyncJoin_active_peers_get_next(server_gs.wctx, &peer_ids) == OK)
		{
			if (peer_ids.network_peer_id < 0) continue; 
			WyncClock_peer_set_current_latency(
				server_gs.wctx, peer_ids.wync_peer_id, 200);
		}
		WyncFlow_server_tick_end(server_gs.wctx);
		WyncPacket_set_data_limit_chars_for_out_packets(server_gs.wctx, 100000);
		WyncFlow_gather_packets(server_gs.wctx);

		// client
		// ----------------------------------------------------------

		WyncFlow_client_tick_end(client_gs.wctx);
		WyncPacket_set_data_limit_chars_for_out_packets(client_gs.wctx, 100000);
		WyncFlow_gather_packets(client_gs.wctx);
	}
}


void util_wync_actions_and_events_consumed(void) {
	WyncConsumed_global_event_consume_tick(server_gs.wctx, 1, 0, 100, 100);
	WyncName action = { .name = "test_action_A" };
	WyncAction_mark_as_ran_on_tick(client_gs.wctx, 100, action);
	WyncAction_already_ran_on_tick(client_gs.wctx, 100, action);
	WyncAction_tick_history_reset(client_gs.wctx, 100);
}


void util_wync_debug(void) {
	static char lines[4096] = "";
	lines[0] = 0;
	WyncDebug_get_info_general_text(server_gs.wctx, client_gs.wctx, lines);
	WyncDebug_get_packets_received_info_text(client_gs.wctx, lines, 2);
	WyncDebug_get_prop_info_text(client_gs.wctx, lines);
}


void util_setup_sync_for_chunk_actor(GameState *gs, u32 actor_id) {
	WyncCtx *wctx = gs->wctx;
	Chunk *chunk_instance = &gs->chunks[actor_id];

	assert(WyncTrack_track_entity(wctx, actor_id, 0xDEAD) == OK);

	u32 block_prop_id;

	assert(
	WyncTrack_prop_register_minimal(
		wctx,
		actor_id,
		"blocks",
		WYNC_PROP_TYPE_STATE,
		&block_prop_id
	) == OK);
	WyncWrapper_set_prop_callbacks(
		wctx,
		block_prop_id,
		(WyncWrapper_UserCtx) {
			.ctx = chunk_instance,
			.type_size = sizeof(Chunk)
		},
		chunk_instance_get_blocks,
		chunk_instance_set_blocks
	);

	// hook blueprint to prop

	bool predict = actor_id % 2 == 0;
	int error = WyncProp_enable_relative_sync(
		wctx,
		actor_id,
		block_prop_id,
		BLUEPRINT_ID_BLOCK_GRID_DELTA,
		predict
	);
	assert(error == OK);

}

void util_chunk_block_replace(
	GameState *gs, Vector2i block_pos, uint block_type
) {
	EventDeltaBlockReplace event = { 0 };
	event.pos = block_pos;
	event.pos.x = block_pos.x % CHUNK_WIDTH_BLOCKS;
	event.block_type = block_type;

	// get block prop id

	uint chunk_id = (uint)block_pos.x / CHUNK_WIDTH_BLOCKS;
	Chunk *chunk = &gs->chunks[chunk_id];

	uint prop_id;
	int error = WyncTrack_entity_get_prop_id(
								gs->wctx, chunk->actor_id, "blocks", &prop_id);
	assert(error == OK);

	// client prediction: Allowed to predic this entity?
	if (!WyncXtrap_allowed_to_predict_entity(gs->wctx, chunk->actor_id)) {
		return;
	}

	// create wync event

	uint event_id;
	error = WyncEventUtils_new_event_wrap_up(
		gs->wctx,
		EVENT_DELTA_BLOCK_REPLACE,
		sizeof(EventDeltaBlockReplace),
		&event,
		&event_id
	);
	assert(error == OK);

	// push to prop's current event buffer

	error = WyncDelta_prop_push_event_to_current(
		gs->wctx, prop_id, EVENT_DELTA_BLOCK_REPLACE, event_id
	);
	assert(error == OK);

	// apply it for instant results

	error = WyncDelta_merge_event_to_state_real_state(
		gs->wctx, prop_id, event_id
	);
	assert(error == OK);
}


void util_random_blocks_events(GameState* gs) {
	/*if (PlatGlobal_ticks % 16 != 0) { return; }*/
	//Chunk *chunk = &gs->chunks[CHUNK_AMOUNT-1];
	int block_id = rand() % CHUNK_HEIGHT_BLOCKS;
	uint block_type = (uint)rand() % BLOCK_TYPE_AMOUNT;
	//chunk->blocks[CHUNK_WIDTH_BLOCKS-1][block_id].type = block_type;

	util_chunk_block_replace(
		gs,
		(Vector2i) { (CHUNK_MAX * CHUNK_WIDTH_BLOCKS)-1, block_id },
		block_type
	);
}


void util_delta_sync(void) {
	initialize_chunks(&server_gs);
	initialize_chunks(&client_gs);
	setup_blueprints(server_gs.wctx);
	setup_blueprints(client_gs.wctx);
	util_setup_sync_for_chunk_actor(&server_gs, 3);
	util_setup_sync_for_chunk_actor(&client_gs, 3);
	for (int i = 0; i < 5; ++i) {
		util_random_blocks_events(&server_gs);
	}
}


void util_timewarp(void) {
	uint actor_id = 2;
	WyncProp_enable_timewarp(server_gs.wctx, actor_id);

	WyncTimewarp_get_peer_latency_stable(server_gs.wctx, 1);
	WyncTimewarp_get_peer_lerp_ms(server_gs.wctx, 1);
	uint server_tick = WyncClock_get_ticks(server_gs.wctx);
	WyncTimewarp_can_we_timerwarp_to_this_tick(server_gs.wctx, server_tick);
	WyncTimewarp_cache_current_state_timewarpable_props(server_gs.wctx);
	WyncTimewarp_warp_to_tick(server_gs.wctx, server_tick, 0);
	WyncTimewarp_warp_entity_to_tick(server_gs.wctx, actor_id, server_tick, 0);
}


/// A Client connects to a Server
void test_join (void) {
	TESTS_INIT();
	util_reset_state();

	// setup
	util_setup_server_and_client();

	TEST_FALSE(client_gs.wctx->common.connected);

	// join each other
	util_client_joins_server();

	TEST_TRUE(client_gs.wctx->common.connected);
	TESTS_SHOW_RESULTS();
}

/// Server tracks a game entity
void test_tracking (void) {
	TESTS_INIT();

	util_reset_state();
	
	// setup game
	int actor_id = 0;
	server_gs.balls[actor_id].enabled = true;
	server_gs.balls[actor_id].position = (Vector2i) { 10, 20 };

	// setup wync server
	server_gs.network_peer_id = 0;
	server_gs.wctx = WyncInit_create_context();
	WyncFlow_server_setup(server_gs.wctx);

	// track entity
	int error = WyncTrack_track_entity(server_gs.wctx, actor_id, 0xDEADBEEF);
	TEST_INT(error, OK);

	// double check
	bool succ = WyncTrack_is_entity_tracked(server_gs.wctx, actor_id);
	TEST_TRUE(succ);

	TESTS_SHOW_RESULTS();
}

/// Server sends snapshot to Client
void test_snapshot (void) {
	TESTS_INIT();
	util_reset_state();

	// server, setup state

	uint actor_id = 3;
	uint actor_type = 0xBEEF; // TODO: Test data limits
	server_gs.balls[actor_id].enabled = true;
	server_gs.balls[actor_id].position = (Vector2i) { 10, 20 };

	// client, join server
	// ----------------------------------------------------------

	util_setup_server_and_client();
	util_client_joins_server();
	TEST_TRUE(client_gs.wctx->common.connected);
	TEST_INT(client_gs.wctx->common.my_peer_id, 1); // 0: server 1: client



	// server, track entity
	// ----------------------------------------------------------

	TEST_INT(WyncTrack_track_entity(server_gs.wctx, actor_id, actor_type), OK);

	// server, register position prop

	uint pos_prop_id = 999;
	int error = WyncTrack_prop_register_minimal(
		server_gs.wctx,
		actor_id,
		"position",
		WYNC_PROP_TYPE_STATE,
		&pos_prop_id
	);
	TEST_INT(error, OK);
	TEST_FALSE(pos_prop_id == 999);

	// server, register prop callbacks
	// ----------------------------------------------------------

	Ball *ball_instance = &server_gs.balls[actor_id];
	WyncWrapper_set_prop_callbacks(
		server_gs.wctx,
		pos_prop_id,
		(WyncWrapper_UserCtx) {
			.ctx = ball_instance,
			.type_size = sizeof(Ball)
		},
		ball_instance_get_position,
		ball_instance_set_position
	);



	// server, let client know about ball actor
	// ----------------------------------------------------------

	WyncThrottle_client_now_can_see_entity(
		server_gs.wctx, client_gs.wctx->common.my_peer_id, actor_id);

	// server, send spawn packet to client
	// ----------------------------------------------------------

	WyncSpawn_system_send_entities_to_spawn(server_gs.wctx);

	// server, send packets to client

	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);



	// client, handle spawning
	// ----------------------------------------------------------

	Wync_EntitySpawnEvent event = { 0 };
	bool received_spawn_events = false;

	while (WyncSpawn_get_next_entity_event_spawn(client_gs.wctx, &event) == OK)
	{
		received_spawn_events = true;
		TEST_TRUE(event.spawn);

		uint spawn_actor_id = event.entity_id;
		uint spawn_actor_type = event.entity_type_id;

		TEST_INT(spawn_actor_id, actor_id);
		printf("actor_type diff %u %u\n", spawn_actor_type, actor_type);
		TEST_INT((int)spawn_actor_type, (int)actor_type);

		// client, spawn ball actor
		// ----------------------------------------------------------

		client_gs.balls[spawn_actor_id].enabled = true;

		// client, track and setup synchronization for ball actor

		TEST_INT(WyncTrack_track_entity(client_gs.wctx, actor_id, actor_type), OK);

		uint pos_prop_id = 999;
		int error = WyncTrack_prop_register_minimal(
			client_gs.wctx,
			actor_id,
			"position",
			WYNC_PROP_TYPE_STATE,
			&pos_prop_id
		);
		TEST_INT(error, OK);
		TEST_FALSE(pos_prop_id == 999);

		Ball *ball_instance = &client_gs.balls[actor_id];
		WyncWrapper_set_prop_callbacks(
			client_gs.wctx,
			pos_prop_id,
			(WyncWrapper_UserCtx) {
				.ctx = ball_instance,
				.type_size = sizeof(Ball)
			},
			ball_instance_get_position,
			ball_instance_set_position
		);

		WyncSpawn_finish_spawning_entity(client_gs.wctx, event.entity_id);
	}

	WyncSpawn_system_spawned_props_cleanup(client_gs.wctx);
	TEST_TRUE(received_spawn_events);




	// server, extract data from prop
	// ----------------------------------------------------------

	WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	WyncWrapper_extract_data_to_tick(server_gs.wctx, server_gs.wctx->common.ticks);

	// server, populate entity update queue
	// ----------------------------------------------------------

	WyncPacket_set_data_limit_chars_for_out_packets(server_gs.wctx, 100000);
	WyncThrottle_system_fill_entity_sync_queue(server_gs.wctx);
	WyncThrottle_compute_entity_sync_order(server_gs.wctx);
	WyncSend_extracted_data(server_gs.wctx);
	WyncSend_queue_out_snapshots_for_delivery(server_gs.wctx);

	// server, send packets to client

	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);





	// client, confirm snapshot arrival
	// ----------------------------------------------------------

	uint received_snaps_pkts = client_gs.wctx->co_metrics.debug_packets_received
		[WYNC_PKT_PROP_SNAP][0];
	TEST_INT(received_snaps_pkts, 1);

	// client, test ball updated state

	Ball *client_ball_instance = &client_gs.balls[actor_id];

	TEST_INT(client_ball_instance->position.x, 0);
	TEST_INT(client_ball_instance->position.y, 0);

	WyncState_reset_props_to_latest_value(client_gs.wctx);

	TEST_FALSE(client_ball_instance->position.x == 0);
	TEST_FALSE(client_ball_instance->position.y == 0);
	TEST_INT(client_ball_instance->position.x, ball_instance->position.x);
	TEST_INT(client_ball_instance->position.y, ball_instance->position.y);

	TESTS_SHOW_RESULTS();
}


void util_setup_tracking_ball (
	GameState* gs, uint entity_id, uint *pos_prop_id, uint *input_prop_id,
	bool enable_lerping
) {
	int error = WyncTrack_prop_register_minimal(
		gs->wctx, entity_id, "position", WYNC_PROP_TYPE_STATE, pos_prop_id
	);
	assert(error == OK);

	error = WyncTrack_prop_register_minimal(
		gs->wctx, entity_id, "input", WYNC_PROP_TYPE_INPUT, input_prop_id
	);
	assert(error == OK);

	Ball *ball_instance = &gs->balls[entity_id];

	WyncWrapper_set_prop_callbacks(
		gs->wctx,
		*pos_prop_id,
		(WyncWrapper_UserCtx) { .ctx = ball_instance, .type_size = sizeof(Ball) },
		ball_instance_get_position,
		ball_instance_set_position
	);
	WyncWrapper_set_prop_callbacks(
		gs->wctx,
		*input_prop_id,
		(WyncWrapper_UserCtx) { .ctx = ball_instance, .type_size = sizeof(Ball) },
		ball_instance_get_input,
		ball_instance_set_input
	);

	if (enable_lerping) {
		error = WyncProp_enable_interpolation(
			gs->wctx,
			*pos_prop_id,
			LERP_DATA_TYPE_VECTOR2I,
			ball_instance_set_position
		);
		assert(error == OK);
	}

	ball_instance->wync_prop_id_pos = *pos_prop_id;
	ball_instance->wync_prop_id_input = *input_prop_id;
}


void test_client_authority_inputs (void) {
	TESTS_INIT();

	util_reset_state();

	// server, setup state

	uint actor_id = 2;
	uint actor_type = 0xBEEF;
	server_gs.balls[actor_id].enabled = true;
	server_gs.balls[actor_id].position = (Vector2i) { 10, 20 };

	// client, join server

	util_setup_server_and_client();
	util_client_joins_server();

	// server, track entity with input prop

	uint pos_prop_id = 999;
	uint input_prop_id = 999;

	TEST_INT(WyncTrack_track_entity(server_gs.wctx, actor_id, actor_type), OK);
	util_setup_tracking_ball(
			&server_gs, actor_id, &pos_prop_id, &input_prop_id, false);
	TEST_FALSE(pos_prop_id == 999);
	TEST_FALSE(input_prop_id == 999);

	// server, give client authority over it's input

	util_force_WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	size_t server_input_props = u32_DynArr_get_size(
		&server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids);
	TEST_UINT(server_input_props, 1); 

	WyncInput_prop_set_client_owner(
		server_gs.wctx, input_prop_id, client_gs.wctx->common.my_peer_id);

	util_force_WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	server_input_props = u32_DynArr_get_size(
		&server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids);
	TEST_UINT(server_input_props, 2); 

	// server, let client know about it

	int error = WyncThrottle_client_now_can_see_entity(
		server_gs.wctx, client_gs.wctx->common.my_peer_id, actor_id);
	TEST_INT(error, OK);

	WyncInput_system_sync_client_ownership(server_gs.wctx);

	// server, let client spawn it

	WyncSpawn_system_send_entities_to_spawn(server_gs.wctx);

	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);

	// client, spawn entity

	Wync_EntitySpawnEvent event = { 0 };
	bool received_spawn_events = false;

	while (WyncSpawn_get_next_entity_event_spawn(client_gs.wctx, &event) == OK)
	{
		received_spawn_events = true;
		TEST_TRUE(event.spawn);

		uint spawn_actor_id = event.entity_id;
		uint spawn_actor_type = event.entity_type_id;

		TEST_INT(spawn_actor_id, actor_id);
		TEST_INT((int)spawn_actor_type, (int)actor_type);

		uint client_pos_prop_id = 999;
		uint client_input_prop_id = 999;

		TEST_INT(WyncTrack_track_entity(client_gs.wctx, actor_id, actor_type), OK);
		util_setup_tracking_ball(
			&client_gs, actor_id,
			&client_pos_prop_id, &client_input_prop_id, false);
		TEST_FALSE(client_pos_prop_id == 999);
		TEST_FALSE(client_input_prop_id == 999);

		// client, actor setup game state

		client_gs.balls[spawn_actor_id].enabled = true;

		WyncSpawn_finish_spawning_entity(client_gs.wctx, event.entity_id);
	}

	WyncSpawn_system_spawned_props_cleanup(client_gs.wctx);
	TEST_TRUE(received_spawn_events);

	// client, dictate input prop state

	Ball *client_ball_instance = &client_gs.balls[actor_id];
	client_ball_instance->input_move_direction = -100;

	// client, force input extraction
	// (from WyncFlow_client_tick_end)

	util_force_WyncWrapper_client_filter_prop_ids(client_gs.wctx);
	int owned_props_amount = u32_DynArr_get_size(
			&client_gs.wctx->co_filter_c.type_input_event__owned_prop_ids);
	TEST_INT(owned_props_amount, 2);

	client_gs.wctx->co_pred.target_tick = 200;
	WyncWrapper_buffer_inputs(client_gs.wctx);
	client_gs.wctx->co_pred.target_tick = 201;

	// client, send inputs to server

	WyncPacket_set_data_limit_chars_for_out_packets(client_gs.wctx, 50000);
	WyncSend_client_send_inputs(client_gs.wctx);
	util_send_packets_to(client_gs.network_peer_id, client_gs.wctx, server_gs.wctx);

	TEST_INT(server_gs.wctx->co_metrics.debug_packets_received[
		WYNC_PKT_INPUTS][0], 2); // JOIN_REQ + SET_LERP_MS

	// server, set inputs
	WyncState_reset_all_state_to_confirmed_tick_absolute
	(
	server_gs.wctx,
	server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids.items,
	(u32)server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids.size,
	200
	);

	// server, confirm client's dictated state was applied

	TEST_INT(
		server_gs.balls[actor_id].input_move_direction, 
		client_gs.balls[actor_id].input_move_direction
	);

	util_simulate_game_engine_logic_cycle ();

	TESTS_SHOW_RESULTS();
}

void test_extrapolation (void) {
	TESTS_INIT();

	util_reset_state();

	// server, setup state

	uint actor_id = 2;
	uint actor_type = 0xBEEF;
	server_gs.balls[actor_id].enabled = true;
	server_gs.balls[actor_id].position = (Vector2i) { 10, 20 };

	// client, join server

	util_setup_server_and_client();
	util_client_joins_server();

	// server, track entity with input prop

	uint pos_prop_id = 999;
	uint input_prop_id = 999;

	TEST_INT(WyncTrack_track_entity(server_gs.wctx, actor_id, actor_type), OK);
	util_setup_tracking_ball(
			&server_gs, actor_id, &pos_prop_id, &input_prop_id, false);
	TEST_FALSE(pos_prop_id == 999);
	TEST_FALSE(input_prop_id == 999);

	// server, give client authority over it's input

	WyncInput_prop_set_client_owner(
		server_gs.wctx, input_prop_id, client_gs.wctx->common.my_peer_id);

	WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	size_t server_input_props = u32_DynArr_get_size(
		&server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids);
	TEST_UINT(server_input_props, 2);

	// server, let client know about it

	int error = WyncThrottle_client_now_can_see_entity(
		server_gs.wctx, client_gs.wctx->common.my_peer_id, actor_id);
	TEST_INT(error, OK);

	WyncInput_system_sync_client_ownership(server_gs.wctx);

	// server, let client spawn it

	WyncSpawn_system_send_entities_to_spawn(server_gs.wctx);

	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);

	// client, spawn entity

	util_setup_lerp_types(client_gs.wctx);

	Wync_EntitySpawnEvent event = { 0 };
	bool received_spawn_events = false;

	while (WyncSpawn_get_next_entity_event_spawn(client_gs.wctx, &event) == OK)
	{
		TEST_FALSE(received_spawn_events);
		received_spawn_events = true;
		TEST_TRUE(event.spawn);

		uint spawn_actor_id = event.entity_id;
		uint spawn_actor_type = event.entity_type_id;

		TEST_INT(spawn_actor_id, actor_id);
		TEST_INT((int)spawn_actor_type, (int)actor_type);

		uint client_pos_prop_id = 999;
		uint client_input_prop_id = 999;

		// client, spawn entity & state tracking

		TEST_INT(WyncTrack_track_entity(client_gs.wctx, actor_id, actor_type), OK);
		util_setup_tracking_ball(
			&client_gs, actor_id,
			&client_pos_prop_id, &client_input_prop_id, true);
		TEST_FALSE(client_pos_prop_id == 999);
		TEST_FALSE(client_input_prop_id == 999);

		WyncSpawn_finish_spawning_entity(client_gs.wctx, event.entity_id);

		// client, actor setup game state

		client_gs.balls[spawn_actor_id].enabled = true;

		// client, enable prediction for this Ball entity

		TEST_INT( WyncProp_enable_prediction (
				client_gs.wctx, client_pos_prop_id), OK);
		TEST_INT( WyncProp_enable_prediction (
				client_gs.wctx, client_input_prop_id), OK);
	}

	WyncSpawn_system_spawned_props_cleanup(client_gs.wctx);
	TEST_TRUE(received_spawn_events);

	// client, refilter props to recognize props to predict

	util_force_WyncWrapper_client_filter_prop_ids(client_gs.wctx);

	TEST_UINT( u32_DynArr_get_size(
		&client_gs.wctx->co_filter_c.type_state__predicted_regular_prop_ids),
	1);
	TEST_UINT( u32_DynArr_get_size(
		&client_gs.wctx->co_filter_c.type_input_event__predicted_owned_prop_ids),
	1);

	// server, send snapshot to client
	// ----------------------------------------------------------

	// server, extract data from prop

	WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	WyncWrapper_extract_data_to_tick(
			server_gs.wctx, server_gs.wctx->common.ticks);

	// server, populate entity update queue

	WyncPacket_set_data_limit_chars_for_out_packets(server_gs.wctx, 100000);
	WyncThrottle_system_fill_entity_sync_queue(server_gs.wctx);
	WyncThrottle_compute_entity_sync_order(server_gs.wctx);
	WyncSend_extracted_data(server_gs.wctx);
	WyncSend_queue_out_snapshots_for_delivery(server_gs.wctx);

	// server, send packets to client

	util_send_packets_to(
			server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);

	// client, confirm snapshot arrival

	uint received_snaps_pkts = client_gs.wctx->co_metrics.debug_packets_received
		[WYNC_PKT_PROP_SNAP][0];
	TEST_INT(received_snaps_pkts, 1);
	printf("received_snaps_pkts %u\n", received_snaps_pkts);

	// client and server, exchange clock packets
	// ----------------------------------------------------------

	for (uint i = 0; i < 3; ++i) {
		WyncClock_client_ask_for_clock(client_gs.wctx, true);
		util_send_packets_to(
				client_gs.network_peer_id, client_gs.wctx, server_gs.wctx);
		util_send_packets_to(
				server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);

		WyncClock_peer_set_current_latency(
				client_gs.wctx, SERVER_PEER_ID, FAKENET_LATENCY_MS);
	}
	WyncClock_system_stabilize_latency(
			client_gs.wctx,
			&client_gs.wctx->common.peer_latency_info[SERVER_PEER_ID]
	);

	// client, setup prediction loop (and succeed to predict)
	// ----------------------------------------------------------

	printf("last_tick_received %u\n", 
	client_gs.wctx->co_pred.last_tick_received);

	WyncClock_update_prediction_ticks(client_gs.wctx, true);

	WyncXtrap_ticks xtrap_ticks = WyncXtrap_preparation(client_gs.wctx);
	printf("PRED TICK: %d %d %d\n",
		xtrap_ticks.should_predict, xtrap_ticks.tick_start, xtrap_ticks.tick_end);

	TEST_TRUE(xtrap_ticks.should_predict);
	TEST_UINT(xtrap_ticks.tick_start, 321);
	TEST_TRUE(xtrap_ticks.tick_start < xtrap_ticks.tick_end);

	for (uint tick = xtrap_ticks.tick_start;
		tick < xtrap_ticks.tick_end; ++tick)
	{
		printf("Prediction tick %u\n", tick);
		WyncXtrap_entities xtrap_entities =
			WyncXtrap_tick_init(client_gs.wctx, tick);
		TEST_UINT(xtrap_entities.entity_ids_amount, 1);
		TEST_TRUE(client_gs.wctx->co_pred.currently_on_predicted_tick);

		uint ball_id = xtrap_entities.entity_ids_to_predict[0];
		bool allowed_to_predict = WyncXtrap_allowed_to_predict_entity(
				client_gs.wctx, ball_id);
		TEST_TRUE(allowed_to_predict);

		client_gs.balls[ball_id].position = (Vector2i){ 1010, 1010 };

		WyncXtrap_tick_end(client_gs.wctx, tick);
		WyncXtrap_tick_end(client_gs.wctx, tick); // simulate two cycles
	}
	WyncXtrap_termination(client_gs.wctx);

	TEST_FALSE(client_gs.wctx->co_pred.currently_on_predicted_tick);

	// ==========================================================
	//
	// LERPING TEST (PREDICTED STATE)
	//
	// ==========================================================

	// precompute lerping data

	WyncState_reset_props_to_latest_value(client_gs.wctx);
	WyncLerp_precompute(client_gs.wctx);

	// simulate drawing loop

	for (int i = -10, k = 0; i < 66; i += 10, ++k) {
		float physics_fraction = (float)i / GAME_TPS;
		printf("Lerp tick %u physics_fraction %f\n", k, physics_fraction);
		WyncLerp_interpolate_all(client_gs.wctx, physics_fraction);
	}

	util_wync_actions_and_events_consumed();
	util_wync_debug();
	util_delta_sync();
	util_timewarp();
	util_simulate_game_engine_logic_cycle ();

	TESTS_SHOW_RESULTS();
}


/// Server sends snapshot to Client
void test_lerp_canonic_state (void) {
	TESTS_INIT();
	util_reset_state();


	// server & client, setup & join server
	// ----------------------------------------------------------

	uint actor_id = 3;
	uint actor_type = 0xBEEF; // TODO: Test data limits
	server_gs.balls[actor_id].enabled = true;
	server_gs.balls[actor_id].position = (Vector2i) { 10, 20 };

	util_setup_server_and_client();
	util_client_joins_server();
	TEST_TRUE(client_gs.wctx->common.connected);
	TEST_INT(client_gs.wctx->common.my_peer_id, 1); // 0: server 1: client


	// server, track entity + setup sync
	// ----------------------------------------------------------

	uint pos_prop_id, input_prop_id;
	TEST_INT(WyncTrack_track_entity(server_gs.wctx, actor_id, actor_type), OK);
	util_setup_tracking_ball(
			&server_gs, actor_id, &pos_prop_id, &input_prop_id, false);

	// server, let client know about ball actor

	WyncTrack_wync_add_local_existing_entity(
			server_gs.wctx,
			WyncJoin_get_my_wync_peer_id(client_gs.wctx),
			actor_id);
	WyncThrottle_client_now_can_see_entity(
		server_gs.wctx,
		WyncJoin_get_my_wync_peer_id(client_gs.wctx),
		actor_id);

	util_setup_lerp_types(client_gs.wctx);

	TEST_INT(WyncTrack_track_entity(client_gs.wctx, actor_id, actor_type), OK);
	util_setup_tracking_ball(
			&client_gs, actor_id, &pos_prop_id, &input_prop_id, true);

	util_force_WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	util_force_WyncWrapper_client_filter_prop_ids(client_gs.wctx);


	// server, extract data from prop and send
	// ----------------------------------------------------------

	Ball *server_ball_instance = &server_gs.balls[actor_id];
	Ball *client_ball_instance = &client_gs.balls[actor_id];
	server_ball_instance->position = (Vector2i) { 100, 100 };

	WyncWrapper_extract_data_to_tick(server_gs.wctx, server_gs.wctx->common.ticks);

	// server, populate entity update queue

	WyncPacket_set_data_limit_chars_for_out_packets(server_gs.wctx, 100000);
	WyncThrottle_system_fill_entity_sync_queue(server_gs.wctx);
	WyncThrottle_compute_entity_sync_order(server_gs.wctx);
	WyncSend_extracted_data(server_gs.wctx);
	WyncSend_queue_out_snapshots_for_delivery(server_gs.wctx);

	// server, send packets to client

	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);


	// client, confirm snapshot arrival
	// ----------------------------------------------------------

	uint received_snaps_pkts = client_gs.wctx->co_metrics.debug_packets_received
		[WYNC_PKT_PROP_SNAP][0];
	TEST_INT(received_snaps_pkts, 1);

	// client, test ball updated state

	WyncState_reset_props_to_latest_value(client_gs.wctx);

	TEST_INT(client_ball_instance->position.x, server_ball_instance->position.x);
	TEST_INT(client_ball_instance->position.y, server_ball_instance->position.y);

	// server, update ball position and update client again

	server_ball_instance->position = (Vector2i) { 200, 200 };

	WyncClock_advance_ticks(server_gs.wctx);
	WyncWrapper_extract_data_to_tick(server_gs.wctx, server_gs.wctx->common.ticks);

	// server, populate entity update queue
	WyncPacket_set_data_limit_chars_for_out_packets(server_gs.wctx, 100000);
	WyncThrottle_system_fill_entity_sync_queue(server_gs.wctx);
	WyncThrottle_compute_entity_sync_order(server_gs.wctx);
	WyncSend_extracted_data(server_gs.wctx);
	WyncSend_queue_out_snapshots_for_delivery(server_gs.wctx);

	// server, send packets to client
	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);


	// client, lerp confirmed states
	// ----------------------------------------------------------

	received_snaps_pkts = client_gs.wctx->co_metrics.debug_packets_received
		[WYNC_PKT_PROP_SNAP][0];
	TEST_INT(received_snaps_pkts, 2);

	/*WyncState_reset_props_to_latest_value(client_gs.wctx);*/
	WyncLerp_precompute(client_gs.wctx);

	// simulate drawing loop

	for (int i = -10, k = 0; i < 50; i += 10, ++k) {
		float physics_fraction = (float)i / GAME_TPS;
		printf("Lerp cycle %u: physics_fraction %f\n", k, physics_fraction);
		WyncLerp_interpolate_all(client_gs.wctx, physics_fraction);
	}

	TEST_TRUE(client_ball_instance->position.x >= 0); // min possible lerp
	TEST_TRUE(client_ball_instance->position.x <= 300); // max possible lerp
	TEST_TRUE(client_ball_instance->position.y >= 0);
	TEST_TRUE(client_ball_instance->position.y <= 300);

	util_simulate_game_engine_logic_cycle ();

	TESTS_SHOW_RESULTS();
}


// TODO: Improve tests for
// * Inputs, client ownership, extrapolation, interpolation?
// * Despawning
// * snapshot selection for lerping
// * Registering lerp types, (throttling) data limit
// * Xtrap, exact amount of ticks to predict, hard set the values that
//       calculate that, get last received tick, saved state on end tick (curr)


int main (void) {
	test_join();
	test_tracking();
	test_snapshot();
	test_client_authority_inputs();
	test_extrapolation();
	test_lerp_canonic_state();
	return SIMPLE_TEST_CODE;
}
