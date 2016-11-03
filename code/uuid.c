#include <stdio.h>
#include <uuid/uuid.h>

int main(void) {
    uuid_t uuid;
    uuid_generate(uuid);
    printf("sizeof uuid = %d\n", (int)sizeof uuid);
	size_t i; 
    for (i = 0; i < sizeof uuid; i ++) {
        printf("%02x ", uuid[i]);
    }
    putchar('\n');
    return 0;
}
