#include <math.h>
#include "wync_private.h"
#include "assert.h"


void WyncStat_try_to_update_prob_prop_rate (WyncCtx *ctx) {
	if (ctx->co_metrics.low_priority_entity_tick_last_update == ctx->common.ticks)
	{ return; }
	i32 tick_rate =
		(i32)ctx->common.ticks -
		(i32)ctx->co_metrics.low_priority_entity_tick_last_update -1;
	i32_RinBuf_push(
		&ctx->co_metrics.low_priority_entity_update_rate_sliding_window,
		tick_rate, NULL, NULL);
	ctx->co_metrics.low_priority_entity_tick_last_update = ctx->common.ticks;
}


void WyncStat_system_calculate_prob_prop_rate(WyncCtx *ctx) {
	i32 accu = 0;
	size_t amount = ctx->co_metrics.low_priority_entity_update_rate_sliding_window_size;

	for (size_t i = 0; i < amount; ++i)
	{
		i32 value = *i32_RinBuf_get_at(
			&ctx->co_metrics.low_priority_entity_update_rate_sliding_window, i);
		accu += value;
	}

	ctx->co_metrics.low_priority_entity_update_rate = (double) accu / (double) amount;

	// TODO: Move this elsewhere

	// calculate prediction threeshold
	// adding 1 of padding for good measure
	ctx->co_pred.max_prediction_tick_threeshold =
		(i32) ceil(ctx->co_metrics.low_priority_entity_update_rate) + 1;

	// 'REGULAR_PROP_CACHED_STATE_AMOUNT -1' because for xtrap we need to set it
	// to the value just before 'ctx.max_prediction_tick_threeshold -1'
	/*ctx->co_pred.max_prediction_tick_threeshold = MIN(*/
			/*ctx->co_track.REGULAR_PROP_CACHED_STATE_AMOUNT-1,*/
			/*ctx->co_pred.max_prediction_tick_threeshold);*/
}



// ==================================================
// WRAPPER
// ==================================================

static inline WyncWrapper_Data prob_get_state (WyncWrapper_UserCtx ctx) {
	static u32 deadbeef = 0xDEADBEFF;
	WyncWrapper_Data data;
	data.data_size = sizeof(u32);
	data.data = malloc(data.data_size); 
	memcpy(data.data, &deadbeef, data.data_size);
	return data;
}

void WyncStat_setup_prob_for_entity_update_delay_ticks(
	WyncCtx *ctx, u32 peer_id
) {
	
	u32 entity_id = ENTITY_ID_PROB_FOR_ENTITY_UPDATE_DELAY_TICKS;
	u32 prob_id;

	WyncTrack_track_entity(ctx, entity_id, (u32)(-1));

	i32 err = WyncTrack_prop_register_minimal(
		ctx,
		entity_id,
		"entity_prob",
		WYNC_PROP_TYPE_STATE,
		&prob_id
	);
	assert(err == OK);

	// TODO: internal functions shouldn't be using wrapper functions...
	// Maybe we can treat these differently? These are all internal, so it
	// doesn't make sense to require external functions like the wrapper's
	WyncWrapper_set_prop_callbacks(
		ctx,
		prob_id,
		(WyncWrapper_UserCtx) { NULL, 0 },
		prob_get_state,
		NULL
	);

	ctx->co_metrics.PROP_ID_PROB = prob_id;

	// add as local existing prop
	if (!ctx->common.is_client) {
		WyncTrack_wync_add_local_existing_entity(ctx, peer_id, entity_id);
	}
}

/*
static func wync_system_calculate_data_per_tick(ctx: WyncCtx):

	var data_sent = ctx.common.out_packets_size_limit - ctx.common.out_packets_size_remaining_chars
	ctx.co_metrics.debug_data_per_tick_current = data_sent

	ctx.co_metrics.debug_ticks_sent += 1
	ctx.co_metrics.debug_data_per_tick_total_mean = (
		ctx.co_metrics.debug_data_per_tick_total_mean
 			* (ctx.co_metrics.debug_ticks_sent -1)
		+ data_sent) / float(ctx.co_metrics.debug_ticks_sent)

	ctx.co_metrics.debug_data_per_tick_sliding_window.push(data_sent)
	var data_sent_acc = 0
	for i in range(ctx.co_metrics.debug_data_per_tick_sliding_window_size):
		var value = ctx.co_metrics.debug_data_per_tick_sliding_window.get_at(i)
		if value is int:
			data_sent_acc +=
				ctx.co_metrics.debug_data_per_tick_sliding_window.get_at(i)
	ctx.co_metrics.debug_data_per_tick_sliding_window_mean =
		data_sent_acc / ctx.co_metrics.debug_data_per_tick_sliding_window_size

*/

void WyncStat_calculate_data_per_tick (WyncCtx *ctx) {
	LOG_OUT_C(ctx, "debugrate, remaining %d consumed %d",
		ctx->common.out_packets_size_remaining_chars,
		ctx->common.out_packets_size_limit
			- ctx->common.out_packets_size_remaining_chars
	);

	CoMetrics *metrics = &ctx->co_metrics;

	float data_sent = (float)ctx->common.out_packets_size_limit
			- (float)ctx->common.out_packets_size_remaining_chars;
	metrics->debug_data_per_tick_current = data_sent;

	metrics->debug_ticks_sent += 1;
	metrics->debug_data_per_tick_total_mean = (
		metrics->debug_data_per_tick_total_mean 
			* (metrics->debug_ticks_sent -1) + data_sent)
		/ metrics->debug_ticks_sent;
	;
	
	u32_RinBuf_push( &metrics->debug_data_per_tick_sliding_window,
			(uint)data_sent, NULL, NULL);

	uint data_sent_acc = 1;
	for (uint i = 0; i < metrics->debug_data_per_tick_sliding_window_size; ++i){
		uint value = *u32_RinBuf_get_at(
			&ctx->co_metrics.debug_data_per_tick_sliding_window, i);
		data_sent_acc += value;
	}

	metrics->debug_data_per_tick_sliding_window_mean = (float)data_sent_acc
		/ (float)ctx->co_metrics.debug_data_per_tick_sliding_window_size;
}
