#ifndef WYNC_FLOW_H
#define WYNC_FLOW_H

#include "wync/wync_input.h"
#include "wync/wync_lerp.h"
#include "wync/wync_stat.h"
#include "wync/wync_state_send.h"
#include "wync/wync_state_set.h"
#include "wync/wync_wrapper_util.h"
#include "wync_join.h"
#include "wync_spawn.h"
#include "wync_typedef.h"
#include "wync_throttle.h"
#include "wync_init.h"
#include "wync_clock.h"
#include "math.h"

// ==================================================
// PUBLIC API
// ==================================================

// High level functions related to logic cycles


void wync_flow_internal_setup_context(WyncCtx *ctx) {
	// Misc config

	ctx->max_tick_history_timewarp = (u32)pow(2, 7);

	// Both server and client

	//ctx->common = Wync_CoCommon.new()
	//ctx->wrapper = WyncWrapperStructs.WyncWrapperCtx.new()

	//ctx->co_track = WyncCtx.CoStateTrackingCommon.new()
	//ctx->co_events = WyncCtx.CoEvents.new()
	//ctx->co_clientauth = WyncCtx.CoClientAuthority.new()
	//ctx->co_metrics = WyncCtx.CoMetrics.new()
	//ctx->co_spawn = WyncCtx.CoSpawn.new()

	//// Server only

	//ctx->co_throttling = WyncCtx.CoThrottling.new()
	//ctx->co_filter_s = WyncCtx.CoFilterServer.new()

	//// Client only

	//ctx->co_ticks = WyncCtx.CoTicks.new()
	//ctx->co_pred = WyncCtx.CoPredictionData.new()
	//ctx->co_lerp = WyncCtx.CoLerp.new()
	//ctx->co_dummy = WyncCtx.CoDummyProps.new()
	//ctx->co_filter_c = WyncCtx.CoFilterClient.new()

	wync_init_ctx_common(ctx);
	wync_init_ctx_state_tracking(ctx);
	wync_init_ctx_events(ctx);
	wync_init_ctx_clientauth(ctx);
	wync_init_ctx_metrics(ctx);
	wync_init_ctx_spawn(ctx);

	wync_init_ctx_throttling(ctx);
	wync_init_ctx_filter_s(ctx);

	wync_init_ctx_ticks(ctx);
	wync_init_ctx_prediction_data(ctx);
	wync_init_ctx_lerp(ctx);
	wync_init_ctx_dummy(ctx);
	wync_init_ctx_filter_c(ctx);

	ctx->initialized = true;
}

// Calls all the systems that produce packets to send whilst respecting the data limit

void wync_flow_internal_wync_system_gather_packets_start(WyncCtx *ctx) {
	if (ctx->common.is_client) {
		if (!ctx->common.connected) {
			WyncJoin_service_wync_try_to_connect(ctx);  // reliable, commited
		} else {
			WyncClock_client_ask_for_clock(ctx);   // unreliable
			//WyncDeltaSyncUtilsInternal.wync_system_client_send_delta_prop_acks(ctx) // unreliable
			WyncSend_client_send_inputs(ctx); // unreliable
			//WyncEventUtils_send_event_data(ctx)   // reliable, commited
		}
	} else {
		WyncSpawn_system_send_entities_to_despawn(ctx); // reliable, commited
		WyncSpawn_system_send_entities_to_spawn(ctx);   // reliable, commited
		WyncInput_system_sync_client_ownership(ctx);    // reliable, commited

		WyncThrottle_system_fill_entity_sync_queue(ctx);
		WyncThrottle_compute_entity_sync_order(ctx);
		WyncSend_extracted_data(ctx); // both reliable/unreliable
	}
	//WyncStats.wync_system_calculate_data_per_tick(ctx)
}

void wync_flow_internal_wync_system_gather_packets_end(WyncCtx *ctx) {
	// pending delta props fullsnapshots should be extracted by now
	//WyncStateSend.wync_send_pending_rela_props_fullsnapshot(ctx)
	WyncSend_queue_out_snapshots_for_delivery(ctx); // both reliable/unreliable
}

