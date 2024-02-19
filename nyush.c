#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

char** user_input_handler(int *num_args);
void memory_cleanup(char **args, int num_args);
int builtin_commands_handler(char **args, int num_args);

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
        char **args = user_input_handler(&num_args);

        if (num_args == 0 || args == NULL) {
            if (args) free(args);
            continue;
        }

        if (builtin_commands_handler(args, num_args)) {
            memory_cleanup(args, num_args);
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

        memory_cleanup(args, num_args);
    }
}

void memory_cleanup(char **args, int num_args){
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
    free(args);
}

char** user_input_handler(int *num_args) {
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

int builtin_commands_handler(char **args, int num_args) {
    if (strcmp(args[0], "cd") == 0) {
        if (num_args != 2) {
            fprintf(stderr, "Error: invalid command\n");
        } else if (chdir(args[1]) != 0) {
            fprintf(stderr, "Error: invalid directory\n");
        }
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        if(num_args != 1) {
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        memory_cleanup(args, num_args);
        exit(EXIT_SUCCESS);
    }
    return 0;
}

/*
References
https://www.codecademy.com/resources/docs/c
https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
https://opensource.com/article/22/5/safely-read-user-input-getline
https://systems-encyclopedia.cs.illinois.edu/articles/c-strtok/
https://www.geeksforgeeks.org/different-ways-to-copy-a-string-in-c-c/
https://stackoverflow.com/questions/252782/strdup-what-does-it-do-in-c
https://www.scaler.com/topics/c/string-comparison-in-c/
https://www.ibm.com/docs/en/zos/2.3.0?topic=functions-chdir-change-working-directory
*/
