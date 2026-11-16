// 64-bit launcher for the 32-bit engine unit-test runner: execs argv[1] with argv[2..].
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: exec32 <program> [args...]\n");
		return 2;
	}
	execv(argv[1], argv + 1);
	perror(argv[1]);
	return 127;
}
