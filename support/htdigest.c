
#include "config.h"
#include "portability.h"

#include <stdio.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif /* NO_STDLIB_H */

int main(int argc, char **argv)
{
	char pre_digest[2048];
	char digest[33];

	if (argc != 4)
	{
		fprintf(stderr, "\n%s is useful for generating the digest entries for\n", argv[0]);
		fprintf(stderr, "your .htdigest file\n\n");
		fprintf(stderr, "Usage: %s username realm password\n", argv[0]);
		exit(-1);
	}

	sprintf(pre_digest, "%s:%s:%s", argv[1], argv[2], argv[3]);
	md5(pre_digest, digest);
	printf("%s:%s:%s\n", argv[1], argv[2], digest);
	return 0;
}

