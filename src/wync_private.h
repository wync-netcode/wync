#ifndef WYNC_PRIVATE_H
#define WYNC_PRIVATE_H

#define WYNC_WRAPPER

#include "../wync.h"
#include "wync_typedef.h"

/*
   WyncCtx
   Wrapper**
   Wync_EntitySpawnEvent
   enum WYNC_PROP_TYPE
   */

/// ---------------------------------------------------------------------------
/// WYNC STATISTICS
/// ---------------------------------------------------------------------------

void WyncStat_try_to_update_prob_prop_rate(WyncCtx *ctx);

void WyncStat_system_calculate_prob_prop_rate(WyncCtx *ctx);

bool WyncPacket_type_exists(u16 packet_type_id);

/// Wrapper
/// vvvvvvv

// #ifdef WYNC_WRAPPER
// void WyncStat_setup_prob_for_entity_update_delay_ticks(
// WyncCtx *ctx, u32 peer_id);
// #endif

/// ---------------------------------------------------------------------------
/// WYNC PACKET UTIL
/// ---------------------------------------------------------------------------

i32 WyncPacket_wrap_packet_out_alloc(
    WyncCtx *ctx, u16 to_wync_peer_id, u16 packet_type_id, u32 data_size,
    void *data, WyncPacketOut *out_packet);

i32 WyncPacket_try_to_queue_out_packet(
    WyncCtx *ctx, WyncPacketOut out_packet, bool reliable,
    bool already_commited,
    bool dont_ocuppy // default false
);

void WyncPacket_ocuppy_space_towards_packets_data_size_limit(
    WyncCtx *ctx, u32 bytes);

void WyncPacket_set_data_limit_chars_for_out_packets(
    WyncCtx *ctx, u32 data_limit_bytes);

/// ---------------------------------------------------------------------------
/// WYNC CLOCK
/// ---------------------------------------------------------------------------

u64 WyncClock_get_system_milliseconds(void);

u64 WyncClock_get_ms(WyncCtx *ctx);

void WyncClock_client_handle_pkt_clock(WyncCtx *ctx, WyncPktClock pkt);

i32 WyncClock_server_handle_pkt_clock_req(
    WyncCtx *ctx, WyncPktClock pkt, u16 from_nete_peer_id);

void WyncClock_system_stabilize_latency(
    WyncCtx *ctx, Wync_PeerLatencyInfo *lat_info);

void WyncClock_update_prediction_ticks(WyncCtx *ctx);

i32 WyncClock_client_ask_for_clock(WyncCtx *ctx);

void WyncClock_advance_ticks(WyncCtx *ctx);

void WyncClock_peer_set_current_latency(
    WyncCtx *ctx, u16 peer_id, u16 latency_ms);

void WyncClock_wync_client_set_physics_ticks_per_second(WyncCtx *ctx, u16 tps);

void WyncClock_set_debug_time_offset(WyncCtx *ctx, u64 time_offset_ms);

void WyncClock_set_ticks(WyncCtx *ctx, u32 ticks);

float WyncClock_get_tick_timestamp_ms(WyncCtx *ctx, i32 ticks);

u32 WyncClock_get_ticks(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC DEBUG
/// ---------------------------------------------------------------------------

void WyncDebug_log_packet_received(WyncCtx *ctx, u16 packet_type_id);

void WyncDebug_received_log_prop_id(
    WyncCtx *ctx, u16 packet_type_id, u32 prop_id);

/// ---------------------------------------------------------------------------
/// WYNC FLOW
/// ---------------------------------------------------------------------------

i32 wync_flow_wync_feed_packet(
    WyncCtx *ctx, u16 from_nete_peer_id, u32 data_size, void *data);

i32 wync_flow_server_setup(WyncCtx *ctx);

void wync_flow_client_setup(WyncCtx *ctx);

void wync_flow_setup_context(WyncCtx *ctx); // Note: hide it?

void wync_flow_wync_server_tick_start(WyncCtx *ctx);

void wync_flow_wync_server_tick_end(WyncCtx *ctx);

void wync_flow_wync_client_tick_end(WyncCtx *ctx);

void WyncFlow_gather_packets(WyncCtx *ctx);

void WyncFlow_packet_cleanup(WyncCtx *ctx);

i32 WyncFlow_get_next_reliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);

i32 WyncFlow_get_next_unreliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);


/// ---------------------------------------------------------------------------
/// WYNC INIT
/// ---------------------------------------------------------------------------

