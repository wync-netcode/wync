#ifndef WYNC_PACKET_UTIL_H
#define WYNC_PACKET_UTIL_H


#include "wync_typedef.h"


bool WyncPacket_type_exists(u16 packet_type_id);


// Wraps a valid packet in a WyncPacket in a WyncPacketOut for delivery
//
// @param[out] *out_packet
// @returns error
i32 WyncPacket_wrap_packet_out_alloc (
	WyncCtx *ctx,
	u16 to_wync_peer_id,
	u16 packet_type_id,
	u32 data_size,
	void *data,
	WyncPacketOut *out_packet
);


/// Takes ownership of WyncPacketOut (out_packet)
///
/// @returns error
i32 WyncPacket_try_to_queue_out_packet (
	WyncCtx *ctx,
	WyncPacketOut out_packet,
	bool reliable,
	bool already_commited,
	bool dont_ocuppy       // default false
);

void WyncPacket_ocuppy_space_towards_packets_data_size_limit(
	WyncCtx *ctx,
	u32 bytes
);

void WyncPacket_set_data_limit_chars_for_out_packets(
	WyncCtx *ctx,
	u32 data_limit_bytes
);

#endif // !WYNC_PACKET_UTIL_H
