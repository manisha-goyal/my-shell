#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

char** read_and_tokenize_input(int *num_args);
void memory_cleanup(char **args, int num_args);

int main(void) {
    char cwd[PATH_MAX]; 
    while (1) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("[nyush %s]$ ", basename(cwd));
        } else {
            fprintf(stderr, "Error in getting base directory, getcwd()\n");
            exit(EXIT_FAILURE);
        }
        fflush(stdout);

        int num_args = 0;
        char **args = read_and_tokenize_input(&num_args);

        if (num_args == 0) {
            free(args);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: fork failed, unable to execute command\n");
            memory_cleanup(args, num_args);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "Error: execvp failed\n");
                memory_cleanup(args, num_args);
                exit(EXIT_FAILURE);
            }
        } else {
            int status;
            waitpid(pid, &status, 0);
        }

        for (int i = 0; i < num_args; i++) {
            free(args[i]);
        }
        free(args);
    }
}

void memory_cleanup(char **args, int num_args){
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
    free(args);
}

char** read_and_tokenize_input(int *num_args) {
    char *input = NULL;
    size_t input_size = 0;
    ssize_t input_chars_read = getline(&input, &input_size, stdin);

    if (input_chars_read == -1) {
        fprintf(stderr, "Error: getline failed, unable to execute command\n");
        free(input); 
        return NULL;
    }

    if (input[input_chars_read - 1] == '\n') {
        input[input_chars_read - 1] = '\0';
    }

    char **input_args = (char **)malloc(sizeof(char *) * (input_chars_read + 1));
    char *saveptr;
    char *arg = strtok_r(input, " ", &saveptr);
    while (arg != NULL) {
        input_args[*num_args] = strdup(arg);
        (*num_args)++;
        arg = strtok_r(NULL, " ", &saveptr);
    }

    free(input);
    return input_args;
}

/*
References
https://www.codecademy.com/resources/docs/c
https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
https://opensource.com/article/22/5/safely-read-user-input-getline
https://systems-encyclopedia.cs.illinois.edu/articles/c-strtok/
https://stackoverflow.com/questions/252782/strdup-what-does-it-do-in-c
*/
