#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

char** get_user_input(int *user_input_status);
int builtin_commands_handler(char **args);
char *path_handler(char **args);
void input_redirection_handler(char **args);
void output_redirection_handler(char **args);
void single_command_handler(char **args, char *program_path);
int has_pipe(char **args);
char ***get_pipe_args(char **args);
void pipe_commands_handler(char ***args_pipe);
int get_num_args(char **args);
int get_num_args_pipe(char ***args_pipe);
void memory_cleanup(char **args);
void memory_cleanup_pipe(char ***args_pipe);

int main(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    char cwd[PATH_MAX]; 
    while (1) {
        if (getcwd(cwd, sizeof(cwd))) {
            printf("[nyush %s]$ ", basename(cwd));
            fflush(stdout);
        } else {
            fprintf(stderr, "Error in getting base directory, getcwd()\n");
            exit(EXIT_FAILURE);
        }

        int user_input_status = 0;
        char **args = get_user_input(&user_input_status);

        if(user_input_status != 0) {
            if (args) 
                free(args);
            if (user_input_status == 1)
                exit(EXIT_SUCCESS);
            else
                continue;
        }

        if (builtin_commands_handler(args)) {
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
            char ***args_pipe = get_pipe_args(args);
            if(args_pipe == NULL)
                continue;
            pipe_commands_handler(args_pipe);
            memory_cleanup_pipe(args_pipe);
        }
        else {
            char *program_path = path_handler(args);
            single_command_handler(args, program_path);
            free(program_path);
            memory_cleanup(args);
        }
    }
    return EXIT_SUCCESS; 
}

char** get_user_input(int *user_input_status) {
    char *input = malloc(1001 * sizeof(char));
    size_t input_size = 0;
    ssize_t input_chars_read = getline(&input, &input_size, stdin);
    *user_input_status = EXIT_SUCCESS;

    if (input_chars_read == -1) {
        if(feof(stdin)) {
            *user_input_status = EXIT_FAILURE;
        }
        else {
            *user_input_status = -1;
            fprintf(stderr, "Error: getline failed, unable to execute command\n");
        }
        free(input); 
        return NULL;
    }

    if (input[input_chars_read - 1] == '\n')
        input[input_chars_read - 1] = '\0';
    
    char *saveptr;
    char *arg = strtok_r(input, " ", &saveptr);
    int num_args = 0;
    char **input_args = malloc(sizeof(char *) * (input_chars_read + 1));

    while (arg) {
        input_args[num_args] = strdup(arg);
        (num_args)++;
        arg = strtok_r(NULL, " ", &saveptr);
    }
    
    input_args[num_args] = NULL;
    free(input);

    return input_args;
}

int builtin_commands_handler(char **args) {
    int num_args = get_num_args(args);

    if (strcmp(args[0], "cd") == 0) {
        if (num_args != 2) {
            fprintf(stderr, "Error: invalid command\n");
        } else if (chdir(args[1]) != 0) {
            fprintf(stderr, "Error: invalid directory\n");
        }
        return EXIT_FAILURE;
    }

    if (strcmp(args[0], "exit") == 0) {
        if(num_args != 1) {
            fprintf(stderr, "Error: invalid command\n");
            return EXIT_FAILURE;
        }
        memory_cleanup(args);
        exit(EXIT_SUCCESS);
    }

    return EXIT_SUCCESS;
}

char *path_handler(char **args) {
    char *input_path = args[0];
    char *program_path = NULL;

    if(input_path[0]=='/' || (input_path[0]=='.' && input_path[1]=='/')) {
        program_path = strdup(input_path);
    }
    else {
        if (strchr(input_path, '/')) {
            program_path = malloc(strlen(input_path) + 3);
            strcpy(program_path, "./");
            strcat(program_path, input_path);
        }
        else {
            const char *program_dir = "/usr/bin/";
            program_path = malloc(strlen(input_path) + strlen(program_dir) + 1);
            strcpy(program_path, program_dir);
            strcat(program_path, input_path);
        }
    }

    return program_path;
}

void input_redirection_handler(char **args) {
    char *input_file = NULL;
    int input_count = 0;
    int input_pos = -1;
    int num_args = get_num_args(args);

    for (int i = 0; i < num_args; i++)
        if (strcmp(args[i], "<") == 0) {
            input_pos = i;
            if (++input_count > 1){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
        }

    if(input_pos != -1) {
        if(input_pos + 1 >= num_args) {
            fprintf(stderr, "Error: invalid command\n");
            exit(EXIT_FAILURE);
        }

        input_file = args[input_pos + 1];
        if (access(input_file, F_OK) != 0) {
            fprintf(stderr, "Error: invalid file\n");
            exit(EXIT_FAILURE);
        }

        int fd_in = open(input_file, O_RDONLY);
        if (fd_in == -1) {
            fprintf(stderr, "Error: invalid file\n");
            exit(EXIT_FAILURE);
        }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);

        for (int j = input_pos; j < num_args - 2; j++) {
            args[j] = args[j + 2];
        }
        args[num_args - 2] = NULL;
        args[num_args - 1] = NULL;
    }
}

