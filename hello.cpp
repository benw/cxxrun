#!c++run -Wall -Werror -O3

#include <stdio.h>

int main(int argc, char** argv)
{
	int i;

	printf("Hello world!\n");
	for (i = 0; i < argc; i++) {
		printf("argv[%d] = '%s'\n", i, argv[i]);
	}
	return 5;
}

