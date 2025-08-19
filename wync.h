#ifndef WYNC_H
#define WYNC_H

#define WYNC_WRAPPER

#include "stdbool.h"
#include "stdint.h"

typedef struct WyncCtx WyncCtx;

enum WYNC_PROP_TYPE {
    WYNC_PROP_TYPE_STATE,
    WYNC_PROP_TYPE_INPUT,
    WYNC_PROP_TYPE_EVENT // can only store Array[int]
};

typedef struct {
    uint32_t data_size;
    void *data;
} WyncWrapper_Data;

typedef struct {
    void *ctx;
    uint32_t type_size;
} WyncWrapper_UserCtx;

typedef WyncWrapper_Data (*WyncWrapper_Getter)(WyncWrapper_UserCtx ctx);

typedef void (*WyncWrapper_Setter)(WyncWrapper_UserCtx, WyncWrapper_Data data);

typedef WyncWrapper_Data (*WyncWrapper_LerpFunc)(
    WyncWrapper_Data from, WyncWrapper_Data to, float delta);

typedef struct {
    bool spawn; // wether to spawn or to dispawn
    uint32_t entity_id;
    uint32_t entity_type_id;
    uint32_t spawn_data_size;
    void *spawn_data;
} Wync_EntitySpawnEvent;

// Packets handed to you for sending through the network
// it includes the destination nete peer

typedef struct {
	uint16_t to_nete_peer_id;
	uint32_t data_size;
	void *data; // WyncPacket
} WyncPacketOut;

/// ---------------------------------------------------------------------------
/// WYNC STATISTICS
/// ---------------------------------------------------------------------------

// void WyncStat_try_to_update_prob_prop_rate(WyncCtx *ctx);

// void WyncStat_system_calculate_prob_prop_rate(WyncCtx *ctx);

// bool WyncPacket_type_exists(uint16_t packet_type_id);

/// Wrapper
/// vvvvvvv

#ifdef WYNC_WRAPPER
void WyncStat_setup_prob_for_entity_update_delay_ticks(
    WyncCtx *ctx, uint32_t peer_id);
#endif

/// ---------------------------------------------------------------------------
/// WYNC PACKET UTIL
/// ---------------------------------------------------------------------------

// int32_t WyncPacket_wrap_packet_out_alloc(
// WyncCtx *ctx, uint16_t to_wync_peer_id, uint16_t packet_type_id,
// uint32_t data_size, void *data, WyncPacketOut *out_packet);

// int32_t WyncPacket_try_to_queue_out_packet(
// WyncCtx *ctx, WyncPacketOut out_packet, bool reliable,
// bool already_commited,
// bool dont_ocuppy // default false
//);

// void WyncPacket_ocuppy_space_towards_packets_data_size_limit(
// WyncCtx *ctx, uint32_t bytes);

void WyncPacket_set_data_limit_chars_for_out_packets(
    WyncCtx *ctx, uint32_t data_limit_bytes);

/// ---------------------------------------------------------------------------
/// WYNC CLOCK
/// ---------------------------------------------------------------------------

// uint64_t WyncClock_get_system_milliseconds(void);

uint64_t WyncClock_get_ms(WyncCtx *ctx);

// void WyncClock_client_handle_pkt_clock(WyncCtx *ctx, WyncPktClock pkt);

// int32_t WyncClock_server_handle_pkt_clock_req(
// WyncCtx *ctx, WyncPktClock pkt, uint16_t from_nete_peer_id);

// void WyncClock_system_stabilize_latency(
// WyncCtx *ctx, Wync_PeerLatencyInfo *lat_info);

// void WyncClock_update_prediction_ticks(WyncCtx *ctx);

// int32_t WyncClock_client_ask_for_clock(WyncCtx *ctx);

// void WyncClock_advance_ticks(WyncCtx *ctx);

void WyncClock_peer_set_current_latency(
    WyncCtx *ctx, uint16_t peer_id, uint16_t latency_ms);

void WyncClock_wync_client_set_physics_ticks_per_second(
    WyncCtx *ctx, uint16_t tps);

