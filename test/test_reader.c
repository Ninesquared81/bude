#include <stdio.h>
#include <stdlib.h>

#include "../src/reader.h"
#include "../src/writer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "ERROR: Not enough arguments.\n");
        exit(1);
    }
    if (argc > 2) {
        fprintf(stderr, "WARNING: Extra arguments ignored.\n");
    }
    struct module module = read_bytecode(argv[1]);
    display_bytecode(&module, stdout);
    return 0;
}
