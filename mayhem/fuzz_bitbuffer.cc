// Drives HLTV's BitBuffer reader over the input bytes with a script of read/skip operations
// taken from the tail of the input (FuzzedDataProvider), then round-trips values through the writer.
#include <fuzzer/FuzzedDataProvider.h>

#include "precompiled.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzedDataProvider fdp(data, size);
	size_t payloadSize = fdp.ConsumeIntegralInRange<size_t>(0, fdp.remaining_bytes() / 2);
	std::vector<uint8_t> payload = fdp.ConsumeBytes<uint8_t>(payloadSize);

	// BitBuffer::Resize() allocates 4 spare bytes because ReadBits() loads a whole dword at the cursor.
	uint8_t *mem = (uint8_t *)calloc(1, payload.size() + 4);
	if (!payload.empty())
		memcpy(mem, payload.data(), payload.size());

	BitBuffer in(mem, (unsigned int)payload.size());
	BitBuffer out((unsigned int)512);
	uint8_t scratch[256];
	float vec[3];

	for (int ops = 0; ops < 256 && fdp.remaining_bytes() > 0; ops++)
	{
		switch (fdp.ConsumeIntegralInRange<int>(0, 29))
		{
		case 0:  in.ReadBits(fdp.ConsumeIntegralInRange<int>(1, 32)); break;
		case 1:  in.ReadBit(); break;
		case 2:  in.ReadChar(); break;
		case 3:  in.ReadByte(); break;
		case 4:  in.ReadShort(); break;
		case 5:  in.ReadWord(); break;
		case 6:  in.ReadLong(); break;
		case 7:  in.ReadFloat(); break;
		case 8:  in.ReadString(); break;
		case 9:  in.ReadStringLine(); break;
		case 10: in.ReadBitString(); break;
		case 11: in.ReadSBits(fdp.ConsumeIntegralInRange<int>(1, 32)); break;
		case 12: in.ReadBitAngle(fdp.ConsumeIntegralInRange<int>(1, 32)); break;
		case 13: in.ReadBitData(scratch, fdp.ConsumeIntegralInRange<int>(0, sizeof(scratch))); break;
		case 14: in.ReadBuf(fdp.ConsumeIntegralInRange<int>(0, sizeof(scratch)), scratch); break;
		case 15: in.ReadAngle(); break;
		case 16: in.ReadHiresAngle(); break;
		case 17: in.ReadCoord(); break;
		case 18: in.ReadBitCoord(); break;
		case 19: in.ReadBitVec3Coord(vec); break;
		case 20: in.SkipBits(fdp.ConsumeIntegralInRange<int>(0, 64)); break;
		case 21: in.SkipBytes(fdp.ConsumeIntegralInRange<int>(0, 8)); break;
		case 22: in.SkipString(); break;
		case 23: in.AlignByte(); break;
		case 24: in.StartBitMode(); break;
		case 25: in.EndBitMode(); break;
		case 26: in.PeekBits(fdp.ConsumeIntegralInRange<int>(1, 32)); break;
		case 27: out.WriteBits(fdp.ConsumeIntegral<uint32_t>(), fdp.ConsumeIntegralInRange<int>(1, 32)); break;
		case 28: out.WriteByte(fdp.ConsumeIntegral<uint8_t>()); out.WriteShort(fdp.ConsumeIntegral<int16_t>()); out.WriteLong(fdp.ConsumeIntegral<uint32_t>()); break;
		case 29: out.WriteString(fdp.ConsumeRandomLengthString(40).c_str()); break;
		}
	}

	in.CurrentBit();
	in.CurrentSize();
	in.SpaceLeft();
	free(mem);
	return 0;
}