void WyncClock_set_debug_time_offset(WyncCtx *ctx, uint64_t time_offset_ms);

// float WyncClock_get_tick_timestamp_ms(WyncCtx *ctx, int32_t ticks);

void WyncClock_set_ticks(WyncCtx *ctx, uint32_t ticks);

uint32_t WyncClock_get_ticks(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC DEBUG
/// ---------------------------------------------------------------------------

// void WyncDebug_log_packet_received(WyncCtx *ctx, uint16_t
// packet_type_id);

// void WyncDebug_received_log_prop_id(
// WyncCtx *ctx, uint16_t packet_type_id, uint32_t prop_id);

/// ---------------------------------------------------------------------------
/// WYNC FLOW
/// ---------------------------------------------------------------------------

int32_t wync_flow_wync_feed_packet(
    WyncCtx *ctx, uint16_t from_nete_peer_id, uint32_t data_size, void *data);

int32_t wync_flow_server_setup(WyncCtx *ctx);

void wync_flow_client_setup(WyncCtx *ctx);

//void wync_flow_setup_context(WyncCtx *ctx); // Note: hide it?

void wync_flow_wync_server_tick_start(WyncCtx *ctx);

void wync_flow_wync_server_tick_end(WyncCtx *ctx);

void wync_flow_wync_client_tick_end(WyncCtx *ctx);

void wync_flow_wync_system_gather_packets(WyncCtx *ctx);

void WyncFlow_gather_packets(WyncCtx *ctx);

void WyncFlow_packet_cleanup(WyncCtx *ctx);

int32_t WyncFlow_get_next_reliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);

int32_t WyncFlow_get_next_unreliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);

/// ---------------------------------------------------------------------------
/// WYNC INIT
/// ---------------------------------------------------------------------------

WyncCtx *WyncInit_create_context(void);
// void wync_init_ctx_common(WyncCtx *ctx);
// void wync_init_ctx_state_tracking(WyncCtx *ctx);
// void wync_init_ctx_clientauth(WyncCtx *ctx);
// void wync_init_ctx_events(WyncCtx *ctx);
// void wync_init_ctx_metrics(WyncCtx *ctx);
// void wync_init_ctx_spawn(WyncCtx *ctx);
// void wync_init_ctx_throttling(WyncCtx *ctx);
// void wync_init_ctx_ticks(WyncCtx *ctx);
// void wync_init_ctx_prediction_data(WyncCtx *ctx);
// void wync_init_ctx_lerp(WyncCtx *ctx);
// void wync_init_ctx_dummy(WyncCtx *ctx);
// void wync_init_ctx_filter_s(WyncCtx *ctx);
// void wync_init_ctx_filter_c(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC INPUT
/// ---------------------------------------------------------------------------

// int32_t WyncInput_prop_get_peer_owner(WyncCtx *ctx, uint32_t prop_id,
// uint32_t *out_prop_id);

int32_t WyncInput_prop_set_client_owner(
    WyncCtx *ctx, uint32_t prop_id, uint16_t client_id);

// void WyncInput_system_sync_client_ownership(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC JOIN
/// ---------------------------------------------------------------------------

bool WyncJoin_is_connected(WyncCtx *ctx);

int32_t WyncJoin_is_peer_registered(
    WyncCtx *ctx, uint16_t nete_peer_id, uint16_t *out_wync_peer_id);

// int32_t WyncJoin_get_nete_peer_id_from_wync_peer_id(
// WyncCtx *ctx, uint16_t wync_peer_id, int32_t *out_nete_peer_id);

// int32_t WyncJoin_get_wync_peer_id_from_nete_peer_id(
// WyncCtx *ctx, uint16_t nete_peer_id, uint16_t *out_wync_peer_id);

void WyncJoin_set_my_nete_peer_id(WyncCtx *ctx, uint16_t nete_peer_id);

void WyncJoin_set_server_nete_peer_id(WyncCtx *ctx, uint16_t nete_peer_id);

// uint16_t WyncJoin_peer_register(WyncCtx *ctx, int32_t nete_peer_id);

// int32_t WyncJoin_service_wync_try_to_connect(WyncCtx *ctx);

