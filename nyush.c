#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

char** user_input_handler(int *num_args);
void memory_cleanup(char **args);
int builtin_commands_handler(char **args, int num_args);
void command_path_handler(char **args, char **program_path);
int io_redirection_handler(char **args);
int has_pipe(char **args);
char** pipe_commands_handler(char **args, int pipe_pos, int num_args, int args_pos);

int main(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    char cwd[PATH_MAX]; 
    while (1) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("[nyush %s]$ ", basename(cwd));
            fflush(stdout);
        } else {
            fprintf(stderr, "Error in getting base directory, getcwd()\n");
            exit(EXIT_FAILURE);
        }

        int num_args = 0;
        char **args = user_input_handler(&num_args);
        char *program_path = NULL, *program_path1 = NULL, *program_path2 = NULL;

        if (num_args == 0 || args == NULL) {
            if (args) 
                free(args);
            continue;
        }

        if (builtin_commands_handler(args, num_args)) {
            memory_cleanup(args);
            continue;
        }

        int pipe_pos = has_pipe(args);

        if(pipe_pos == 0) {
            fprintf(stderr, "Error: invalid command\n");
            memory_cleanup(args);
            continue;
        }
        else if(pipe_pos != -1) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                fprintf(stderr, "Error: pipe failed, unable to execute command\n");
                memory_cleanup(args);
                continue;
            }

            char **args1 = pipe_commands_handler(args, pipe_pos, num_args, 0);
            char **args2 = pipe_commands_handler(args, pipe_pos, num_args, 1);
            command_path_handler(args1, &program_path1);
            command_path_handler(args2, &program_path2);

            pid_t pid1 = fork();
            if (pid1 < 0) {
                fprintf(stderr, "Error: fork failed, unable to execute command\n");
                free(program_path1);
                free(program_path2);
                memory_cleanup(args1);
                memory_cleanup(args2);
                memory_cleanup(args);
                continue;
            }
            if (pid1 == 0) { // First child executes the first part of the pipeline
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                if (execv(program_path1, args1) == -1) {
                    fprintf(stderr, "Error: invalid program\n");
                    free(program_path1);
                    free(program_path2);
                    memory_cleanup(args1);
                    memory_cleanup(args2);
                    memory_cleanup(args);
                    exit(EXIT_FAILURE);
                }
            } else {
                pid_t pid2 = fork();
                if (pid2 < 0) {
                    fprintf(stderr, "Error: fork failed, unable to execute command\n");
                    free(program_path1);
                    free(program_path2);
                    memory_cleanup(args1);
                    memory_cleanup(args2);
                    memory_cleanup(args);
                    continue;
                }
                if (pid2 == 0) {
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
                    
                    if (execv(program_path2, args2) == -1) {
                        fprintf(stderr, "Error: invalid program\n");
                        free(program_path1);
                        free(program_path2);
                        memory_cleanup(args1);
                        memory_cleanup(args2);
                        memory_cleanup(args);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    close(pipefd[0]);
                    close(pipefd[1]);
                    int status;
                    do {
                        waitpid(pid1, &status, 0);
                        waitpid(pid2, &status, 0);
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
                    free(program_path1);
                    free(program_path2);
                    memory_cleanup(args1);
                    memory_cleanup(args2);
                    memory_cleanup(args);
                }
            }
        }
        else {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Error: fork failed, unable to execute command\n");
                memory_cleanup(args);
                continue;
            } else if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                command_path_handler(args, &program_path);

                int io_redirection = io_redirection_handler(args);
                if(io_redirection != 0) {
                    if (io_redirection == -1)
                        fprintf(stderr, "Error: invalid command\n");
                    if (io_redirection == -2 || io_redirection == -3)
                        fprintf(stderr, "Error: invalid file\n");
                    free(program_path);
                    memory_cleanup(args);
                    exit(EXIT_FAILURE);
                }
                if (execv(program_path, args) == -1) {
                    fprintf(stderr, "Error: invalid program\n");
                    free(program_path);
                    memory_cleanup(args);
                    exit(EXIT_FAILURE);
                }
                
            } else {
                int status;
                do {
                    waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
                free(program_path);
                memory_cleanup(args);
            }
        }
    }
    return EXIT_SUCCESS; 
}

void memory_cleanup(char **args) {
    int i = 0;
    while(args[i] != NULL)
        free(args[i++]);
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
        memory_cleanup(args);
        exit(EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

void command_path_handler(char **args, char **program_path) {
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

int io_redirection_handler(char **args) {
    int input_count = 0, output_count = 0;
    char *input_file = NULL, *output_file = NULL;
    int output_append = 0;
    int num_args = 0;

    while(args[num_args] != NULL) 
        num_args++;

    for (int i = 0; i < num_args; i++) {
        if (strcmp(args[i], "<") == 0) {
            input_count++;
            if (input_count > 1 || i + 1 >= num_args) 
                return -1;
            input_file = args[i + 1];
            if (access(input_file, F_OK) != 0) 
                return -2;
        } else if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            output_count++;
            if (output_count > 1 || i + 1 >= num_args) 
                return -1;
            output_file = args[i + 1];
            output_append = strcmp(args[i], ">>") == 0;
        }
    }

    if (input_file) {
        int fd_in = open(input_file, O_RDONLY);
        if (fd_in == -1) 
            return -3;
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }

    if (output_file) {
        int fd_out = open(output_file, output_append ? 
                        O_CREAT | O_WRONLY | O_APPEND
                        : O_CREAT | O_WRONLY | O_TRUNC, 
                        S_IRUSR | S_IWUSR);
        if (fd_out == -1) 
            return -3;
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }

    for (int i = 0; i < num_args; i++) {
        if (args[i] && (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0)) {
            for (int j = i; j < num_args - 2; j++) {
                args[j] = args[j + 2];
            }
            args[num_args - 2] = NULL;
            args[num_args - 1] = NULL;
            i -= 1;
            num_args -= 2;
        }
    }
    return EXIT_SUCCESS;
}

int has_pipe(char **args) {
    int i = 0;
    int pipe_pos = -1;
    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
            return pipe_pos;
        }
        i++;
    }
    return pipe_pos;
}

char** pipe_commands_handler(char **arg, int pipe_pos, int num_args, int args_pos) {
    if(args_pos == 0 ) {
        char **pipe_args = malloc(sizeof(char *) * (pipe_pos + 1));
        int i=0;
        for(i=0; i<pipe_pos; i++) {
            pipe_args[i] = strdup(arg[i]);
        }
        pipe_args[i] = NULL;
        return pipe_args;
    } else {
        char **pipe_args = malloc(sizeof(char *) * (num_args - pipe_pos + 1));
        int i=0;
        int j=0;
        for(i=pipe_pos + 1; i<num_args; i++) {
            pipe_args[j++] = strdup(arg[i]);
        }
        pipe_args[j] = NULL;
        return pipe_args;
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
https://stackoverflow.com/questions/4204915/please-explain-the-exec-function-and-its-family
https://www.tutorialspoint.com/unix_system_calls/waitpid.htm
https://www.scaler.com/topics/c/string-comparison-in-c/
https://www.ibm.com/docs/en/zos/2.3.0?topic=functions-chdir-change-working-directory
https://www.geeksforgeeks.org/concatenating-two-strings-in-c/
https://www.cs.utexas.edu/~theksong/posts/2020-08-30-using-dup2-to-redirect-output/
https://www.educative.io/answers/how-to-use-the-pipe-system-call-for-inter-process-communication
*/
