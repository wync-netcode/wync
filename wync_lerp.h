#ifndef WYNC_LERP
#define WYNC_LERP

#include "raylib.h"
#include "wync/wync_clock.h"
#include "wync/wync_state_store.h"
#include "wync/wync_wrapper.h"
#include "wync_typedef.h"


struct LerpTicks {
	i32 lhs_tick_server;
	i32 rhs_tick_server;
	i32 lhs_tick_local;
	i32 rhs_tick_local;
};


// WARNING! Here we asume `prop.last_ticks_received` is sorted
static struct LerpTicks WyncLerp_find_closest_two_snapshots_from_prop (
	WyncCtx *ctx,
	WyncProp *prop,
	float target_time_ms
) {
	// Note: maybe we don't need local ticks here
	bool done_selecting_right = false;
	i32 rhs_tick_server = -1;
	i32 rhs_tick_server_prev = -1;
	i32 rhs_tick_local = -1;
	i32 rhs_tick_local_prev = -1;
	i32 lhs_tick_server = -1;
	i32 lhs_tick_local = -1;
	i32 lhs_timestamp = -1;
	i32 size = prop->statebff.last_ticks_received.size;
	i32 server_tick = 0;
	i32 server_tick_prev = 0;

	for (i32 i = 0; i < size; ++i) {
		server_tick_prev = server_tick;
		server_tick = 
			*i32_RinBuf_get_absolute(&prop->statebff.last_ticks_received, size -1 -i);

		if (server_tick == -1) {
			if ((lhs_tick_server == -1 || lhs_tick_server >= rhs_tick_server)
				&& rhs_tick_server_prev != -1)
			{
				lhs_tick_server = rhs_tick_server;
				lhs_tick_local = rhs_tick_local;
				rhs_tick_server = rhs_tick_server_prev;
				rhs_tick_local = rhs_tick_local_prev;
			}
			else {
				continue;
			}
		}
		else if (server_tick == server_tick_prev) {
			continue;
		}

		// This check is necessary because of the current strategy where we sort
		// last_ticks_received causing newer received ticks (albeit older
		// numerically) to overlive older received ticks with higher number
		WyncState data =
			WyncState_prop_state_buffer_get_throughout(prop, server_tick);
		if (data.data_size == 0 || data.data == NULL) {
			continue;
		}

		// calculate local tick from server tick
		i32 local_tick = server_tick - ctx->co_ticks.server_tick_offset;
		float snapshot_timestamp = WyncClock_get_tick_timestamp_ms(ctx, local_tick);

		if (!done_selecting_right) {
			if (snapshot_timestamp > target_time_ms) {
				rhs_tick_server_prev = rhs_tick_server;
				rhs_tick_local_prev = rhs_tick_local;
				rhs_tick_server = server_tick;
				rhs_tick_local = local_tick;
			} else {
				done_selecting_right = true;
				if (rhs_tick_server == -1) {
					rhs_tick_server = server_tick;
				}
			}
		}

		if (snapshot_timestamp > lhs_timestamp
			|| lhs_tick_server == -1
			|| lhs_tick_server >= rhs_tick_server
		) {
			lhs_tick_server = server_tick;
			lhs_tick_local = local_tick;
			lhs_timestamp = snapshot_timestamp;
			// TODO: End prematurely when both sides are found
		}
	}

	if ((lhs_tick_server == -1 || lhs_tick_server >= rhs_tick_server)
		&& rhs_tick_server_prev != -1
	) {
		lhs_tick_server = rhs_tick_server;
		lhs_tick_local = rhs_tick_local;
		rhs_tick_server = rhs_tick_server_prev;
		rhs_tick_local = rhs_tick_local_prev;
	}

	if (lhs_tick_server == -1 || rhs_tick_server == -1) {
		return (struct LerpTicks) { -1, 0, 0, 0 };
	}

	return (struct LerpTicks) {
		lhs_tick_server,
		rhs_tick_server,
		lhs_tick_local,
		rhs_tick_local
	};
}


