#include "wync_private.h"
#include <math.h>
#include <time.h>


// ==================================================
// Private
// ==================================================

#ifdef _WIN32
#include <windows.h>
#else // Linux, macOS, etc.
#include <sys/time.h>
#endif

u64 WyncClock_get_system_milliseconds(void) {
#ifdef _WIN32
    return GetTickCount64();
#else // Linux, macOS, etc.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + (u64)tv.tv_usec / 1000;
#endif
}

u64 WyncClock_get_ms(WyncCtx* ctx){
	return (u64)(
		((i64)WyncClock_get_system_milliseconds()
		- (i64)ctx->co_ticks.start_time_ms)
		+ (i64)ctx->common.debug_time_offset_ms
	);
}

void WyncClock_client_handle_pkt_clock (WyncCtx *ctx, WyncPktClock pkt) {

	// see https://en.wikipedia.org/wiki/Cristian%27s_algorithm

	CoTicks *co_ticks = &ctx->co_ticks;
	CoPredictionData *co_pred = &ctx->co_pred;
	u16 physics_fps = ctx->common.physic_ticks_per_second;
	double curr_time = (double)WyncClock_get_ms(ctx);
	double curr_clock_offset =
		((double)pkt.time + (curr_time - (double)pkt.time_og) / 2) - (double)curr_time;

	printf("LATENCIA REAL %f\n", curr_time - (double)pkt.time_og);

	// calculate mean
	// Note: To improve accurace modify _server clock sync_ throttling or
	// sliding window size
	// Note: Use a better algorithm for calculating a stable long lasting value
	// of the clock offset
	//   Resistant to sudden lag spikes. Look into 'Trimmed mean'

	double_RinBuf_push(
		&co_pred->clock_offset_sliding_window, curr_clock_offset, NULL, NULL);

	u32 window_size =
		(u32)double_RinBuf_get_size(&co_pred->clock_offset_sliding_window);

	u32 count = 0;
	double acc = 0;

	for (u32 i = 0; i < window_size; ++i) {
		double i_clock_offset = 
			*double_RinBuf_get_at(&co_pred->clock_offset_sliding_window, i);
		if (i_clock_offset == 0)
			continue;

		++count;
		acc += i_clock_offset;
	}

	co_pred->clock_offset_mean = ceil(acc / (double)count);
	//double current_server_time = (double)curr_time + co_pred->clock_offset_mean;

	// update ticks

	float cal_server_ticks =
		(pkt.tick + ((curr_time - pkt.time_og) / 2.0) /
			(1000.0 / physics_fps));
	i32 new_server_ticks_offset =
		(i32)round(cal_server_ticks - (float)ctx->common.ticks);

	// Note: at the beggining 'server_ticks' will be equal to 0

	WyncOffsetCollection_add_value(co_ticks, new_server_ticks_offset);
	co_ticks->server_tick_offset = WyncOffsetCollection_get_most_common(co_ticks);
	co_ticks->server_ticks = (u32)((i32)ctx->common.ticks + co_ticks->server_tick_offset);
}

/// @returns error
i32 WyncClock_server_handle_pkt_clock_req (
	WyncCtx *ctx,
	WyncPktClock pkt,
	u16 from_nete_peer_id
) {
	i32 return_error = OK;
	WyncPacketOut packet_out = { 0 };

	do {
		u16 wync_peer_id;
		i32 error = WyncJoin_is_peer_registered(
			ctx, from_nete_peer_id, &wync_peer_id);
		if (error != OK) {
			LOG_ERR_C(ctx, "client %hu is not registered", from_nete_peer_id);
			return_error = -1;
			break;
		}

		// prepare packet back

		WyncPktClock packet_clock = {
			.time_og = pkt.time_og,
			.tick_og = pkt.tick_og,
			.tick = ctx->common.ticks,
			.time = WyncClock_get_ms(ctx)
		};
		
		static NeteBuffer buffer = { 0 };
		if (buffer.size_bytes == 0) {
			buffer.size_bytes = sizeof(WyncPktClock);
			buffer.data = calloc(1, buffer.size_bytes);
		}
		buffer.cursor_byte = 0;
		WyncPktClock_serialize(false, &buffer, &packet_clock);

		error = WyncPacket_wrap_packet_out_alloc(
			ctx,
			wync_peer_id,
			WYNC_PKT_CLOCK,
			buffer.cursor_byte,
			buffer.data,
			&packet_out
		);
		if (error != OK) {
			LOG_ERR_C(ctx, "couldn't wrap packet %p", (void *)&packet_clock);
			return_error = -2;
			break;
		}

		error = WyncPacket_try_to_queue_out_packet(
			ctx,
			packet_out,
			UNRELIABLE,
			false, false
		);
		if (error != OK) {
			LOG_ERR_C(ctx, "couldn't queue packet");
			return_error = -3;
			break;
		}
	} while (0);

	WyncPacketOut_free(&packet_out);

	return return_error;
}


