// Tokenizes the input as a sequence of HLTV console command lines (split on '\n') through one
// TokenLine, so SetLine's reset and re-parse paths run, and walks every accessor on each line.
#include "precompiled.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void WalkLine(TokenLine &line)
{
	int count = line.CountToken();
	for (int i = 0; i <= count; i++)
	{
		line.GetToken(i);
		line.GetRestOfLine(i);
		char *token = line.GetToken(i);
		if (token)
			line.CheckToken(token);
	}

	line.GetLine();
	line.CheckToken((char *)"-port");
	line.CheckToken((char *)"");
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *text = (char *)malloc(size + 1);
	memcpy(text, data, size);
	text[size] = '\0';

	TokenLine line;
	char *cursor = text;
	for (;;)
	{
		char *eol = strchr(cursor, '\n');
		if (eol)
			*eol = '\0';

		line.SetLine(cursor);
		WalkLine(line);

		if (!eol)
			break;
		cursor = eol + 1;
	}

	line.SetLine(NULL);
	WalkLine(line);

	TokenLine copy(text);
	WalkLine(copy);

	free(text);
	return 0;
}
