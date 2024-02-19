#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

char** user_input_handler(int *num_args);
void memory_cleanup(char **args, int num_args);
int builtin_commands_handler(char **args, int num_args);
void command_path_handler(char **args, char **program_path);

int main(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

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

        char *program_path = NULL;
        command_path_handler(args, &program_path);

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: fork failed, unable to execute command\n");
            free(program_path);
            memory_cleanup(args, num_args);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            if (execv(program_path, args) == -1) {
                fprintf(stderr, "Error: invalid program\n");
                free(program_path);
                memory_cleanup(args, num_args);
                exit(EXIT_FAILURE);
            }
        } else {
            int status;
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
        }
        free(program_path);
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
    char *input = malloc(1001 * sizeof(char));
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

    char **input_args = malloc(sizeof(char *) * (input_chars_read + 1));
    char *saveptr;
    char *arg = strtok_r(input, " ", &saveptr);
    while (arg != NULL) {
        input_args[*num_args] = strdup(arg);
        (*num_args)++;
        arg = strtok_r(NULL, " ", &saveptr);
    }
    input_args[*num_args] = NULL;

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

void command_path_handler(char **args, char **program_path) {
    free(*program_path);
    *program_path = NULL;

    char *input_path = args[0];

    if(input_path[0]=='/' || (input_path[0]=='.' && input_path[1]=='/')) {
        *program_path = strdup(input_path);
    }
    else {
        if (strchr(input_path, '/') != NULL) {
            *program_path = malloc(strlen(input_path) + 3);
            if (*program_path) {
                strcpy(*program_path, "./");
                strcat(*program_path, input_path);
            }
        }
        else {
            const char *program_dir = "/usr/bin/";
            *program_path = malloc(strlen(input_path) + strlen(program_dir) + 1);
            if (*program_path) {
                strcpy(*program_path, program_dir);
                strcat(*program_path, input_path);
            }
        }
    }
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
https://www.geeksforgeeks.org/concatenating-two-strings-in-c/
*/