static void WyncLerp_precompute_confirmed_states (
	WyncCtx *ctx,
	WyncProp *prop,
	float target_time_ms
) {
	struct LerpTicks snaps = WyncLerp_find_closest_two_snapshots_from_prop(
		ctx, prop, target_time_ms);
	if (snaps.lhs_tick_server == -1) {
		return;
	}

	if (prop->co_lerp.lerp_left_canon_tick == snaps.lhs_tick_server
		&& prop->co_lerp.lerp_right_canon_tick == snaps.rhs_tick_server
	) {
		return;
	}

	prop->co_lerp.lerp_ready = false;
	prop->co_lerp.lerp_left_canon_tick = snaps.lhs_tick_server;
	prop->co_lerp.lerp_right_canon_tick = snaps.rhs_tick_server;
	prop->co_lerp.lerp_left_local_tick =
		prop->co_lerp.lerp_left_canon_tick - ctx->co_ticks.server_tick_offset;
	prop->co_lerp.lerp_right_local_tick =
		prop->co_lerp.lerp_right_canon_tick - ctx->co_ticks.server_tick_offset;
	// Note: ^^^ for more consistent lerp we use equivalent _local tick_ from
	// _canon_ intead of using real _local tick_

	// Note: Might want to limit how much it grows
	// TODO: Move this elsewhere
	ctx->co_ticks.last_tick_rendered_left = MAX(
		ctx->co_ticks.last_tick_rendered_left,
		prop->co_lerp.lerp_left_canon_tick);

	WyncState val_left = WyncState_prop_state_buffer_get_throughout(
		prop, prop->co_lerp.lerp_left_canon_tick);
	WyncState val_right = WyncState_prop_state_buffer_get_throughout(
		prop, prop->co_lerp.lerp_right_canon_tick);

	if (val_left.data_size == 0 || val_left.data == NULL 
		|| val_right.data_size == 0 || val_right.data == NULL
	) {
		return;
	}

	prop->co_lerp.lerp_use_confirmed_state = true;
	prop->co_lerp.lerp_ready = true;

	// Overwrite or otherwise Allocate state
	WyncState_set_from_buffer(
		&prop->co_lerp.lerp_left_state, val_left.data_size, val_left.data);
	WyncState_set_from_buffer(
		&prop->co_lerp.lerp_right_state, val_left.data_size, val_right.data);
}


static void WyncLerp_precompute_predicted (
	WyncCtx *ctx,
	WyncProp *prop
) {
	if (prop->co_xtrap.pred_prev.data.data_size == 0 
		|| prop->co_xtrap.pred_curr.data.data_size == 0 
		|| prop->co_xtrap.pred_prev.data.data == NULL
		|| prop->co_xtrap.pred_curr.data.data == NULL
	){
		prop->co_lerp.lerp_ready = false;
	}

	prop->co_lerp.lerp_left_local_tick = ctx->common.ticks;
	prop->co_lerp.lerp_right_local_tick = ctx->common.ticks +1;
	prop->co_lerp.lerp_use_confirmed_state = false;
	prop->co_lerp.lerp_ready = true;

	// Overwrite or otherwise Allocate state
	WyncState_set_from_buffer(
		&prop->co_lerp.lerp_left_state,
		prop->co_xtrap.pred_prev.data.data_size,
		prop->co_xtrap.pred_prev.data.data);
	WyncState_set_from_buffer(
		&prop->co_lerp.lerp_right_state,
		prop->co_xtrap.pred_curr.data.data_size,
		prop->co_xtrap.pred_curr.data.data);
}


void WyncLerp_client_set_lerp_ms(
	WyncCtx *ctx,
	float server_tick_rate,
	u32 lerp_ms
) {
	ctx->co_lerp.lerp_ms = MAX(lerp_ms, ceil((1000.0 / server_tick_rate) * 2));

	// TODO: Also set maximum based on tick history size
	// Note: What about tick differences between server and clients?
}



// ==================================================
// Public API
// ==================================================



/// How much the lerping is allowed to extrapolate when missing packages
/// Beware of rubber-banding
void WyncLerp_set_max_lerp_factor_symmetric(
	WyncCtx *ctx,
	float max_lerp_factor_symmetric
) {
	ctx->co_lerp.max_lerp_factor_symmetric = max_lerp_factor_symmetric;
}


void WyncLerp_handle_packet_client_set_lerp_ms(
	WyncCtx *ctx,
	WyncPktClientSetLerpMS pkt,
	u32 from_nete_peer_id
) {
	// client and prop exists

	u16 wync_peer_id;
	i32 err = WyncJoin_is_peer_registered(ctx, from_nete_peer_id, &wync_peer_id);
	if (err != OK) {
		LOG_ERR_C(ctx, "client %u is not registered", wync_peer_id);
	}

	Wync_ClientInfo *client_info = &ctx->common.client_has_info[wync_peer_id];
	client_info->lerp_ms = pkt.lerp_ms;
}


void WyncLerp_precompute(WyncCtx *ctx) {
	Wync_PeerLatencyInfo *latency_info =
		&ctx->common.peer_latency_info[SERVER_PEER_ID];
	ctx->co_lerp.lerp_latency_ms = latency_info->latency_stable_ms;
	float curr_time = WyncClock_get_tick_timestamp_ms(ctx, ctx->common.ticks);
	float target_time_conf = curr_time - ctx->co_lerp.lerp_ms - ctx->co_lerp.lerp_latency_ms;


	// precompute which ticks we'll be interpolating
	// TODO: might want to use another filtered prop list for 'predicted'.
	// Before doing that we might need to settled on our strategy for extrapolation as fallback
	// of interpolation for confirmed states

	u32_DynArrIterator it = { 0 };
	WyncProp *prop = NULL;
	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_state__interpolated_regular_prop_ids, &it) == OK)
	{
		u32 prop_id = *it.item;
		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		if (prop->xtrap_enabled) {
			WyncLerp_precompute_predicted(ctx, prop);
		} else {
			WyncLerp_precompute_confirmed_states(ctx, prop, target_time_conf);
		}
	}
}


