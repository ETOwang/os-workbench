#include "ulib.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: rmdir <directory>\n");
        return -1;
    }

    if (unlinkat(AT_FDCWD,argv[1],AT_REMOVEDIR) < 0) {
        printf("rmdir: failed to remove '%s'\n", argv[1]);
        return -1;
    }

    return 0;
}