void WyncClock_system_stabilize_latency (
	WyncCtx *ctx,
	Wync_PeerLatencyInfo *lat_info
) {
	if (FAST_MODULUS(ctx->common.ticks, 16) != 0) return;

	// poll latency

	lat_info->latency_buffer
		[lat_info->latency_buffer_head % LATENCY_BUFFER_SIZE] =
			lat_info->latency_raw_latest_ms;

	++lat_info->latency_buffer_head;

	// sliding window mean

	u32 counter = 0, accum = 0, mean = 0;
	u32 lat = 0;

	for (u32 i = 0; i < LATENCY_BUFFER_SIZE; ++i) {
		lat = lat_info->latency_buffer[i];
		if (lat == 0) continue;
		++counter;
		accum += lat;
	}

	if (counter == 0) return;
	mean = (u32)ceilf((float)accum / (float)counter);
	lat_info->debug_latency_mean_ms = (float)accum / (float)counter;

	// update if new mean is outside range

	bool is_outside = 
		abs((i32)mean - (i32)lat_info->latency_mean_ms) > lat_info->latency_std_dev_ms
		|| counter < LATENCY_BUFFER_SIZE;
	if (!is_outside) return;

	lat_info->latency_mean_ms = mean;

	// calculate std dev

	accum = 0;
	for (u32 i = 0; i < LATENCY_BUFFER_SIZE; ++i) {
		lat = lat_info->latency_buffer[i];
		if (lat == 0) continue;
		accum += (u32)pow(lat - lat_info->latency_mean_ms, 2);
	}

	// 98th percentile = mean + 2*std_dev

	lat_info->latency_std_dev_ms = (u32)ceil(sqrt((double)accum/(double)counter));
	lat_info->latency_stable_ms = lat_info->latency_mean_ms + lat_info->latency_std_dev_ms*2;
}


void WyncClock_update_prediction_ticks (WyncCtx *ctx) {

	Wync_PeerLatencyInfo *lat_info = &ctx->common.peer_latency_info[SERVER_PEER_ID];
	CoTicks *co_ticks = &ctx->co_ticks;
	CoPredictionData *co_pred = &ctx->co_pred;
	double curr_time = WyncClock_get_ms(ctx);
	u16 physics_fps = ctx->common.physic_ticks_per_second;

	// Adjust tick_offset_desired periodically to compensate for unstable ping
	
	if (FAST_MODULUS(ctx->common.ticks, 32) == 0) {

		co_pred->tick_offset_desired = (i32)ceil(lat_info->latency_stable_ms / (1000.0 / physics_fps)) + 2;
		
		u32 target_tick = (u32)MAX(co_ticks->server_ticks + co_pred->tick_offset, co_pred->target_tick);
		double target_time = curr_time + co_pred->tick_offset_desired * (1000.0 / physics_fps);

		LOG_OUT_C(ctx, "predict, tick_offset_desired %d tick_offset %d target_tick %u target_time %f",
			co_pred->tick_offset_desired, co_pred->tick_offset, target_tick, target_time
		);
	}
			
	// Smoothly transition tick_offset
	// NOTE: Should be configurable
	
	if (co_pred->tick_offset_desired != co_pred->tick_offset) {
		
		// up transition
		if (co_pred->tick_offset_desired > co_pred->tick_offset) {
			co_pred->tick_offset += 1;
		// down transition
		} else {
			if (co_pred->tick_offset == co_pred->tick_offset_prev) {
				co_pred->tick_offset -= 1;
			} else {
				// NOTE: Somehow I can't find another way to keep the prev updated
				co_pred->tick_offset_prev = co_pred->tick_offset;
			}
		}
	}

	// target_tick can only go forward. Use max so that we never go back
	u32 _prev_target_tick = co_pred->target_tick;
	co_pred->target_tick = MAX(co_ticks->server_ticks + co_pred->tick_offset, co_pred->target_tick);
	co_pred->current_tick_timestamp = curr_time;

	if (co_pred->target_tick - _prev_target_tick != 1) {
		LOG_OUT_C(ctx, "couldn't find input | target tick changed badly %u %u",
				_prev_target_tick, co_pred->target_tick);
	}
	
	// ==============================================================
	// Setup the next tick-action-history
	// Run before any prediction takes places on the current tick
	// NOTE: This could be moved elsewhere
	
	//WyncActions.action_tick_history_reset(ctx, co_pred.target_tick);
}

