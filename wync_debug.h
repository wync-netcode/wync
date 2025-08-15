#ifndef WYNC_DEBUG_H

#include "wync/lib/log.h"
#include "wync_packet_util.h"

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

#endif // !WYNC_DEBUG_H
