#include "wync_private.h"

bool WyncJoin_is_connected(WyncCtx *ctx){
	return ctx->common.connected;
}

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

i32 WyncJoin_get_my_wync_peer_id(WyncCtx *ctx) {
	return ctx->common.my_peer_id;
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

	WyncThrottle_client_now_can_see_entity(
	ctx, peer_id, ENTITY_ID_PROB_FOR_ENTITY_UPDATE_DELAY_TICKS);
	WyncTrack_wync_add_local_existing_entity(
	ctx, peer_id, ENTITY_ID_PROB_FOR_ENTITY_UPDATE_DELAY_TICKS);

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

	WyncPktJoinReq packet_join = { 0xDEADBEEF };

	WyncPacket_wrap_and_queue(
		ctx,
		WYNC_PKT_JOIN_REQ,
		&packet_join,
		SERVER_PEER_ID,
		RELIABLE,
		true
	);

	// TODO: Set lerpms could be moved somewhere else, could be sent anytime

	WyncPktClientSetLerpMS packet_lerp = { 0 };
	packet_lerp.lerp_ms = ctx->co_lerp.lerp_ms;

	WyncPacket_wrap_and_queue(
		ctx,
		WYNC_PKT_CLIENT_SET_LERP_MS,
		&packet_lerp,
		SERVER_PEER_ID,
		RELIABLE,
		true
	);

	return error;
}


// packet consuming -----------------------------------------------------------


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
		(u32_FIFOMap*) calloc(sizeof(u32_FIFOMap), max_peers);

	for (u16 i = 0; i < max_peers; ++i) {
		ctx->co_events.to_peers_i_sent_events[i] =
			u32_FIFOMap_init_calloc(ctx->common.max_amount_cache_events);
	}

	// setup relative synchronization

	ctx->co_throttling.peers_events_to_sync =
		(ConMap*) calloc(sizeof(ConMap), max_peers);
	for (u16 i = 0; i < max_peers; ++i) {
		ConMap_init(&ctx->co_throttling.peers_events_to_sync[i]);
	}


	// predict my own global events
	/*uint channel_id = 0;*/
	/*uint channel_prop_id =*/
		/*ctx->co_events.prop_id_by_peer_by_channel[peer_id][channel_id];*/
	/*WyncProp_enable_prediction(ctx, channel_prop_id);*/

	// TODO
	// setup peer channels
	//WyncEventUtils_setup_peer_global_events

	return OK;
}


/// @returns error
i32 WyncJoin_handle_pkt_join_res (WyncCtx *ctx, WyncPktJoinRes pkt) {
	if (ctx->common.connected) {
		return OK;
	}
	if (!pkt.approved) {
		LOG_ERR_C(ctx, "Connection DENIED for client(%d) (me)", ctx->common.my_nete_peer_id);
		return -1;
	}

	// setup client stufff
	// NOTE: Move this elsewhere?

	ctx->common.connected = true;
	ctx->common.my_peer_id = pkt.wync_client_id;
	WyncJoin_client_setup_my_client(ctx, pkt.wync_client_id);

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
	// NOTE: the criteria to determine wether a client has a valid prop
	// ownership could be user defined
	// NOTE: wync setup must be ran only once per client

	u16 wync_client_id;
	i32 err;
	err = WyncJoin_is_peer_registered(ctx, from_nete_peer_id, &wync_client_id);

	if (err == OK) {
		LOG_OUT_C(ctx, "Client %hu is already registered in as %hu",
			from_nete_peer_id, wync_client_id);
		return -1;
	}
	wync_client_id = WyncJoin_peer_register(ctx, from_nete_peer_id);

	// send confirmation

	WyncPktJoinRes packet_res = { 0 };

	packet_res.approved = true;
	packet_res.wync_client_id = wync_client_id;

	WyncPacket_wrap_and_queue(
		ctx,
		WYNC_PKT_JOIN_RES,
		&packet_res,
		wync_client_id,
		RELIABLE,
		true
	);

	// let client own it's own global events
	// Note: Maybe move this where all channel are defined
	// WARN: TODO: REFACTOR

	u32 prop_id;
	u32 global_events_entity_id = ENTITY_ID_GLOBAL_EVENTS + wync_client_id;
	i32 query_err = WyncTrack_entity_get_prop_id(
		ctx, global_events_entity_id, "channel_0", &prop_id);
	if (query_err == OK) {
		WyncInput_prop_set_client_owner(ctx, prop_id, wync_client_id);
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
	WyncInput_prop_set_client_owner(ctx, pkt.prop_id, pkt.peer_id);

	LOG_OUT_C(ctx, "Prop %hu ownership given to client %hu", pkt.prop_id, pkt.peer_id);

	// trigger refilter
	ctx->common.was_any_prop_added_deleted = true;
}

bool WyncJoin_out_client_just_connected_to_server (WyncCtx *ctx) {
	bool just_connected = ctx->common.connected && !ctx->common.prev_connected;
	ctx->common.prev_connected = ctx->common.connected;
	return just_connected;
}

void WyncJoin_pending_peers_setup_iteration(WyncCtx *ctx) {
	ctx->co_throttling.pending_peers_it = (u32_DynArrIterator) { 0 };
}

/// @returns Net Peer ID which is pending to setup
/// @retval -1 End reached, no more peers
int32_t WyncJoin_pending_peers_get_next(WyncCtx *ctx) {
	i32 err = u32_DynArr_iterator_get_next(
		&ctx->co_throttling.out_peer_pending_to_setup,
		&ctx->co_throttling.pending_peers_it
	);
	if (err != OK) {
		return -1;
	}
	return (i32)(*ctx->co_throttling.pending_peers_it.item);
}

void WyncJoin_pending_peers_clear (WyncCtx *ctx) {
	u32_DynArr_clear_preserving_capacity(&ctx->co_throttling.out_peer_pending_to_setup);
}

void WyncJoin_active_peers_setup_iteration(WyncCtx *ctx) {
	ctx->common.active_peers_it = (i32_DynArrIterator) { 0 };
}

/// @returns Wync Peer ID that are active
/// @retval -1 End reached, no more peers
i32 WyncJoin_active_peers_get_next(WyncCtx *ctx, WyncPeer_ids *out_peer_ids)
{
	i32 err = i32_DynArr_iterator_get_next(
		&ctx->common.peers,
		&ctx->common.active_peers_it
	);
	if (err != OK) {
		return -1;
	}

	out_peer_ids->wync_peer_id = ctx->common.active_peers_it.index;
	out_peer_ids->network_peer_id = *ctx->common.active_peers_it.item;
	return OK;
}


