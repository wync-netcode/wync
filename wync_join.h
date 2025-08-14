#ifndef WYNC_JOIN_H
#define WYNC_JOIN_H

#include "wync_event_utils.h"
#include "wync_typedef.h"
#include "wync_packets.h"
#include "wync_packet_util.h"
#include "wync_track.h"


/// @param[out] out_peer_id If found
/// @returns error
i32 WyncJoin_is_peer_registered (WyncCtx *ctx, u16 nete_peer_id, u16 *out_wync_peer_id) {
	size_t peer_amount = i32_DynArr_get_size(&ctx->common.peers);
	for (size_t peer_id = 0; peer_id < peer_amount; ++peer_id) {
		i32 here_nete_peer_id = *i32_DynArr_get(&ctx->common.peers, peer_id);
		if (here_nete_peer_id == (i32)nete_peer_id)
		{
			*out_wync_peer_id = peer_id;
			return OK;
		}
	}
	return -1;
}


/// Gets nete_peer_id from a given wync_peer_id
/// Used to know to whom to send packets
///
/// @param[out] out_nete_peer_id
/// @returns error
i32 WyncJoin_get_nete_peer_id_from_wync_peer_id (
	WyncCtx *ctx,
	u16 wync_peer_id,
	i32 *out_nete_peer_id
) {
	size_t peers_amount = i32_DynArr_get_size(&ctx->common.peers);

	if (wync_peer_id >= 0 && wync_peer_id < peers_amount) {
		*out_nete_peer_id = *i32_DynArr_get(&ctx->common.peers, wync_peer_id);
		return OK;
	}
	return -1;
}

/// @param[out] out_wync_peer_id
/// @returns error
i32 WyncJoin_get_wync_peer_id_from_nete_peer_id (
	WyncCtx *ctx,
	u16 nete_peer_id,
	u16 *out_wync_peer_id
) {
	return WyncJoin_is_peer_registered(ctx, nete_peer_id, out_wync_peer_id);
}


void WyncJoin_set_my_nete_peer_id (WyncCtx *ctx, u16 nete_peer_id){
	ctx->common.my_nete_peer_id = nete_peer_id;
}

/// Client only
///
void WyncJoin_set_server_nete_peer_id (WyncCtx *ctx, u16 nete_peer_id){
	u32 peer_amount = i32_DynArr_get_size(&ctx->common.peers);
	if (peer_amount == 0) {
		i32_DynArr_insert(&ctx->common.peers, nete_peer_id);
	}
	i32_DynArr_insert_at(&ctx->common.peers, 0, nete_peer_id);
	ctx->common.my_nete_peer_id = nete_peer_id;
}


/// NOTE: What if already registered?
///
/// @param   nete_peer_id Usually the transport's peer_id
/// @returns peer_id
u16 WyncJoin_peer_register (
	WyncCtx *ctx,
	i32 nete_peer_id
) {

	size_t peer_id = i32_DynArr_get_size(&ctx->common.peers);
	i32_DynArr_insert(&ctx->common.peers, nete_peer_id);

	ConMap client_owns_props = { 0 };
	ConMap_init(&client_owns_props);
	ctx->co_clientauth.client_owns_prop[peer_id] = client_owns_props;

	ConMap prop_last_tick = { 0 };
	ConMap_init(&prop_last_tick);
	ctx->co_track.client_has_relative_prop_has_last_tick[peer_id] =
		prop_last_tick;

	Wync_ClientInfo client_info = { 0 };
	ctx->common.client_has_info[peer_id] = client_info;

	return peer_id;
}


/// @returns error
i32 WyncJoin_service_wync_try_to_connect(WyncCtx *ctx) {
	if (ctx->common.connected) { return OK; }
	
	// throttle
	if (ctx->common.ticks % 8 != 0) { return OK; }

	// try get server nete_peer_id
	i32 server_nete_peer_id = -1;
	i32 error = WyncJoin_get_nete_peer_id_from_wync_peer_id
		(ctx, SERVER_PEER_ID, &server_nete_peer_id);
	if (error != OK) {
		return -1;
	}

	// send connect req packet
	// TODO: Move this elsewhere

	WyncPacketOut packet_out = { 0 };
	WyncPktJoinReq packet_join = { 0xDEADBEEF };

	error = WyncPacket_wrap_packet_out (
		ctx,
		SERVER_PEER_ID,
		WYNC_PKT_JOIN_REQ,
		sizeof(packet_join),
		&packet_join,
		&packet_out
	);
	if (error == OK) {
		WyncPacket_try_to_queue_out_packet(
			ctx,
			packet_out,
			RELIABLE, true, false
		);
	}

	// TODO: Set lerpms could be moved somewhere else, could be sent anytime

	packet_out = (WyncPacketOut) { 0 };
	WyncPktClientSetLerpMS packet_lerp = { 0 };
	packet_lerp.lerp_ms = ctx->co_lerp.lerp_ms;

	i32 lerp_error = WyncPacket_wrap_packet_out (
		ctx,
		SERVER_PEER_ID,
		WYNC_PKT_CLIENT_SET_LERP_MS,
		sizeof(packet_lerp),
		&packet_lerp,
		&packet_out
	);
	if (lerp_error == OK) {
		WyncPacket_try_to_queue_out_packet(
			ctx,
			packet_out,
			RELIABLE, true, false
		);
	}

	return error;
}