/// @returns error
i32 WyncClock_client_ask_for_clock(WyncCtx *ctx) {
	if (FAST_MODULUS(ctx->common.ticks, 16) != 0) return -1;

	i32 return_error = OK;
	WyncPacketOut packet_out = { 0 };

	do{
		WyncPktClock packet_clock = {
			.time_og = (u32)WyncClock_get_ms(ctx),
			.tick_og = ctx->common.ticks
		};

		static NeteBuffer buffer = { 0 };
		if (buffer.size_bytes == 0) {
			buffer.size_bytes = sizeof(WyncPktClock);
			buffer.data = calloc(1, buffer.size_bytes);
		}
		buffer.cursor_byte = 0;
		WyncPktClock_serialize(false, &buffer, &packet_clock);

		i32 error = WyncPacket_wrap_packet_out_alloc(
			ctx,
			SERVER_PEER_ID,
			WYNC_PKT_CLOCK,
			buffer.cursor_byte,
			buffer.data,
			&packet_out
		);
		if (error != OK) {
			LOG_ERR_C(ctx, "couldn't wrap packet %p", (void *)&packet_clock);
			return_error = -2;
			break;
		}

		error = WyncPacket_try_to_queue_out_packet(
			ctx,
			packet_out,
			UNRELIABLE,
			false, false
		);
		if (error != OK) {
			LOG_ERR_C(ctx, "couldn't queue packet");
			return_error = -3;
			break;
		}
	} while (0);

	WyncPacketOut_free(&packet_out);

	return return_error;
}

// ==================================================
// Private
// ==================================================

void WyncClock_advance_ticks (WyncCtx *ctx) {
	++ctx->common.ticks;
	++ctx->co_ticks.server_ticks;
	ctx->co_ticks.lerp_delta_accumulator_ms = 0;
}

/// TODO: Receive nete_peer_id then convert to wync_peer_id
/// set the latency this peer is experimenting (get it from your transport)
/// @argument latency_ms: int. Latency in milliseconds
void WyncClock_peer_set_current_latency (WyncCtx *ctx, u16 peer_id, u16 latency_ms){
	ctx->common.peer_latency_info[peer_id].latency_raw_latest_ms = latency_ms;
}

void WyncClock_wync_client_set_physics_ticks_per_second (WyncCtx *ctx, u16 tps){
	ctx->common.physic_ticks_per_second = tps;
}

void WyncClock_set_debug_time_offset(WyncCtx *ctx, u64 time_offset_ms){
	ctx->common.debug_time_offset_ms = time_offset_ms;
}

void WyncClock_set_ticks(WyncCtx *ctx, u32 ticks) {
	ctx->common.ticks = ticks;
}

float WyncClock_get_tick_timestamp_ms(WyncCtx *ctx, i32 ticks) {
	float frame = 1000.0f / ctx->common.physic_ticks_per_second;
	return (ctx->co_pred.current_tick_timestamp +
			(float)(ticks - (i32)ctx->common.ticks) * frame);
}

u32 WyncClock_get_ticks(WyncCtx *ctx) {
	return ctx->common.ticks;
}