// int32_t WyncJoin_client_setup_my_client(WyncCtx *ctx, uint16_t
// peer_id);

// int32_t WyncJoin_handle_pkt_join_res(WyncCtx *ctx, WyncPktJoinRes pkt);

// int32_t WyncJoin_handle_pkt_join_req(
// WyncCtx *ctx, WyncPktJoinReq pkt, uint16_t from_nete_peer_id);

// void WyncJoin_handle_pkt_res_client_info(
// WyncCtx *ctx, WyncPktResClientInfo pkt);

void WyncJoin_clear_peers_pending_to_setup(WyncCtx *ctx);

// bool WyncJoin_out_client_just_connected_to_server(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC LERP
/// ---------------------------------------------------------------------------

void WyncLerp_client_set_lerp_ms(
    WyncCtx *ctx, float server_tick_rate, uint32_t lerp_ms);

void WyncLerp_set_max_lerp_factor_symmetric(
    WyncCtx *ctx, float max_lerp_factor_symmetric);

// void WyncLerp_handle_packet_client_set_lerp_ms(
// WyncCtx *ctx, WyncPktClientSetLerpMS pkt, uint32_t from_nete_peer_id);

// void WyncLerp_precompute(WyncCtx *ctx);

void WyncLerp_register_lerp_type(
    WyncCtx *ctx, uint16_t user_type_id, WyncWrapper_LerpFunc lerp_func);

void WyncLerp_interpolate_all(WyncCtx *ctx, float delta_lerp_fraction);

/// ---------------------------------------------------------------------------
/// WYNC PROP
/// ---------------------------------------------------------------------------

/// @returns error
int32_t WyncProp_enable_prediction(WyncCtx *ctx, uint32_t prop_id);

int32_t WyncProp_enable_interpolation(
    WyncCtx *ctx, uint32_t prop_id, uint16_t user_data_type,
    WyncWrapper_Setter setter_lerp);

/// ---------------------------------------------------------------------------
/// WYNC SPAWN
/// ---------------------------------------------------------------------------

// void WyncSpawn_handle_pkt_spawn(WyncCtx *ctx, WyncPktSpawn pkt);

// void WyncSpawn_handle_pkt_despawn(WyncCtx *ctx, WyncPktDespawn pkt);

int32_t WyncSpawn_get_next_entity_event_spawn(
    WyncCtx *ctx, Wync_EntitySpawnEvent *out_spawn_event);

void WyncSpawn_finish_spawning_entity(WyncCtx *ctx, uint32_t entity_id);

void WyncSpawn_system_spawned_props_cleanup(WyncCtx *ctx);

// void WyncSpawn_system_send_entities_to_spawn(WyncCtx *ctx);

// void WyncSpawn_system_send_entities_to_despawn(WyncCtx *ctx);

// void _wync_confirm_client_can_see_entity(
// WyncCtx *ctx, uint16_t client_id, uint32_t entity_id);

/// ---------------------------------------------------------------------------
/// WYNC STATE SEND
/// ---------------------------------------------------------------------------

// int32_t WyncSend__wync_sync_regular_prop(
// WyncProp *prop, uint32_t prop_id, uint32_t tick, WyncSnap *out_snap);

// void WyncSend_extracted_data(WyncCtx *ctx);

// void WyncSend_queue_out_snapshots_for_delivery(WyncCtx *ctx);

// void WyncSend_system_update_delta_base_state_tick(WyncCtx *ctx);

// void WyncSend_client_send_inputs(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC STATE SET
/// ---------------------------------------------------------------------------

// void WyncState_reset_all_state_to_confirmed_tick_absolute(
// WyncCtx *ctx, uint32_t *prop_ids, uint32_t prop_id_amount, uint32_t
// tick);

// void WyncState_reset_all_state_to_confirmed_tick_relative(
// WyncCtx *ctx, uint32_t *prop_ids, uint32_t prop_id_amount, uint32_t
// tick);

// void WyncState_reset_props_to_latest_value(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC STATE STORE
/// ---------------------------------------------------------------------------

// void WyncStore_prop_state_buffer_insert(
// WyncCtx *ctx, WyncProp *prop, int32_t tick, WyncState state);