/// param wync_pkt User must free it manually
///
i32 wync_flow_wync_feed_packet(
	WyncCtx *ctx, 
	WyncPacket wync_pkt,
	u16 from_nete_peer_id
) {
	// debug statistics
	//WyncDebug.log_packet_received(ctx, wync_pkt.packet_type_id)
	bool is_client = ctx->common.is_client, is_server = !is_client;

	// tick rate calculation	
	if (is_client) {
		//WyncStats._wync_report_update_received(ctx)
	}

	static NeteBuffer buffer = { 0 };
	buffer.cursor_byte = 0;
	buffer.size_bytes = wync_pkt.data.data_size;
	buffer.data = wync_pkt.data.data;

	LOG_OUT_C(ctx, "Received PKT %s", PKT_NAMES[wync_pkt.packet_type_id]);

	switch (wync_pkt.packet_type_id) {
		case WYNC_PKT_JOIN_REQ:
		{
			if (is_server) {
				WyncPktJoinReq pkt = { 0 };
				if (WyncPktJoinReq_serialize (true, &buffer, &pkt)) {
					WyncJoin_handle_pkt_join_req(ctx, pkt, from_nete_peer_id);
				}
			}
			break;
		}
		case WYNC_PKT_JOIN_RES:
		{
			if (is_client) {
				WyncPktJoinRes pkt = { 0 };
				if (WyncPktJoinRes_serialize (true, &buffer, &pkt)) {
					WyncJoin_handle_pkt_join_res(ctx, pkt);
				}
			}
			break;
		}
		case WYNC_PKT_EVENT_DATA:
		{
			//WyncEventUtils.wync_handle_pkt_event_data(ctx, wync_pkt.data)
			break;
		}
		case WYNC_PKT_INPUTS:
		{
			WyncPktInputs pkt = { 0 };
			if (!WyncPktInputs_serialize(true, &buffer, &pkt)) {
				WyncPktInputs_free(&pkt);
				break;
			}
			if (is_client) {
				WyncStore_client_handle_pkt_inputs(ctx, pkt);
			} else {
				WyncStore_server_handle_pkt_inputs(ctx, pkt, from_nete_peer_id);
			}
			WyncPktInputs_free(&pkt);
			break;
		}
		case WYNC_PKT_PROP_SNAP:
		{
			if (is_client) {
				WyncPktSnap pkt = { 0 };
				if (!WyncPktSnap_serialize(true, &buffer, &pkt)) {
					LOG_ERR_C(ctx, "couldn't deserialize Snap");
					WyncPktSnap_free(&pkt);
					break;
				}
				WyncStore_handle_pkt_prop_snap(ctx, pkt);
				WyncPktSnap_free(&pkt);
			}
			break;
		}
		case WYNC_PKT_RES_CLIENT_INFO:
		{
			if (is_client) {
				WyncPktResClientInfo pkt = { 0 };
				if (!WyncPktResClientInfo_serialize(true, &buffer, &pkt)) break;
				WyncJoin_handle_pkt_res_client_info(ctx, pkt);
			}
			break;
		}
		case WYNC_PKT_CLOCK:
		{
			WyncPktClock pkt = { 0 };
			if (!WyncPktClock_serialize(true, &buffer, &pkt)) {
				LOG_ERR_C(ctx, "Coulnd't read PKT_CLOCK");
				break;
			}
			if (is_client) {
				WyncClock_client_handle_pkt_clock(ctx, pkt);
			} else {
				WyncClock_server_handle_pkt_clock_req(ctx, pkt, from_nete_peer_id);
			}
			break;
		}
		case WYNC_PKT_CLIENT_SET_LERP_MS:
		{
			if (is_server) {
				WyncPktClientSetLerpMS pkt = { 0 };
				if (!WyncPktClientSetLerpMS_serialize(true, &buffer, &pkt)) {
					LOG_ERR_C(ctx, "couldn't read WyncPktClientSetLerpMS");
					break;
				}
				WyncLerp_handle_packet_client_set_lerp_ms(
					ctx, pkt, from_nete_peer_id);
			}
			break;
		}
		case WYNC_PKT_SPAWN:
		{
			if (is_client) {
				WyncPktSpawn pkt = { 0 };
				if (!WyncPktSpawn_serialize(true, &buffer, &pkt, 0)) {
					WyncPktSpawn_free(&pkt);
					LOG_WAR_C(ctx, "flow, couldn't deserialize packet");
					break;
				}
				LOG_OUT_C(ctx, "spawn, received pcket");
				//Log.outc(ctx, "spawn, spawn pkt %s" % [(wync_pkt.data as WyncPktSpawn).entity_ids])
				WyncSpawn_handle_pkt_spawn(ctx, pkt);
				WyncPktSpawn_free(&pkt);
			}
			break;
		}
		case WYNC_PKT_DESPAWN:
		{
			if (is_client) {
				WyncPktDespawn pkt = { 0 };
				if (!WyncPktDespawn_serialize(true, &buffer, &pkt)) {
					WyncPktDespawn_free(&pkt);
					break;
				}
				//Log.outc(ctx, "spawn, despawn pkt %s" % [(wync_pkt.data as WyncPktDespawn).entity_ids])
				WyncSpawn_handle_pkt_despawn(ctx, pkt);
				WyncPktDespawn_free(&pkt);
			}
			break;
		}
		case WYNC_PKT_DELTA_PROP_ACK:
		{
			if (is_server) {
				//WyncDeltaSyncUtilsInternal.wync_handle_pkt_delta_prop_ack(ctx, wync_pkt.data, from_nete_peer_id)
			}
			break;
		}
		default:
			//Log.errc(ctx, "wync packet_type_id(%s) not recognized skipping (%s)" % [wync_pkt.packet_type_id, wync_pkt.data])
			return -1;
			break;
	}

	return OK;
}


