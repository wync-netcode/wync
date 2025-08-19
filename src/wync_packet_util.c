#include "wync_private.h"
#include "assert.h"


bool WyncPacket_type_exists(u16 packet_type_id) {
	return (packet_type_id < WYNC_PKT_AMOUNT);
}


/// * Wraps data in a WyncPacket in a WyncPacketOut for delivery
/// * Will allocate and give you back WyncPacketOut, don't forget to free!
///
/// @param[out] *out_packet Must point to instance
/// @returns error
i32 WyncPacket_wrap_packet_out_alloc (
	WyncCtx *ctx,
	u16 to_wync_peer_id,
	u16 packet_type_id,
	u32 data_size,
	void *data,
	WyncPacketOut *out_packet
) {
	assert(out_packet != NULL);

	if (!WyncPacket_type_exists(packet_type_id)) {
		LOG_ERR_C(ctx, "Invalid packet_type_id(%hu)", packet_type_id);
		return -1;
	}

	i32 nete_peer_id = -1;
	i32 error = WyncJoin_get_nete_peer_id_from_wync_peer_id
		(ctx, to_wync_peer_id, &nete_peer_id);
	if (error != OK) {
		LOG_ERR_C(ctx, "Couldn't find a nete_peer_id for wync_peer_id(%hu)",
			to_wync_peer_id);
		return -1;
	}

	NeteBuffer buffer = { 0 };
	WyncPacket wync_pkt = { 0 };
	WyncPacketOut wync_pkt_out = { 0 };

	buffer.size_bytes = data_size + sizeof(WyncPacket) + sizeof(WyncPacketOut);
	buffer.data = calloc(sizeof(char), buffer.size_bytes);

	wync_pkt.packet_type_id = packet_type_id;
	wync_pkt.data.data_size = data_size;
	wync_pkt.data.data = data;

	if (!WyncPacket_serialize(false, &buffer, &wync_pkt)) {
		error = -2;
		goto WyncPacket_wrap_packet_out__defer;
	}

	wync_pkt_out.to_nete_peer_id = nete_peer_id;
	wync_pkt_out.data_size = buffer.cursor_byte;
	wync_pkt_out.data = calloc(sizeof(char), wync_pkt_out.data_size);

	memcpy(wync_pkt_out.data, buffer.data, wync_pkt_out.data_size);

	*out_packet = wync_pkt_out;
	error = OK;
	WyncPacket_wrap_packet_out__defer:
	free(buffer.data);
	buffer.data = NULL;

	return error;
}


/// Keeps a copy of out_packet, don't forget to free your copy
///
/// @returns error
i32 WyncPacket_try_to_queue_out_packet (
	WyncCtx *ctx,
	WyncPacketOut p_out_packet,
	bool reliable,
	bool already_commited,
	bool dont_ocuppy       // default false
) {
	if (p_out_packet.data_size == 0 || p_out_packet.data == NULL) {
		return -2;
	}

	WyncPacketOut out_packet = {
		.to_nete_peer_id = p_out_packet.to_nete_peer_id,
		.data_size = p_out_packet.data_size,
	};
	out_packet.data = malloc(out_packet.data_size);
	memcpy(out_packet.data, p_out_packet.data, out_packet.data_size);

	u32 packet_size = out_packet.data_size;
	if (packet_size >= ctx->common.out_packets_size_remaining_chars) {
		if (already_commited) {
			//pass
		} else {
			LOG_ERR_C(ctx, "DROPPED, Packet too big (%u), remaining data (%u), d(%u)",
				packet_size,
				ctx->common.out_packets_size_remaining_chars,
				packet_size-ctx->common.out_packets_size_remaining_chars);

			WyncPacketOut_free(&out_packet);
			return -1;
		}
	}

	if (!dont_ocuppy) {
		ctx->common.out_packets_size_remaining_chars -= packet_size;
	}

	if (reliable) {
		WyncPacketOut_DynArr_insert(&ctx->common.out_reliable_packets, out_packet);
	} else {
		WyncPacketOut_DynArr_insert(&ctx->common.out_unreliable_packets, out_packet);
	}

	return OK;
}


void WyncPacket_ocuppy_space_towards_packets_data_size_limit(
	WyncCtx *ctx,
	u32 bytes
) {
	ctx->common.out_packets_size_remaining_chars -= bytes;
}

void WyncPacket_set_data_limit_chars_for_out_packets(
	WyncCtx *ctx,
	u32 data_limit_bytes
) {
	ctx->common.out_packets_size_limit = data_limit_bytes;
	ctx->common.out_packets_size_remaining_chars = data_limit_bytes;
}