WyncCtx *WyncInit_create_context(void);
void wync_init_ctx_common(WyncCtx *ctx);
void wync_init_ctx_state_tracking(WyncCtx *ctx);
void wync_init_ctx_clientauth(WyncCtx *ctx);
void wync_init_ctx_events(WyncCtx *ctx);
void wync_init_ctx_metrics(WyncCtx *ctx);
void wync_init_ctx_spawn(WyncCtx *ctx);
void wync_init_ctx_throttling(WyncCtx *ctx);
void wync_init_ctx_ticks(WyncCtx *ctx);
void wync_init_ctx_prediction_data(WyncCtx *ctx);
void wync_init_ctx_lerp(WyncCtx *ctx);
void wync_init_ctx_dummy(WyncCtx *ctx);
void wync_init_ctx_filter_s(WyncCtx *ctx);
void wync_init_ctx_filter_c(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC INPUT
/// ---------------------------------------------------------------------------

i32 WyncInput_prop_get_peer_owner(WyncCtx *ctx, u32 prop_id, u32 *out_prop_id);

i32 WyncInput_prop_set_client_owner(WyncCtx *ctx, u32 prop_id, u16 client_id);

void WyncInput_system_sync_client_ownership(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC JOIN
/// ---------------------------------------------------------------------------

bool WyncJoin_is_connected(WyncCtx *ctx);

i32 WyncJoin_is_peer_registered(
    WyncCtx *ctx, u16 nete_peer_id, u16 *out_wync_peer_id);

i32 WyncJoin_get_nete_peer_id_from_wync_peer_id(
    WyncCtx *ctx, u16 wync_peer_id, i32 *out_nete_peer_id);

i32 WyncJoin_get_wync_peer_id_from_nete_peer_id(
    WyncCtx *ctx, u16 nete_peer_id, u16 *out_wync_peer_id);

void WyncJoin_set_my_nete_peer_id(WyncCtx *ctx, u16 nete_peer_id);

i32 WyncJoin_get_my_wync_peer_id(WyncCtx *ctx);

void WyncJoin_set_server_nete_peer_id(WyncCtx *ctx, u16 nete_peer_id);

u16 WyncJoin_peer_register(WyncCtx *ctx, i32 nete_peer_id);

i32 WyncJoin_service_wync_try_to_connect(WyncCtx *ctx);

i32 WyncJoin_client_setup_my_client(WyncCtx *ctx, u16 peer_id);

i32 WyncJoin_handle_pkt_join_res(WyncCtx *ctx, WyncPktJoinRes pkt);

i32 WyncJoin_handle_pkt_join_req(
    WyncCtx *ctx, WyncPktJoinReq pkt, u16 from_nete_peer_id);

void WyncJoin_handle_pkt_res_client_info(
    WyncCtx *ctx, WyncPktResClientInfo pkt);

void WyncJoin_clear_peers_pending_to_setup(WyncCtx *ctx);

bool WyncJoin_out_client_just_connected_to_server(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC LERP
/// ---------------------------------------------------------------------------

void WyncLerp_client_set_lerp_ms(
    WyncCtx *ctx, float server_tick_rate, u32 lerp_ms);

void WyncLerp_set_max_lerp_factor_symmetric(
    WyncCtx *ctx, float max_lerp_factor_symmetric);

void WyncLerp_handle_packet_client_set_lerp_ms(
    WyncCtx *ctx, WyncPktClientSetLerpMS pkt, u32 from_nete_peer_id);

void WyncLerp_precompute(WyncCtx *ctx);

void WyncLerp_register_lerp_type(
    WyncCtx *ctx, u16 user_type_id, WyncWrapper_LerpFunc lerp_func);

void WyncLerp_interpolate_all(WyncCtx *ctx, float delta_lerp_fraction);

/// ---------------------------------------------------------------------------
/// WYNC PROP
/// ---------------------------------------------------------------------------

/// @returns error
i32 WyncProp_enable_prediction(WyncCtx *ctx, u32 prop_id);

i32 WyncProp_enable_interpolation(
    WyncCtx *ctx, u32 prop_id, u16 user_data_type,
    WyncWrapper_Setter setter_lerp);

/// ---------------------------------------------------------------------------
/// WYNC SPAWN
/// ---------------------------------------------------------------------------

void _wync_confirm_client_can_see_entity(
    WyncCtx *ctx, u16 client_id, u32 entity_id);

void WyncSpawn_handle_pkt_spawn(WyncCtx *ctx, WyncPktSpawn pkt);

void WyncSpawn_handle_pkt_despawn(WyncCtx *ctx, WyncPktDespawn pkt);

i32 WyncSpawn_get_next_entity_event_spawn(
    WyncCtx *ctx, Wync_EntitySpawnEvent *out_spawn_event);

void WyncSpawn_finish_spawning_entity(WyncCtx *ctx, u32 entity_id);

void WyncSpawn_system_spawned_props_cleanup(WyncCtx *ctx);

void WyncSpawn_system_send_entities_to_spawn(WyncCtx *ctx);

void WyncSpawn_system_send_entities_to_despawn(WyncCtx *ctx);

void _wync_confirm_client_can_see_entity(
    WyncCtx *ctx, u16 client_id, u32 entity_id);

/// ---------------------------------------------------------------------------
/// WYNC STATE SEND
/// ---------------------------------------------------------------------------

i32 WyncSend__wync_sync_regular_prop(
    WyncProp *prop, u32 prop_id, u32 tick, WyncSnap *out_snap);

void WyncSend_extracted_data(WyncCtx *ctx);

void WyncSend_queue_out_snapshots_for_delivery(WyncCtx *ctx);

void WyncSend_system_update_delta_base_state_tick(WyncCtx *ctx);

void WyncSend_client_send_inputs(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC STATE SET
/// ---------------------------------------------------------------------------

void WyncState_reset_all_state_to_confirmed_tick_absolute(
    WyncCtx *ctx, u32 *prop_ids, u32 prop_id_amount, u32 tick);

void WyncState_reset_all_state_to_confirmed_tick_relative(
    WyncCtx *ctx, u32 *prop_ids, u32 prop_id_amount, u32 tick);

void WyncState_reset_props_to_latest_value(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC STATE STORE
/// ---------------------------------------------------------------------------

void WyncStore_prop_state_buffer_insert(
    WyncCtx *ctx, WyncProp *prop, i32 tick, WyncState state);

void WyncStore_prop_state_buffer_insert_in_place(
    WyncCtx *ctx, WyncProp *prop, i32 tick, WyncState state);

i32 WyncStore_save_confirmed_state(
    WyncCtx *ctx, u32 prop_id, u32 tick, WyncState state);

void WyncStore_client_update_last_tick_received(WyncCtx *ctx, u32 tick);

void WyncStore_handle_pkt_prop_snap(WyncCtx *ctx, WyncPktSnap pkt);

i32 WyncStore_save_confirmed_state(
    WyncCtx *ctx, u32 prop_id, u32 tick, WyncState state);

void WyncStore_service_cleanup_dummy_props(WyncCtx *ctx);

i32 WyncStore_server_handle_pkt_inputs(
    WyncCtx *ctx, WyncPktInputs pkt, u16 from_nete_peer_id);

i32 WyncStore_client_handle_pkt_inputs(WyncCtx *ctx, WyncPktInputs pkt);

void WyncStore_prop_state_buffer_insert(
    WyncCtx *ctx, WyncProp *prop, i32 tick, WyncState state);

void WyncStore_prop_state_buffer_insert_in_place(
    WyncCtx *ctx, WyncProp *prop, i32 tick, WyncState state);

i32 WyncStore_insert_state_to_entity_prop(
    WyncCtx *ctx, u32 entity_id, const char *prop_name_id, u32 tick,
    WyncState state);

WyncState WyncState_prop_state_buffer_get(WyncProp *prop, i32 tick);

WyncState WyncState_prop_state_buffer_get_throughout(WyncProp *prop, u32 tick);

/// ---------------------------------------------------------------------------
/// WYNC THROTTLE
/// ---------------------------------------------------------------------------

void WyncThrottle__remove_entity_from_sync_queue(
    WyncCtx *ctx, u16 peer_id, u32 entity_id);

void WyncThrottle_system_fill_entity_sync_queue(WyncCtx *ctx);

void WyncThrottle_compute_entity_sync_order(WyncCtx *ctx);

i32 WyncThrottle_client_now_can_see_entity(
    WyncCtx *ctx, u16 client_id, u32 entity_id);

void WyncThrottle_everyone_now_can_see_entity(WyncCtx *ctx, u32 entity_id);

void WyncThrottle_entity_set_spawn_data(
    WyncCtx *ctx, u32 entity_id, u32 data_size, void *data);

i32 WyncThrottle_client_no_longer_sees_entity(
    WyncCtx *ctx, u16 client_id, u32 entity_id);

/// ---------------------------------------------------------------------------
/// WYNC TICK COLLECTION
/// ---------------------------------------------------------------------------

void WyncOffsetCollection_replace_value(
    CoTicks *co_ticks, i32 find_value, i32 new_value);

void WyncOffsetCollection_increase_value(CoTicks *co_ticks, i32 find_value);

bool WyncOffsetCollection_value_exists(CoTicks *co_ticks, i32 ar_value);

i32 WyncOffsetCollection_get_most_common(CoTicks *co_ticks);

i32 WyncOffsetCollection_get_less_common(CoTicks *co_ticks);

void WyncOffsetCollection_add_value(CoTicks *co_ticks, i32 new_value);

/// ---------------------------------------------------------------------------
/// WYNC TRACK
/// ---------------------------------------------------------------------------

i32 WyncTrack_track_entity(WyncCtx *ctx, u32 entity_id, u32 entity_type_id);

void WyncTrack_untrack_entity(WyncCtx *ctx, u32 entity_id);

void WyncTrack_delete_prop(WyncCtx *ctx, u32 prop_id);

bool WyncTrack_is_entity_tracked(WyncCtx *ctx, u32 entity_id);

i32 WyncTrack_get_new_prop_id(WyncCtx *ctx, u32 *out_prop_id);

i32 WyncTrack_prop_register_minimal(
    WyncCtx *ctx, u32 entity_id, const char *name_id,
    enum WYNC_PROP_TYPE data_type, u32 *out_prop_id);

WyncProp *WyncTrack_get_prop(WyncCtx *ctx, u32 prop_id);

WyncProp *WyncTrack_entity_get_prop(
    WyncCtx *ctx, u32 entity_id, const char *prop_name_id);

i32 WyncTrack_entity_get_prop_id(
    WyncCtx *ctx, u32 entity_id, const char *prop_name_id, u32 *out_prop_id);

/// Note: Better have a structure for direct access instead of searching
/// @param[out] out_entity_id Pointer to Instance
/// @returns error
i32 WyncTrack_prop_get_entity(WyncCtx *ctx, u32 prop_id, u32 *out_entity_id);

bool WyncTrack_is_entity_tracked(WyncCtx *ctx, u32 entity_id);

/// @returns Optional WyncProp
/// @retval NULL Not found / Not enabled
WyncProp *WyncTrack_get_prop(WyncCtx *ctx, u32 prop_id);

WyncProp *WyncTrack_get_prop_unsafe(WyncCtx *ctx, u32 prop_id);

i32 WyncTrack_prop_register_update_dummy(
    WyncCtx *ctx, u32 prop_id, u32 last_tick, u32 data_size, void *data);

i32 WyncTrack_wync_add_local_existing_entity(
    WyncCtx *ctx, u16 wync_client_id, u32 entity_id);

/// ---------------------------------------------------------------------------
/// WYNC XTRAP
/// ---------------------------------------------------------------------------

i32 WyncXtrap_preparation(WyncCtx *ctx);

void WyncXtrap_regular_entities_to_predict(WyncCtx *ctx, i32 tick);

void WyncXtrap_termination(WyncCtx *ctx);

bool WyncXtrap_is_entity_predicted(WyncCtx *ctx, u32 entity_id);

void WyncXtrap_tick_init(WyncCtx *ctx, i32 tick);

void WyncXtrap_props_update_predicted_states_data(
    WyncCtx *ctx, u32 *prop_ids, u32 prop_id_amount);

static void WyncXtrap_props_update_predicted_states_ticks(
    WyncCtx *ctx, u32 target_tick, u32 *prop_ids, u32 prop_id_amount);

void WyncXtrap_save_latest_predicted_state(WyncCtx *ctx, i32 tick);

void WyncXtrap_delta_props_clear_current_delta_events(WyncCtx *ctx);

static i32 WyncXtrap_entity_get_last_received_tick_from_pred_props(
    WyncCtx *ctx, u32 entity_id);

static void WyncXtrap_internal_tick_end(WyncCtx *ctx, i32 tick);

/// NOTE: assuming snap props always include all snaps for an entity
void WyncXtrap_update_entity_last_tick_received(WyncCtx *ctx, u32 prop_id);

void WyncXtrap_tick_end(WyncCtx *ctx, i32 tick);

/// ---------------------------------------------------------------------------
/// WYNC WRAPPER UTIL
/// ---------------------------------------------------------------------------

void WyncWrapper_initialize(WyncCtx *ctx);

void WyncWrapper_set_prop_callbacks(
    WyncCtx *ctx, u32 prop_id, WyncWrapper_UserCtx user_ctx,
    WyncWrapper_Getter getter, WyncWrapper_Setter setter);

void WyncWrapper_buffer_inputs(WyncCtx *ctx);

void WyncWrapper_extract_data_to_tick(WyncCtx *ctx, u32 save_on_tick);

void WyncWrapper_server_filter_prop_ids(WyncCtx *ctx);

void WyncWrapper_client_filter_prop_ids(WyncCtx *ctx);

#endif // !WYNC_PRIVATE_H
