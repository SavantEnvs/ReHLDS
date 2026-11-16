// Feeds a stream of UDP datagrams into a connected HLTV NetChannel: sequence/ack parsing,
// COM_UnMunge2, split-fragment validation + reassembly, and the BZ2 decompression of the
// reassembled message. Input = repeated [u16 little-endian length][datagram bytes].
#include "mocksystem.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	MockSystem sys;
	NetAddress addr;
	NetChannel chan;

	if (!chan.Create(&sys, nullptr, &addr))
		return -1;

	chan.SetConnected(true);

	// NetSocket::ReceivePacketIntern drops datagrams under 4 bytes and over MAX_UDP_PACKET-1 before
	// NetChannel::ProcessIncoming sees them, and hands over a MAX_UDP_PACKET-sized buffer.
	uint8_t *packet = (uint8_t *)calloc(1, MAX_UDP_PACKET);

	size_t pos = 0;
	for (int n = 0; n < 64 && pos + 2 <= size; n++)
	{
		size_t len = data[pos] | (data[pos + 1] << 8);
		pos += 2;
		if (len > size - pos)
			len = size - pos;

		if (len >= 4 && len < MAX_UDP_PACKET)
		{
			memset(packet, 0, MAX_UDP_PACKET);
			memcpy(packet, data + pos, len);
			chan.ProcessIncoming(packet, (int)len);
		}

		pos += len;
	}

	while (NetPacket *p = chan.GetPacket())
	{
		BitBuffer &b = p->data;
		if (b.GetMaxSize() > 0)
		{
			b.ReadLong();
			b.ReadString();
		}
		chan.FreePacket(p);
	}

	chan.Close();
	free(packet);
	return 0;
}