// Setup functions
// ================================================================
// TODO: Modularize this, make different versions for server/client


i32 wync_flow_server_setup(WyncCtx *ctx) {
	u16 max_peers = ctx->common.max_peers;

	// peer id 0 reserved for server
	ctx->common.is_client = false;
	ctx->common.my_peer_id = 0;
	ctx->common.connected = true;
	ctx->common.peers = i32_DynArr_create();
	i32_DynArr_insert(&ctx->common.peers, -1);

	// setup event caching
	ctx->co_events.events_hash_to_id = u32_FIFOMap_init_calloc(
		ctx->common.max_amount_cache_events);
	ctx->co_events.to_peers_i_sent_events =
		calloc(sizeof(u32_FIFOMap), max_peers);
	for (u16 i = 0; i < max_peers; ++i) {
		ctx->co_events.to_peers_i_sent_events[i] =
			u32_FIFOMap_init_calloc(ctx->common.max_amount_cache_events);
	}

	// setup relative synchronization
	ctx->co_throttling.peers_events_to_sync =
		calloc(sizeof(ConMap), max_peers);
	for (u16 i = 0; i < max_peers; ++i) {
		ConMap_init(&ctx->co_throttling.peers_events_to_sync[i]);
	}

	// setup peer channels
	//WyncEventUtils.setup_peer_global_events(ctx, WyncCtx.SERVER_PEER_ID)
	//for i in range(1, ctx.common.max_peers):
		//WyncEventUtils.setup_peer_global_events(ctx, i)

	// setup prob prop
	WyncStat_setup_prob_for_entity_update_delay_ticks(ctx, SERVER_PEER_ID);

	return OK;
}


// ==================================================
// WRAPPER
// ==================================================

void wync_flow_setup_context(WyncCtx *ctx) {
	wync_flow_internal_setup_context(ctx);
	WyncWrapper_initialize(ctx);
}


// Note. Before running this, make sure to receive packets from the network

void wync_flow_wync_server_tick_start(WyncCtx *ctx) {

	WyncClock_advance_ticks(ctx);

	//WyncActions.module_events_consumed_advance_tick(ctx)

	// player inputs
	WyncState_reset_all_state_to_confirmed_tick_absolute (
		ctx,
		ctx->co_filter_s.filtered_clients_input_and_event_prop_ids.items,
		(u32)ctx->co_filter_s.filtered_clients_input_and_event_prop_ids.size,
		ctx->common.ticks
	);

	//WyncDeltaSyncUtilsInternal.delta_props_clear_current_delta_events(ctx)
}


void wync_flow_wync_server_tick_end(WyncCtx *ctx) {
	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	for (u16 peer_id = 1; peer_id < peer_amount; ++peer_id) {
		WyncClock_system_stabilize_latency(ctx, &ctx->common.peer_latency_info[peer_id]);
	}

	WyncWrapper_server_filter_prop_ids(ctx);

	//WyncStateSend.system_update_delta_base_state_tick(ctx)

	// NOTE: maybe a way to extract data but only events, since that is unskippable?
	// (shouldn't be throttled)
	// This function extracts regular props, plus _auxiliar delta event props_
	// We need a function to extract data exclusively of events... Like the equivalent
	// of the client's _input_bufferer_
	WyncWrapper_extract_data_to_tick(ctx, ctx->common.ticks); // wrapper function
}

void wync_flow_wync_client_tick_end(WyncCtx *ctx) {

	WyncWrapper_client_filter_prop_ids(ctx);
	WyncClock_advance_ticks(ctx);
	WyncClock_system_stabilize_latency(ctx, &ctx->common.peer_latency_info[SERVER_PEER_ID]);
	WyncClock_update_prediction_ticks(ctx);
	
	WyncWrapper_buffer_inputs(ctx);

	// CANNOT reset events BEFORE polling inputs, WHERE do we put this?
	
	//WyncDeltaSyncUtilsInternal.delta_props_clear_current_delta_events(ctx)
	//WyncDeltaSyncUtils.predicted_event_props_clear_events(ctx)

	WyncState_reset_props_to_latest_value(ctx);
	
	// NOTE: Maybe this one should be called AFTER consuming packets, and BEFORE xtrap
	WyncStat_system_calculate_prob_prop_rate(ctx);

	//WyncStats.wync_system_calculate_server_tick_rate(ctx)

	WyncStore_service_cleanup_dummy_props(ctx);

	WyncLerp_precompute(ctx);
}

void wync_flow_wync_system_gather_packets(WyncCtx *ctx) {
	wync_flow_internal_wync_system_gather_packets_start(ctx);
	if (!ctx->common.is_client) {
		//WyncWrapper.extract_rela_prop_fullsnapshot_to_tick(ctx, ctx.common.ticks);
	}
	wync_flow_internal_wync_system_gather_packets_end(ctx);
}


#endif // !WYNC_FLOW_H
