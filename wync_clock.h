#ifndef WYNC_CLOCK_H
#define WYNC_CLOCK_H

#include "wync_typedef.h"


// ==================================================
// Private
// ==================================================

void WyncClock_advance_ticks (WyncCtx *ctx) {
	++ctx->common.ticks;
	++ctx->co_ticks.server_ticks;
	ctx->co_ticks.lerp_delta_accumulator_ms = 0;
}

void WyncClock_wync_handle_pkt_clock (WyncCtx *ctx) {
	// TODO
}

void WyncClock_wync_system_stabilize_latency (void);


void WyncClock_wync_client_set_physics_ticks_per_second (WyncCtx *ctx, u16 tps){
	ctx->common.physic_ticks_per_second = tps;
}

#endif // !WYNC_CLOCK_H
