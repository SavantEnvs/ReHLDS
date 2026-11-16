// Loads the input as a Half-Life BSP v30 map with HLTV's BSPModel (full and "minimal" loads), then
// runs the PVS / point-contents / line-trace queries over the loaded tree.
#include "mocksystem.h"

#include <stdint.h>
#include <string.h>

static void Query(BSPModel &bsp)
{
	static const float pts[][3] = { { 0, 0, 0 }, { 64, -32, 128 }, { -4096, 4096, -1 } };
	vec3_t impact;
	for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); i++)
	{
		vec3_t p = { pts[i][0], pts[i][1], pts[i][2] };
		bsp.SetPVS(p);
		bsp.InPVS(p);
		bsp.TruePointContents(p);
		bsp.TraceLine(p, (vec_t *)pts[(i + 1) % 3], impact);
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// BSPModel::LoadFromBuffer reads the 4-byte magic and the dheader_t lump table without a length check;
	// real maps always carry a full header, so shorter inputs are not fed to it (lump contents stay unchecked).
	if (size < sizeof(dheader_t))
		return 0;

	MockSystem sys;
	sys.SetFile(data, size);

	for (int minimal = 0; minimal < 2; minimal++)
	{
		BSPModel bsp;
		bsp.Init(&sys);
		if (bsp.Load("maps/fuzz.bsp", minimal != 0) && bsp.IsValid())
			Query(bsp);
		bsp.Clear();
	}

	return 0;
}
