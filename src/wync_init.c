#include "wync_private.h"


// Intend:
// 1. Have all (most) allocations in one place
// 2. Remove initialization from class declarations

// Q: Isn't it better for each module to manage it's initialization?
// A: Do that, where it makes sense


void wync_init_ctx_common(WyncCtx *ctx) {
	Wync_CoCommon *common = &ctx->common;
	common->max_peers = 4;
	common->physic_ticks_per_second = 60;
	common->my_peer_id = -1;
	common->my_nete_peer_id = -1;;
	common->max_amount_cache_events = 1024;
	common->max_prop_relative_sync_history_ticks = 20;
	common->max_age_user_events_for_consumption = 120;

	common->out_reliable_packets = WyncPacketOut_DynArr_create();
	common->out_unreliable_packets = WyncPacketOut_DynArr_create();

	common->peer_latency_info = calloc
		(sizeof(*common->peer_latency_info), common->max_peers);
	common->client_has_info = calloc
		(sizeof(*common->client_has_info), common->max_peers);
}


void wync_init_ctx_state_tracking(WyncCtx *ctx) {
	CoStateTrackingCommon *co_track = &ctx->co_track;

	u16 max_peers = ctx->common.max_peers;
	co_track->REGULAR_PROP_CACHED_STATE_AMOUNT = 8;
	co_track->prop_id_cursor = 0;

	ConMap_init(&co_track->tracked_entities);
	co_track->props = calloc (sizeof(WyncProp), MAX_PROPS);
	ConMap_init(&co_track->active_prop_ids);
	u32_DynArr_ConMap_init(&co_track->entity_has_props);
	ConMap_init(&co_track->entity_is_of_type);

	// NOTE: index 0 not used
	co_track->client_has_relative_prop_has_last_tick =
		calloc (sizeof(ConMap), max_peers);

	ConMap *map;
	for (u32 peer_id = 0; peer_id < max_peers; ++peer_id) {
		map = &co_track->client_has_relative_prop_has_last_tick[peer_id];
		ConMap_init(map);
	}
}

void wync_init_ctx_clientauth(WyncCtx *ctx) {
	u16 max_peers = ctx->common.max_peers;
	ctx->co_clientauth.client_owns_prop = calloc (sizeof(ConMap), max_peers);

	ConMap *map = NULL;
	for (u32 peer_id = 0; peer_id < max_peers; ++peer_id) {
		map = &ctx->co_clientauth.client_owns_prop[peer_id];
		ConMap_init(map);
	}
	return;
}

void wync_init_ctx_events(WyncCtx *ctx) {
	CoEvents *co_events = &ctx->co_events;
	u32 max_peers = ctx->common.max_peers;

	WyncEvent_ConMap_init(&co_events->events);

	co_events->peer_has_channel_has_events =
		calloc (sizeof(*co_events->peer_has_channel_has_events), max_peers);
	co_events->prop_id_by_peer_by_channel =
		calloc (sizeof(*co_events->prop_id_by_peer_by_channel), max_peers);

	//i32[MAX_CHANNELS] *channels_prop_id;
	//i32 (*channels_prop_id) [MAX_CHANNELS];
	//i32* prop_id;
	//u32_DynArr (*channels_dyna) [MAX_CHANNELS];
	u32_DynArr* dyna;
	for (u32 peer_id = 0; peer_id < max_peers; ++peer_id) {
		//channels_prop_id = &co_events->prop_id_by_peer_by_channel[peer_id];
		//*channels_prop_id = calloc (sizeof(i32), MAX_CHANNELS);
		//channels_dyna = &co_events->peer_has_channel_has_events[peer_id];
		//channels_dyna = calloc (sizeof(u32_DynArr), MAX_CHANNELS);

		//peer_channels = &co_events->prop_id_by_peer_by_channel[peer_id];
		//*peer_channels = calloc(sizeof(u32), MAX_CHANNELS);

		for (u32 channel_id = 0; channel_id < MAX_CHANNELS; ++channel_id) {
			dyna = &co_events->peer_has_channel_has_events[peer_id][channel_id];
			*dyna = u32_DynArr_create();

			co_events->prop_id_by_peer_by_channel[peer_id][channel_id] = -1;
		}
	}

	/*
	co_events->peer_has_channel_has_events.resize(max_peers)
	co_events->prop_id_by_peer_by_channel.resize(max_peers)

	for peer_i in range(max_peers):
		co_events->peer_has_channel_has_events[peer_i] = []
		co_events->peer_has_channel_has_events[peer_i].resize(max_channels)
		co_events->prop_id_by_peer_by_channel[peer_i] = []
		co_events->prop_id_by_peer_by_channel[peer_i].resize(max_channels)
		for channel_i in range(max_channels):
			co_events->peer_has_channel_has_events[peer_i][channel_i] = []
			co_events->prop_id_by_peer_by_channel[peer_i][channel_i] = -1
			*/
}

