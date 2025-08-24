#include "assert.h"
#include "common.h"
#include "simpletest.h"
#include "../wync.h"
#include "../src/wync_private.h"


GameState server_gs;
GameState client_gs;


void util_reset_state(void) {
	memset(&server_gs, 0, sizeof(GameState));
	memset(&client_gs, 0, sizeof(GameState));
}


void util_send_packets_to (int from_net_peer_id, WyncCtx *from, WyncCtx *to)
{
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
}

void util_setup_server_and_client (void) {
	server_gs.network_peer_id = 0;
	server_gs.wctx = WyncInit_create_context();
	WyncFlow_server_setup(server_gs.wctx);

	client_gs.network_peer_id = 1;
	client_gs.wctx = WyncInit_create_context();
	WyncFlow_client_setup(client_gs.wctx);
}

void util_client_joins_server (void) {
	WyncJoin_set_my_nete_peer_id(client_gs.wctx, client_gs.network_peer_id);
	WyncJoin_set_server_nete_peer_id(client_gs.wctx, server_gs.network_peer_id);

	WyncJoin_service_wync_try_to_connect(client_gs.wctx);
	util_send_packets_to(client_gs.network_peer_id, client_gs.wctx, server_gs.wctx);
	util_send_packets_to(server_gs.network_peer_id, server_gs.wctx, client_gs.wctx);
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
	TEST_INT(received_spawn_events, 1);

	// client, test ball updated state

	Ball *client_ball_instance = &client_gs.balls[actor_id];

	TEST_INT(client_ball_instance->position.x, 0);
	TEST_INT(client_ball_instance->position.y, 0);

	WyncState_reset_props_to_latest_value(client_gs.wctx);

	TEST_FALSE(client_ball_instance->position.x == 0);
	TEST_FALSE(client_ball_instance->position.y == 0);
	TEST_INT(client_ball_instance->position.x, ball_instance->position.x);
	TEST_INT(client_ball_instance->position.y, ball_instance->position.y);

	// client authority



	// TODO: Inputs, client ownership, extrapolation, interpolation?
	// TODO: Despawning
	// TODO: snapshot selection for lerping
	// TODO: Registering lerp types, (throttling) data limit
	// TODO: Xtrap, exact amount of ticks to predict, hard set the values that
	//       calculate that, get last received tick, saved state on end tick (curr)

	TESTS_SHOW_RESULTS();
}


void util_setup_tracking_ball (
	GameState* gs, uint entity_id, uint *pos_prop_id, uint *input_prop_id
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
	util_setup_tracking_ball(&server_gs, actor_id, &pos_prop_id, &input_prop_id);
	TEST_FALSE(pos_prop_id == 999);
	TEST_FALSE(input_prop_id == 999);

	// server, give client authority over it's input

	WyncInput_prop_set_client_owner(
		server_gs.wctx, input_prop_id, client_gs.wctx->common.my_peer_id);

	WyncWrapper_server_filter_prop_ids(server_gs.wctx);
	size_t server_input_props = u32_DynArr_get_size(
		&server_gs.wctx->co_filter_s.filtered_clients_input_and_event_prop_ids);
	TEST_UINT(server_input_props, 1);

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
			&client_gs, actor_id, &client_pos_prop_id, &client_input_prop_id);
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

	WyncWrapper_client_filter_prop_ids(client_gs.wctx);

	int owned_props_amount = u32_DynArr_get_size(
			&client_gs.wctx->co_filter_c.type_input_event__owned_prop_ids);
	TEST_INT(owned_props_amount, 1);

	client_gs.wctx->co_pred.target_tick = 200;
	WyncWrapper_buffer_inputs(client_gs.wctx);
	client_gs.wctx->co_pred.target_tick = 201;

	// client, send inputs to server

	WyncPacket_set_data_limit_chars_for_out_packets(client_gs.wctx, 50000);
	WyncSend_client_send_inputs(client_gs.wctx);
	util_send_packets_to(client_gs.network_peer_id, client_gs.wctx, server_gs.wctx);

	TEST_INT(server_gs.wctx->co_metrics.debug_packets_received[
		WYNC_PKT_INPUTS][0], 1);

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

	TESTS_SHOW_RESULTS();
}


int main (void) {
	test_join();
	test_tracking();
	test_snapshot();
	test_client_authority_inputs();
}
