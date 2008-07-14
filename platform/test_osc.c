#include <stdio.h>
#include <stlib.h>

#define MAX_CMD_HASHES	512

typedef int (hashFn*)(void *);

typedef struct _cmdHash {
	hashFn fn;
	uint16_t hash;
	cmdHash * next;
} cmdHash;

cmdHash cmdHashes[MAX_CMD_HASHES];

int main(int argc, char * argv[])
{
	
	return 0;
}
