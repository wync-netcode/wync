#include "wync_private.h"
#include "lib/log.h"
#include <string.h>

/// Increments Total
void WyncDebug_log_packet_received(WyncCtx *ctx, u16 packet_type_id) {
	if (!WyncPacket_type_exists(packet_type_id)) {
		LOG_ERR_C(ctx, "Invalid packet_type_id(%hu)", packet_type_id);
		return;
	}
	++(ctx->co_metrics.debug_packets_received[packet_type_id][0]);
}

/// Increments for a specific prop_id
void WyncDebug_received_log_prop_id(
	WyncCtx *ctx,
	u16 packet_type_id,
	u32 prop_id
) {
	if (!WyncPacket_type_exists(packet_type_id)) {
		LOG_ERR_C(ctx, "Invalid packet_type_id(%hu)", packet_type_id);
		return;
	}
	u32* history = ctx->co_metrics.debug_packets_received[packet_type_id];
	if ((prop_id + 1) < DEBUG_PACKETS_RECEIVED_MAX) {
		++history[prop_id +1];
	}
}


static void get_wync_latency_info(
	WyncCtx *wctx,
	char *buffer
) {
	static char line_aux[200] = "";

	Wync_PeerLatencyInfo *lat_info = NULL;

	u32 peer_amount = (u32)i32_DynArr_get_size(&wctx->common.peers);

	for (u16 peer_id = 0; peer_id < peer_amount; ++peer_id) {
		if (peer_id == wctx->common.my_peer_id) { continue; }

		lat_info = &wctx->common.peer_latency_info[peer_id];

		sprintf(line_aux, "peer(%d->%d) lat_stable %dms, m:%d, d:%d mean %.2f\n",
				wctx->common.my_peer_id, peer_id, lat_info->latency_stable_ms, lat_info->latency_mean_ms, lat_info->latency_std_dev_ms, lat_info->debug_latency_mean_ms);
		strcat(buffer, line_aux);
	}
}


void WyncDebug_get_info_general_text (
	WyncCtx *server_wctx,
	WyncCtx *client_wctx,
	char *lines
){
	static char single_line[200] = "";

	float client_time_ms = WyncClock_get_ms(client_wctx);
	float pred_server_time_ms = client_time_ms + client_wctx->co_pred.clock_offset_mean;
	float prob_prop_ms = (1000.0 / client_wctx->common.physic_ticks_per_second) * (client_wctx->co_metrics.low_priority_entity_update_rate +1);
	float latest_low_priority = * i32_RinBuf_get_relative (&client_wctx->co_metrics.low_priority_entity_update_rate_sliding_window, 0);

	single_line[0] = 0;

	#define EASY_LINE(...) \
	do { sprintf(single_line, __VA_ARGS__); \
		strcat(lines, single_line); } while (0)

	EASY_LINE("--- Wync latency (Server) ---\n");
	get_wync_latency_info(server_wctx, lines);
	EASY_LINE("--- Wync latency (Client 1) ---\n");
	get_wync_latency_info(client_wctx, lines);

	EASY_LINE("--- clock ---\n");
	EASY_LINE("  server_tick: %u\n", server_wctx->common.ticks);
	EASY_LINE("(cl)rver_tick: %d (d %d)\n", client_wctx->co_ticks.server_ticks,
			(i32)client_wctx->co_ticks.server_ticks - (i32)server_wctx->common.ticks);
	EASY_LINE("ser_tick_offs: %d\n", client_wctx->co_ticks.server_tick_offset);
	EASY_LINE("server_time: %.2lu\n", WyncClock_get_ms(server_wctx));
	EASY_LINE("(cl)rver_time: %.2f (d %.2f)\n", pred_server_time_ms,
			WyncClock_get_ms(server_wctx) - pred_server_time_ms);
	EASY_LINE("clock_offset_mean: %.2f\n", client_wctx->co_pred.clock_offset_mean);
	EASY_LINE("real time diff: %ld\n", (i64)WyncClock_get_ms(server_wctx) - (i64)WyncClock_get_ms(client_wctx));
	//EASY_LINE("%.d\n", client_wctx->common.debug_time_offset_ms);

	EASY_LINE("--- prediction ---\n");
	EASY_LINE("client tick: %u\n", client_wctx->common.ticks);
	EASY_LINE("target tick: %u\n", client_wctx->co_pred.target_tick);
	EASY_LINE("tick_offset: %d (from local ticks)\n", client_wctx->co_pred.tick_offset);
	EASY_LINE("predicted_ticks: %d\n", client_wctx->co_pred.last_tick_predicted
			-client_wctx->co_pred.first_tick_predicted);

	EASY_LINE("--- timewarp ---\n");
	EASY_LINE("delta_base_tick: %u\n", server_wctx->co_events.delta_base_state_tick);

	EASY_LINE("--- etc ---\n");
	EASY_LINE("lerp_ms: %hu\n", client_wctx->co_lerp.lerp_ms);
	EASY_LINE("server_rate_out: %f/t\n",
			server_wctx->co_metrics.debug_data_per_tick_sliding_window_mean);
	EASY_LINE("client_rate_out: %f/t\n",
			client_wctx->co_metrics.debug_data_per_tick_sliding_window_mean);
	EASY_LINE("(cl)server_tick_rate %.2f (%.2f tps)\n",
			client_wctx->co_metrics.server_tick_rate,
			((1.f / (client_wctx->co_metrics.server_tick_rate + 1))
			 * client_wctx->common.physic_ticks_per_second));
	EASY_LINE("(cl)prob_prop_rate %.2f (latest %f)\n",
			client_wctx->co_metrics.low_priority_entity_update_rate, latest_low_priority);
	EASY_LINE("1 update arrives each %.2fms\n", prob_prop_ms);
	EASY_LINE("(cl)max_pred_threeshold %d\n",
			client_wctx->co_pred.max_prediction_tick_threeshold);
	EASY_LINE("(cl)dummy_props %u (lost %u)\n", DummyProp_ConMap_get_key_count(&client_wctx->co_dummy.dummy_props),
			client_wctx->co_dummy.stat_lost_dummy_props);
	EASY_LINE("snap tick delay %f (expected ~%f)\n",
		client_wctx->co_metrics.snap_tick_delay_mean,
		client_wctx->common.peer_latency_info[SERVER_PEER_ID].latency_stable_ms/(1000.0/client_wctx->common.physic_ticks_per_second));

	#undef EASY_LINE
}