void wync_init_ctx_metrics(WyncCtx *ctx) {
	CoMetrics *co_metrics = &ctx->co_metrics;

	co_metrics->debug_data_per_tick_sliding_window_size = 8;
	co_metrics->low_priority_entity_update_rate_sliding_window_size = 8;
	co_metrics->PROP_ID_PROB = -1;

	co_metrics->debug_data_per_tick_sliding_window = u32_RinBuf_create
		(co_metrics->debug_data_per_tick_sliding_window_size, 0);

	co_metrics->server_tick_rate_sliding_window = u32_RinBuf_create
		(SERVER_TICK_RATE_SLIDING_WINDOW_SIZE, 0);

	co_metrics->low_priority_entity_update_rate_sliding_window = i32_RinBuf_create
		(co_metrics->low_priority_entity_update_rate_sliding_window_size, 0);

	for (u32 i = 0; i < WYNC_PKT_AMOUNT; ++i) {
		co_metrics->debug_packets_received[i] = calloc
			(sizeof(u32), DEBUG_PACKETS_RECEIVED_MAX);
	}
}


void wync_init_ctx_spawn (WyncCtx *ctx){
	CoSpawn *co_spawn = &ctx->co_spawn;
	WyncState_ConMap_init(&co_spawn->entity_spawn_data);
	co_spawn->out_queue_spawn_events = SpawnEvent_FIFORing_init(1024);
	//co_spawn->next_entity_to_spawn = (Wync_EntitySpawnEvent){ 0 };
	EntitySpawnPropRange_ConMap_init(&co_spawn->pending_entity_to_spawn_props);
	co_spawn->despawned_entity_ids = u32_DynArr_create();
}



void wync_init_ctx_throttling (WyncCtx *ctx){
	u32 max_peers = ctx->common.max_peers;
	CoThrottling *co_throt = &ctx->co_throttling;

	co_throt->clients_sees_entities = calloc (sizeof(ConMap), max_peers);
	co_throt->clients_sees_new_entities = calloc (sizeof(ConMap), max_peers);
	co_throt->clients_no_longer_sees_entities = calloc (sizeof(ConMap), max_peers);

	for (u32 peer_id = 0; peer_id < max_peers; ++peer_id) {
		ConMap_init(&co_throt->clients_sees_entities[peer_id]);
		ConMap_init(&co_throt->clients_sees_new_entities[peer_id]);
		ConMap_init(&co_throt->clients_no_longer_sees_entities[peer_id]);
	}
	
	// Queues
	// vvv
	
	co_throt->queue_clients_entities_to_sync = calloc
		(sizeof(u32_FIFORing), max_peers);
	co_throt->entities_synced_last_time = calloc
		(sizeof(ConMap), max_peers);
	co_throt->queue_entity_pairs_to_sync = Wync_PeerEntityPair_DynArr_create();

	co_throt->rela_prop_ids_for_full_snapshot = u32_DynArr_create();
	co_throt->pending_rela_props_to_sync_to_peer =
		Wync_PeerPropPair_DynArr_create();
	co_throt->out_peer_pending_to_setup = u32_DynArr_create();
	
	co_throt->clients_cached_reliable_snapshots =
		calloc(sizeof(WyncSnap_DynArr), max_peers);
	co_throt->clients_cached_unreliable_snapshots =
		calloc(sizeof(WyncSnap_DynArr), max_peers);
	
	co_throt->peers_events_to_sync = calloc (sizeof(ConMap), max_peers);

	for (u32 peer_id = 0; peer_id < max_peers; ++peer_id) {
		co_throt->queue_clients_entities_to_sync[peer_id] =
			u32_FIFORing_init(128);
		ConMap_init(&co_throt->entities_synced_last_time[peer_id]);

		co_throt->clients_cached_reliable_snapshots[peer_id] =
			WyncSnap_DynArr_create();
		co_throt->clients_cached_unreliable_snapshots[peer_id] =
			WyncSnap_DynArr_create();

		ConMap_init(&co_throt->peers_events_to_sync[peer_id]);
	}
}


