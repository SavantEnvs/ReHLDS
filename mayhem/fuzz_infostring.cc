// Parses the input as a Half-Life "\key\value\key\value" info string and exercises lookup, set and removal.
// The first key found after a leading backslash drives the key-based operations.
#include "precompiled.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// The InfoString API takes C strings.
	char *text = (char *)malloc(size + 1);
	memcpy(text, data, size);
	text[size] = '\0';

	InfoString info(text);
	info.ValueForKey("name");
	info.ValueForKey("model");
	info.ValueForKey(text);
	info.GetCurrentSize();

	// Use the first key found in the input as the one to set/remove.
	char key[64] = "name";
	if (size > 1 && text[0] == '\\')
	{
		size_t n = 0;
		while (text[1 + n] && text[1 + n] != '\\' && n < sizeof(key) - 1)
		{
			key[n] = text[1 + n];
			n++;
		}
		key[n] = '\0';
	}

	info.SetValueForKey(key, "value");
	info.SetValueForKey("*star", text);
	info.SetValueForStarKey("*star2", "x");
	info.ValueForKey(key);
	info.RemoveKey(key);
	info.RemovePrefixedKeys('_');
	info.RemovePrefixedKeys('*');
	info.SetString(text);
	info.Clear();

	free(text);
	return 0;
}