void WyncDebug_get_prop_info_text (WyncCtx *ctx, char *lines)
{
	static char single_line[200] = "";
	static char single_line_aux[200] = "";
	static u32_DynArr sorted_entity_ids = { 0 };
	if (sorted_entity_ids.capacity == 0) {
		sorted_entity_ids = u32_DynArr_create();
	}
	u32_DynArr_clear_preserving_capacity(&sorted_entity_ids);

	strcat(lines, "e_id  p_id  p_name_id\n");

	ConMapIterator it = { 0 };
	while (ConMap_iterator_get_next_key(&ctx->co_track.tracked_entities, &it) == OK)
	{
		u32 entity_id = it.key;
		u32_DynArr_insert(&sorted_entity_ids, entity_id);
	}
	u32_DynArr_sort(&sorted_entity_ids);

	u32_DynArrIterator it_dyna_entity_ids = { 0 };
	while(u32_DynArr_iterator_get_next(&sorted_entity_ids, &it_dyna_entity_ids) == OK)
	{
		u32 entity_id = *it_dyna_entity_ids.item;
		u32_DynArr *entity_props = NULL;
		u32_DynArr_ConMap_get(&ctx->co_track.entity_has_props, entity_id, &entity_props);

		u32_DynArrIterator it_dyna = { 0 };
		while (u32_DynArr_iterator_get_next(entity_props, &it_dyna) == OK)
		{
			u32 prop_id = *it_dyna.item;
			WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
			if (prop == NULL) {
				continue;
			}
			single_line[0] = 0;
			single_line_aux[0] = 0;

			sprintf(single_line_aux, "%u       ", entity_id);
			memcpy(single_line, single_line_aux, 6);
			sprintf(single_line_aux, "%u       ", prop_id);
			memcpy(single_line +6, single_line_aux, 6);
			sprintf(single_line_aux, "%s       ", prop->name_id);
			memcpy(single_line +12, single_line_aux, 60);
			strcat(single_line, "\n");

			strcat(lines, single_line);
		}
	}
}


void WyncDebug_get_packets_received_info_text (
	WyncCtx *ctx,
	char *lines,
	u16 prop_amount
) {
	u32 name_length = 10, number_length = 4;

	static char single_line[200] = "";
	single_line[0] = 0;
	
	sprintf(single_line, "Wync Peer %d Received\n", ctx->common.my_peer_id);
	strcat(lines, single_line);
	strncat(lines, "                    ", name_length+1);
	strncat(lines, "Tot    ", number_length);

	for (u16 i = 0; i < prop_amount-1; ++i) {
		sprintf(single_line, "%d               ", i);
		strncat(lines, single_line, number_length);
	}

	for (u16 i = 0; i < WYNC_PKT_AMOUNT; ++i) {

		strcat(lines, "\n");

		// pkt name

		const char *pkt_name = GET_PKT_NAME(i);
		u32 pkt_name_length = (u32)strlen(pkt_name);

		if (name_length > pkt_name_length) {
			for (u16 j = 0; j < (name_length - pkt_name_length); ++j) {
				strcat(lines, " ");
			}
		}

		u32 start_idx = 0;
		if (name_length < pkt_name_length) {
			start_idx = pkt_name_length - name_length;
		}
		strncat(
			lines, pkt_name + start_idx, MIN(name_length, strlen(pkt_name)));
		strcat(lines, " ");

		// amount of packets

		for (u16 j = 0; j < MIN(prop_amount, DEBUG_PACKETS_RECEIVED_MAX); ++j) {
			u32 received = ctx->co_metrics.debug_packets_received[i][j];
			sprintf(single_line, "%d               ", received);
			strncat(lines, single_line, number_length);
		}
	}
}
