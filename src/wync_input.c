#include "containers/map.h"
#include "wync_private.h"

/// @param[out] out_peer_id If found
/// @returns error
i32 WyncInput_prop_get_peer_owner(WyncCtx *ctx, u32 prop_id, u32 *out_peer_id){

	// client only has info about itself
	if (ctx->common.is_client) {
		if (ctx->common.my_peer_id >= 0
			&& ctx->common.my_peer_id < ctx->common.max_peers
			&& ConMap_has_key(
			&ctx->co_clientauth.client_owns_prop[ctx->common.my_peer_id],
			prop_id)
		) {
			*out_peer_id = ctx->common.my_peer_id;
			return OK;
		}
		return -1;
	}

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	for (u16 peer_id = 0; peer_id < peer_amount; ++peer_id) {
		if (ConMap_has_key(
				&ctx->co_clientauth.client_owns_prop[peer_id], prop_id))
		{
			*out_peer_id = peer_id;
			return OK;
		}
	}

	return -1;
}

/// @returns error
i32 WyncInput_prop_set_client_owner_internal(
	WyncCtx *ctx, u32 prop_id, u16 wync_peer_id
){
	ConMap_set_pair(
		&ctx->co_clientauth.client_owns_prop[wync_peer_id], prop_id, true);
	ctx->co_clientauth.client_ownership_updated = true;

	return OK;
}


/// @retuns error
i32 WyncInput_prop_set_client_owner(
	WyncCtx *ctx, u32 prop_id, u16 nete_client_id
){
	uint16_t wync_peer_id = 0;
	if (WyncJoin_is_peer_registered(ctx, nete_client_id, &wync_peer_id) != OK){
		LOG_ERR_C(ctx, "client %hu is not registered", nete_client_id);
		return -1;
	}
	return WyncInput_prop_set_client_owner_internal(ctx, prop_id, wync_peer_id);
}

/// Server only, sends packet
///
void WyncInput_system_sync_client_ownership(WyncCtx *ctx) {
	if (!ctx->co_clientauth.client_ownership_updated) return;
	ctx->co_clientauth.client_ownership_updated = false;

	// remind all clients about their prop ownership
	
	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	for (u16 wync_peer_id = 1; wync_peer_id < peer_amount; ++wync_peer_id)
	{
		ConMap *owns_props =
			&ctx->co_clientauth.client_owns_prop[wync_peer_id];

		ConMapIterator it = { 0 };
		while (ConMap_iterator_get_next_key(owns_props, &it) == OK) {

			u32 prop_id = it.key;
			WyncPktResClientInfo packet = {
				.prop_id = prop_id,
				.peer_id = wync_peer_id
			};

			WyncPacket_wrap_and_queue(
				ctx,
				WYNC_PKT_RES_CLIENT_INFO,
				&packet,
				wync_peer_id,
				RELIABLE,
				true
			);
		}
	}
}