void wync_init_ctx_ticks(WyncCtx *ctx) {
	ctx->co_ticks.server_tick_offset_collection = 
		calloc(sizeof(Wync_i32Pair), SERVER_TICK_OFFSET_COLLECTION_SIZE);
	ctx->co_ticks.start_time_ms = WyncClock_get_system_milliseconds();
}


void wync_init_ctx_prediction_data(WyncCtx *ctx) {
	CoPredictionData *co_pred = &ctx->co_pred;

	co_pred->clock_offset_sliding_window_size = 16;
	co_pred->clock_offset_sliding_window = 
		double_RinBuf_create(co_pred->clock_offset_sliding_window_size, 0);

	co_pred->global_entity_ids_to_predict = u32_DynArr_create();

	ConMap_init(&co_pred->entity_last_predicted_tick);
	ConMap_init(&co_pred->entity_last_received_tick);
	co_pred->predicted_entity_ids = u32_DynArr_create();

	co_pred->first_tick_predicted = 1;
	co_pred->last_tick_predicted = 0;

	// FUTURE
	//co_pred->tick_action_history_size = 32;
	//co_pred->tick_action_history = \
		//RingBuffer.new(co_pred->tick_action_history_size, {});
	//for i in range(co_pred->tick_action_history_size):
		//co_pred.tick_action_history.insert_at(i, {} as Dictionary);
}


void wync_init_ctx_lerp(WyncCtx *ctx) {
	ctx->co_lerp.lerp_ms = 50;
	ctx->co_lerp.lerp_latency_ms = 0;
	ctx->co_lerp.max_lerp_factor_symmetric = 1.0;
}


void wync_init_ctx_dummy(WyncCtx *ctx) {
	DummyProp_ConMap_init(&ctx->co_dummy.dummy_props);
}


void wync_init_ctx_filter_s(WyncCtx *ctx) {
	CoFilterServer *filter = &ctx->co_filter_s;
	filter->filtered_clients_input_and_event_prop_ids = u32_DynArr_create();
	filter->filtered_delta_prop_ids = u32_DynArr_create();
	filter->filtered_regular_extractable_prop_ids = u32_DynArr_create();
	filter->filtered_regular_timewarpable_prop_ids = u32_DynArr_create();
	filter->filtered_regular_timewarpable_interpolable_prop_ids =
		u32_DynArr_create();
}


void wync_init_ctx_filter_c(WyncCtx *ctx) {
	CoFilterClient *filter = &ctx->co_filter_c;
	filter->type_input_event__owned_prop_ids = u32_DynArr_create();
	filter->type_input_event__predicted_owned_prop_ids = u32_DynArr_create();
	filter->type_event__predicted_prop_ids = u32_DynArr_create();
	filter->type_state__delta_prop_ids = u32_DynArr_create();
	filter->type_state__predicted_delta_prop_ids = u32_DynArr_create();
	filter->type_state__predicted_regular_prop_ids = u32_DynArr_create();
	filter->type_state__interpolated_regular_prop_ids = u32_DynArr_create();
	filter->type_state__newstate_prop_ids = u32_DynArr_create();
}
