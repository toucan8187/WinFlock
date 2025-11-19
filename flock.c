#include <stdio.h>
#include "flock.h"

#define PATH "C:\\...."

int main(int argc, char *argv[]) {
    int fd = open(PATH, O_RDONLY);
    printf("LOCK_SH: %d\n", flock(fd, LOCK_SH));
    printf("LOCK_EX: %d\n", flock(fd, LOCK_EX | LOCK_NB));
    flock_close(fd);
    return 0;
}