void output_redirection_handler(char **args) {
    char *output_file = NULL;
    int output_append = 0;
    int output_count = 0;
    int output_pos = -1;
    int num_args = get_num_args(args);

    for (int i = 0; i < num_args; i++)
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            output_pos = i;
            if (++output_count > 1){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
        }

    if(output_pos != -1) {
        if(output_pos + 1 >= num_args) {
            fprintf(stderr, "Error: invalid command\n");
            exit(EXIT_FAILURE);
        }

        output_file = args[output_pos + 1];
        output_append = strcmp(args[output_pos], ">>") == 0;

        int fd_out = open(output_file, output_append ? 
                        O_CREAT | O_WRONLY | O_APPEND : O_CREAT | O_WRONLY | O_TRUNC, 
                        S_IRUSR | S_IWUSR);
        if (fd_out == -1) {
            fprintf(stderr, "Error: invalid file\n");
            exit(EXIT_FAILURE);
        }
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);

        for (int j = output_pos; j < num_args - 2; j++) {
            args[j] = args[j + 2];
        }
        args[num_args - 2] = NULL;
        args[num_args - 1] = NULL;
    }
}

void single_command_handler(char **args, char *program_path) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork failed, unable to execute command\n");
        return;
    } 
    else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        input_redirection_handler(args);
        output_redirection_handler(args);
        if (execv(program_path, args) == -1) {
            fprintf(stderr, "Error: invalid program\n");
            exit(EXIT_FAILURE);
        }
    } 
    else {
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
    }
}

int has_pipe(char **args) {
    int i = 0;
    int pipe_pos = -1;

    while (args[i]) {
        if (strcmp(args[i], "|") == 0)
            pipe_pos = i;
        i++;
    }

    if(strcmp(args[i-1], "|") == 0)
        pipe_pos = 0;

    return pipe_pos;
}

char ***get_pipe_args(char **args) {
    int pipe_count = 1;
    int num_args = get_num_args(args);
    int i=0;

    while (args[i]) {
        if (strcmp(args[i], "|") == 0)
            pipe_count++;
        i++;
    }

    int args_pos = 0;
    int counter = 0;
    char ***args_pipe = malloc(sizeof(char **) * (pipe_count + 1));

    for (i = 0; i <= num_args; ++i) {
        if (i == num_args || strcmp(args[i], "|") == 0) {
            int arg_length = i - counter;
            args_pipe[args_pos] = malloc((arg_length + 1) * sizeof(char *));

            for (int j = 0; j < arg_length; ++j) {
                if ((strcmp(args[counter + j], "<") == 0 && args_pos != 0) ||
                    ((strcmp(args[counter + j], ">") == 0 || strcmp(args[counter + j], ">>") == 0) && args_pos != pipe_count - 1)) {
                    fprintf(stderr, "Error: invalid command\n");
                    free(args_pipe);
                    return NULL;
                }
                args_pipe[args_pos][j] = strdup(args[counter + j]);
            }
            
            char *program_path = path_handler(args_pipe[args_pos]);
            free(args_pipe[args_pos][0]);
            args_pipe[args_pos][0] = strdup(program_path);
            free(program_path);
            
            args_pipe[args_pos][arg_length] = NULL;
            args_pos++;
            counter = i + 1;
        }
    }

    args_pipe[pipe_count] = NULL;
    return args_pipe;
}

void pipe_commands_handler(char ***args_pipe) {
    int num_args_pipe = get_num_args_pipe(args_pipe);
    int pipes[2 * (num_args_pipe - 1)];
    pid_t pids[num_args_pipe];

    for (int i = 0; i < num_args_pipe - 1; ++i) {
        if (pipe(pipes + i * 2) < 0) {
            fprintf(stderr, "Error: pipe failed, unable to execute command\n");
            return;
        }
    }

    for (int i = 0; i < num_args_pipe; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (i == 0) {
                input_redirection_handler(args_pipe[i]);
            }
            if (i == num_args_pipe - 1) {
                output_redirection_handler(args_pipe[i]);
            }

            if (i > 0) {
                dup2(pipes[(i - 1) * 2], 0);
            }

            if (i < num_args_pipe - 1) {
                dup2(pipes[i * 2 + 1], 1);
            }

            for (int j = 0; j < 2 * (num_args_pipe - 1); ++j) {
                close(pipes[j]);
            }

            if (execv(args_pipe[i][0], args_pipe[i]) == -1) {
                fprintf(stderr, "Error: invalid program\n");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            fprintf(stderr, "Error: fork failed, unable to execute command\n");
            return;
        }
        else {
            pids[i] = pid;
        }
    }

    for (int i = 0; i < 2 * (num_args_pipe - 1); ++i) {
        close(pipes[i]);
    }

    int status;
    for (int i = 0; i < num_args_pipe; ++i) {
        do {
            waitpid(pids[i], &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
    }
}

int get_num_args(char **args) {
    int num_args = 0;
    while(args[num_args]) 
        num_args++;
    return num_args;
}

int get_num_args_pipe(char ***args_pipe) {
    int num_args_pipe = 0;
    while(args_pipe[num_args_pipe]) 
        num_args_pipe++;
    return num_args_pipe;
}

void memory_cleanup(char **args) {
    int i = 0;
    while(args[i])
        free(args[i++]);
    free(args);
}

void memory_cleanup_pipe(char ***args_pipe) {
    int i = 0;
    while(args_pipe[i]) {
        int j=0;
        while(args_pipe[i][j])
            free(args_pipe[i][j++]);
        free(args_pipe[i++]);
    }
    free(args_pipe);
}

/*References
https://www.codecademy.com/resources/docs/c
https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
https://opensource.com/article/22/5/safely-read-user-input-getline
https://stackoverflow.com/questions/1516122/how-to-capture-controld-signal
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
https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
https://people.cs.rutgers.edu/~pxk/416/notes/c-tutorials/pipe.html
*/