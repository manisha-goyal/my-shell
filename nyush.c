#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

int main() {
    char cwd[PATH_MAX]; 

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("[nyush %s]$ ", basename(cwd));
    } else {
        fprintf(stderr, "Error in getting base directory, getcwd()");
        exit(-1);
    }

    fflush(stdout);
    return 0;
}