// void WyncStore_prop_state_buffer_insert_in_place(
// WyncCtx *ctx, WyncProp *prop, int32_t tick, WyncState state);

// int32_t WyncStore_save_confirmed_state(
// WyncCtx *ctx, uint32_t prop_id, uint32_t tick, WyncState state);

// void WyncStore_client_update_last_tick_received(WyncCtx *ctx, uint32_t
// tick);

// void WyncStore_handle_pkt_prop_snap(WyncCtx *ctx, WyncPktSnap pkt);

// int32_t WyncStore_save_confirmed_state(
// WyncCtx *ctx, uint32_t prop_id, uint32_t tick, WyncState state);

// void WyncStore_service_cleanup_dummy_props(WyncCtx *ctx);

// int32_t WyncStore_server_handle_pkt_inputs(
// WyncCtx *ctx, WyncPktInputs pkt, uint16_t from_nete_peer_id);

// int32_t WyncStore_client_handle_pkt_inputs(WyncCtx *ctx, WyncPktInputs
// pkt);

// void WyncStore_prop_state_buffer_insert(
// WyncCtx *ctx, WyncProp *prop, int32_t tick, WyncState state);

// void WyncStore_prop_state_buffer_insert_in_place(
// WyncCtx *ctx, WyncProp *prop, int32_t tick, WyncState state);

// int32_t WyncStore_insert_state_to_entity_prop(
// WyncCtx *ctx, uint32_t entity_id, const char *prop_name_id, uint32_t
// tick, WyncState state);

// WyncState WyncState_prop_state_buffer_get(WyncProp *prop, int32_t tick);

// WyncState WyncState_prop_state_buffer_get_throughout(WyncProp *prop, uint32_t
// tick);

/// ---------------------------------------------------------------------------
/// WYNC THROTTLE
/// ---------------------------------------------------------------------------

// void WyncThrottle__remove_entity_from_sync_queue(
// WyncCtx *ctx, uint16_t peer_id, uint32_t entity_id);

// void WyncThrottle_system_fill_entity_sync_queue(WyncCtx *ctx);

// void WyncThrottle_compute_entity_sync_order(WyncCtx *ctx);

int32_t WyncThrottle_client_now_can_see_entity(
    WyncCtx *ctx, uint16_t client_id, uint32_t entity_id);

void WyncThrottle_everyone_now_can_see_entity(WyncCtx *ctx, uint32_t entity_id);

void WyncThrottle_entity_set_spawn_data(
    WyncCtx *ctx, uint32_t entity_id, uint32_t data_size, void *data);

int32_t WyncThrottle_client_no_longer_sees_entity(
    WyncCtx *ctx, uint16_t client_id, uint32_t entity_id);

/// ---------------------------------------------------------------------------
/// WYNC TICK COLLECTION
/// ---------------------------------------------------------------------------

// void WyncOffsetCollection_replace_value(
// CoTicks *co_ticks, int32_t find_value, int32_t new_value);

// void WyncOffsetCollection_increase_value(CoTicks *co_ticks, int32_t
// find_value);

// bool WyncOffsetCollection_value_exists(CoTicks *co_ticks, int32_t ar_value);

// int32_t WyncOffsetCollection_get_most_common(CoTicks *co_ticks);

// int32_t WyncOffsetCollection_get_less_common(CoTicks *co_ticks);

// void WyncOffsetCollection_add_value(CoTicks *co_ticks, int32_t new_value);

/// ---------------------------------------------------------------------------
/// WYNC TRACK
/// ---------------------------------------------------------------------------

int32_t WyncTrack_track_entity(
    WyncCtx *ctx, uint32_t entity_id, uint32_t entity_type_id);

void WyncTrack_untrack_entity(WyncCtx *ctx, uint32_t entity_id);

// void WyncTrack_delete_prop(WyncCtx *ctx, uint32_t prop_id);

bool WyncTrack_is_entity_tracked(WyncCtx *ctx, uint32_t entity_id);

// int32_t WyncTrack_get_new_prop_id(WyncCtx *ctx, uint32_t *out_prop_id);

