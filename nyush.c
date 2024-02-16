#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

int main() {
    char cwd[PATH_MAX]; 
    while (1) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("[nyush %s]$ ", basename(cwd));
        } else {
            fprintf(stderr, "Error in getting base directory, getcwd()");
            exit(-1);
        }
        fflush(stdout);

        char *input = NULL;
        size_t input_size = 0;
        ssize_t input_chars_read;
        input_chars_read = getline(&input, &input_size, stdin);

        if (input_chars_read == -1) {
            fprintf(stderr, "Error in getting user input, getline()");
            free(input);
            return 1;
        }

        printf("%s", input);
        free(input);
    }
}


/*
References
https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
https://opensource.com/article/22/5/safely-read-user-input-getline
*/
