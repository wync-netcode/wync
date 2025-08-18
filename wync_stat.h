#ifndef WYNC_STAT_H
#define WYNC_STAT_H

#include "wync/wync_typedef.h"

// Call every time a WyncPktSnap contains _the prob_prop id_
void WyncStat_try_to_update_prob_prop_rate (WyncCtx *ctx);


void WyncStat_system_calculate_prob_prop_rate(WyncCtx *ctx);


// ==================================================
// WRAPPER
// ==================================================


/// setup "prob prop"
/// The prob prop acts is a low priority entity to sync, it's purpose it's to
/// allow us to measure how much ticks of delay there are between updates for
/// a especific single prop, based on that we can get a better stimate for
/// _prediction threeshold_
void WyncStat_setup_prob_for_entity_update_delay_ticks(
	WyncCtx *ctx, u32 peer_id
);


#endif // !WYNC_STAT_H