// packet consuming -----------------------------------------------------------


/// @returns error
i32 WyncJoin_handle_pkt_join_res (WyncCtx *ctx, WyncPktJoinRes pkt) {
	if (!pkt.approved) {
		LOG_ERR_C(ctx, "Connection DENIED for client(%d) (me)", ctx->common.my_nete_peer_id);
		return -1;
	}

	// setup client stufff
	// NOTE: Move this elsewhere?

	ctx->common.connected = true;
	ctx->common.my_peer_id = pkt.wync_client_id;
	//WyncJoin_client_setup_my_client(ctx, data.wync_client_id);

	LOG_OUT_C(ctx, "client nete_peer_id(%d) connected as wync_peer_id(%d)",
		ctx->common.my_nete_peer_id, ctx->common.my_peer_id);
	return OK;
}


/// @returns error
i32 WyncJoin_handle_pkt_join_req (
	WyncCtx *ctx,
	WyncPktJoinReq pkt,
	u16 from_nete_peer_id
) {
	// NOTE: the criteria to determine wether a client has a valid prop ownership could be user defined
	// NOTE: wync setup must be ran only once per client

	u16 wync_client_id;
	i32 err;
	err = WyncJoin_is_peer_registered(ctx, from_nete_peer_id, &wync_client_id);
	if (err == OK) {
		LOG_OUT_C(ctx, "Client %hu already setup in Wync as %hu",
			from_nete_peer_id, wync_client_id);
		return -1;
	}
	wync_client_id = WyncJoin_peer_register(ctx, from_nete_peer_id);

	// send confirmation

	WyncPktJoinRes packet_res = { 0 };
	WyncPacketOut packet_out = { 0 };

	packet_res.approved = true;
	packet_res.wync_client_id = wync_client_id;

	err = WyncPacket_wrap_packet_out (
		ctx,
		wync_client_id,
		WYNC_PKT_JOIN_RES,
		sizeof(packet_res),
		&packet_res,
		&packet_out
	);
	if (err == OK) {
		WyncPacket_try_to_queue_out_packet(
			ctx,
			packet_out,
			RELIABLE, true, false
		);
	}

	// let client own it's own global events
	// Note: Maybe move this where all channel are defined
	// WARN: TODO: REFACTOR
	u32 prop_id;
	u32 global_events_entity_id = ENTITY_ID_GLOBAL_EVENTS + wync_client_id;
	i32 query_err = WyncTrack_entity_get_prop_id(
		ctx, global_events_entity_id, "channel_0", &prop_id);
	if (query_err == OK) {
		//WyncInput_prop_set_client_owner(ctx, prop_id, wync_client_id);
	}

	// queue as pending for setup

	u32_DynArr_insert(
		&ctx->co_throttling.out_peer_pending_to_setup, from_nete_peer_id);

	return err;
}

/// @returns error
void WyncJoin_handle_pkt_res_client_info (
	WyncCtx *ctx,
	WyncPktResClientInfo pkt
) {
	// set prop ownership
	//WyncInput_prop_set_client_owner(ctx, data.prop_id, data.peer_id);
	LOG_OUT_C(ctx, "Prop %hu ownership given to client %hu", pkt.prop_id, pkt.peer_id);

	// trigger refilter
	ctx->common.was_any_prop_added_deleted = true;

}

/// Client only
///
/// @returns error
i32 WyncJoin_client_setup_my_client (
	WyncCtx *ctx,
	u16 peer_id
) {
	ctx->common.my_peer_id = peer_id;

	// NOTE: Tests multiple clients rejoining and ensure data integrity
	//ConMap* client_owns_prop = &ctx->co_clientauth.client_owns_prop[peer_id];

	// setup event caching

	u16 max_peers = ctx->common.max_peers;
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

	// TODO
	// setup peer channels
	//WyncEventUtils_setup_peer_global_events
	// setup prob prop
	//WyncStats.setup_entity_prob_for_entity_update_delay_ticks(ctx, ctx.common.my_peer_id)

	return OK;
}

void WyncJoin_clear_peers_pending_to_setup (WyncCtx *ctx) {
	u32_DynArr_clear_preserving_capacity(&ctx->co_throttling.out_peer_pending_to_setup);
}

bool WyncJoin_out_client_just_connected_to_server (WyncCtx *ctx) {
	bool just_connected = ctx->common.connected && !ctx->common.prev_connected;
	ctx->common.prev_connected = ctx->common.connected;
	return just_connected;
}

#endif // !WYNC_JOIN_H
