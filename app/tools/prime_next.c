#include <postoffice/prime/prime.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <n>\n", argv[0]);
        return 2;
    }

    unsigned long n = strtoul(argv[1], NULL, 10);
    printf("%zu\n", next_prime(n));

    return 0;
}