// ==================================================
// WRAPPER
// ==================================================


void WyncLerp_register_lerp_type (
	WyncCtx *ctx,
	u16 user_type_id,
	WyncWrapper_LerpFunc lerp_func
){
	if (user_type_id >= WYNC_MAX_USER_TYPES) {
		LOG_ERR_C(ctx, "User type outside allowed range");
		assert(false);
	}
	ctx->wrapper->lerp_function[user_type_id] = lerp_func;
}


/// interpolates confirmed states and predicted states
/// @param delta_lerp_fraction Usually but not always in range 0 to 1. Fraction
/// through the current physics tick we are at the time of rendering the frame.
void WyncLerp_interpolate_all (
	WyncCtx *ctx,
	float delta_lerp_fraction
) {
	float frame = 1000.0 / ctx->common.physic_ticks_per_second;

	// Note: substracting one frame to `target_time_conf` to compensate for one
	// frame added by delta_fraction_ms

	float delta_fraction_ms = delta_lerp_fraction * frame;
	float target_time_pred = delta_fraction_ms;
	float target_time_conf = delta_fraction_ms
		- frame - ctx->co_lerp.lerp_ms - ctx->co_lerp.lerp_latency_ms;

	// time between last rendered tick and current frame target

	float last_tick_rendered_left_timestamp = frame * (float)(
		(i32) ctx->co_ticks.last_tick_rendered_left
		- (i32) ctx->co_ticks.server_tick_offset
		- (i32) ctx->common.ticks
	);
	ctx->co_ticks.minimum_lerp_fraction_accumulated_ms =
		target_time_conf - last_tick_rendered_left_timestamp;

	// Note ^^^: Expanded equation: frame * (ctx.co_ticks.server_tick_offset
	// + ctx.co_ticks.ticks + delta_lerp_fraction
	// - ctx.co_ticks.last_tick_rendered_left -1) - ctx.co_predict_data.lerp_ms
	// - ctx.co_predict_data.lerp_latency_ms

	float left_timestamp_ms = 0;
	float right_timestamp_ms = 0;
	float factor = 0;

	WyncProp *prop = NULL;
	WyncWrapper_Setter *setter_lerp;
	WyncWrapper_UserCtx *user_ctx;
	WyncWrapper_LerpFunc *lerp_func;

	WyncState left_value = { 0 };
	WyncState right_value = { 0 };
	WyncWrapper_Data lerped_state = { 0 };
	u32 prop_id;

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_state__interpolated_regular_prop_ids, &it) == OK)
	{
		prop_id = *it.item;
		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		if (!prop->co_lerp.lerp_ready) { continue; }

		left_value = prop->co_lerp.lerp_left_state;
		right_value = prop->co_lerp.lerp_right_state;

		// Note: strictly getting time by ticks

		left_timestamp_ms =
			(prop->co_lerp.lerp_left_local_tick - (i32)ctx->common.ticks) * frame;
		right_timestamp_ms =
			(prop->co_lerp.lerp_right_local_tick - (i32)ctx->common.ticks) * frame;
				
		setter_lerp = &ctx->wrapper->prop_setter_lerp[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		lerp_func =
			&ctx->wrapper->lerp_function[prop->co_lerp.lerp_user_data_type];

		if (setter_lerp == NULL || user_ctx == NULL || lerp_func == NULL) {
			LOG_ERR_C(ctx,
					"Invalid Lerp function, Setter function or User context");
			continue;
		}
		
		if (fabs(left_timestamp_ms - right_timestamp_ms) < 1e-5)
		{
			lerped_state =
				(WyncWrapper_Data) { right_value.data_size, right_value.data };

			(*setter_lerp)( *user_ctx, lerped_state);
			continue;
		}

		factor = ((
			prop->co_lerp.lerp_use_confirmed_state ?
			target_time_conf : target_time_pred) - left_timestamp_ms)
			/ (right_timestamp_ms - left_timestamp_ms);

		if (factor < (0 - ctx->co_lerp.max_lerp_factor_symmetric)
			|| factor > (1 + ctx->co_lerp.max_lerp_factor_symmetric))
		{
			continue;
		}

		lerped_state = (*lerp_func) (
			(WyncWrapper_Data) { left_value.data_size, left_value.data },
			(WyncWrapper_Data) { right_value.data_size, right_value.data },
			factor
		);

		(*setter_lerp)( *user_ctx, lerped_state);

		WyncState lerped_state_to_free =
			(WyncState) {lerped_state.data_size, lerped_state.data};
		WyncState_free(&lerped_state_to_free);
	}
}


#endif // !WYNC_LERP