int32_t WyncTrack_prop_register_minimal(
    WyncCtx *ctx, uint32_t entity_id, const char *name_id,
    enum WYNC_PROP_TYPE data_type, uint32_t *out_prop_id);

// WyncProp *WyncTrack_get_prop(WyncCtx *ctx, uint32_t prop_id);

// WyncProp *WyncTrack_entity_get_prop(
// WyncCtx *ctx, uint32_t entity_id, const char *prop_name_id);

int32_t WyncTrack_entity_get_prop_id(
    WyncCtx *ctx, uint32_t entity_id, const char *prop_name_id,
    uint32_t *out_prop_id);

/// Note: Better have a structure for direct access instead of searching
/// @param[out] out_entity_id Pointer to Instance
/// @returns error
int32_t WyncTrack_prop_get_entity(
    WyncCtx *ctx, uint32_t prop_id, uint32_t *out_entity_id);

// bool WyncTrack_is_entity_tracked(WyncCtx *ctx, uint32_t entity_id);

/// @returns Optional WyncProp
/// @retval NULL Not found / Not enabled
// WyncProp *WyncTrack_get_prop(WyncCtx *ctx, uint32_t prop_id);

// WyncProp *WyncTrack_get_prop_unsafe(WyncCtx *ctx, uint32_t prop_id);

// int32_t WyncTrack_prop_register_update_dummy(
// WyncCtx *ctx, uint32_t prop_id, uint32_t last_tick, uint32_t data_size,
// void *data);

int32_t WyncTrack_wync_add_local_existing_entity(
    WyncCtx *ctx, uint16_t wync_client_id, uint32_t entity_id);

/// ---------------------------------------------------------------------------
/// WYNC XTRAP
/// ---------------------------------------------------------------------------

int32_t WyncXtrap_preparation(WyncCtx *ctx);

// void WyncXtrap_regular_entities_to_predict(WyncCtx *ctx, int32_t tick);

void WyncXtrap_termination(WyncCtx *ctx);

// bool WyncXtrap_is_entity_predicted (WyncCtx *ctx, uint32_t entity_id);

void WyncXtrap_tick_init(WyncCtx *ctx, int32_t tick);

// void WyncXtrap_props_update_predicted_states_data (
// WyncCtx *ctx,
// uint32_t *prop_ids,
// uint32_t prop_id_amount
//);

// static void WyncXtrap_props_update_predicted_states_ticks (
// WyncCtx *ctx,
// uint32_t target_tick,
// uint32_t *prop_ids,
// uint32_t prop_id_amount
//);

// void WyncXtrap_save_latest_predicted_state (WyncCtx *ctx, int32_t
// tick);

// void WyncXtrap_delta_props_clear_current_delta_events (WyncCtx *ctx);

// static int32_t WyncXtrap_entity_get_last_received_tick_from_pred_props (
// WyncCtx *ctx,
// uint32_t entity_id
//);

// static void WyncXtrap_internal_tick_end(WyncCtx *ctx, int32_t tick);

/// NOTE: assuming snap props always include all snaps for an entity
// void WyncXtrap_update_entity_last_tick_received(
// WyncCtx *ctx,
// uint32_t prop_id
//);

void WyncXtrap_tick_end(WyncCtx *ctx, int32_t tick);

/// ---------------------------------------------------------------------------
/// WYNC WRAPPER UTIL
/// ---------------------------------------------------------------------------

// void WyncWrapper_initialize(WyncCtx *ctx);

void WyncWrapper_set_prop_callbacks(
    WyncCtx *ctx, uint32_t prop_id, WyncWrapper_UserCtx user_ctx,
    WyncWrapper_Getter getter, WyncWrapper_Setter setter);

// void WyncWrapper_buffer_inputs(WyncCtx *ctx);

// void WyncWrapper_extract_data_to_tick(WyncCtx *ctx, uint32_t
// save_on_tick);

// void WyncWrapper_server_filter_prop_ids(WyncCtx *ctx);

// void WyncWrapper_client_filter_prop_ids (WyncCtx *ctx);

#endif // !WYNC_H
