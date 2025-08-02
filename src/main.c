#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_TOKENS 64
#define DELIMITERS " \t\r\n\a"

// built-in command handling
int cd(char **args);
int help(char **args);
int shell_exit(char **args);

int dispatch(char **args, char *line);
void handle_redirection(char **args);

char **split_line(char *line) {
    int bufsize = MAX_TOKENS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens){
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIMITERS); //splitting input
    while (token != NULL){
        tokens[position] = token;
        position++;

        //reallocate if buffer too small
        if (position >= bufsize){
            bufsize += MAX_TOKENS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens){
                fprintf(stderr, "allocation error");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIMITERS);
    }
    tokens[position] = NULL;
    return tokens;
}

#include <sys/wait.h>
#include <unistd.h>

int execute(char **args){
    pid_t pid, wpid;
    int status;
    int background = 0;

    //check for &
    int i = 0;
    while (args[i] != NULL){
        i++;
    }
    if (i > 0 && strcmp(args[i-1], "&")== 0){
        background =1;
        args[i-1] = NULL;
    }
    pid = fork(); //creates new process by duplicating the current one
    if (pid == 0){ //if child
        handle_redirection(args);
        if (execvp(args[0], args) == -1){
            perror("mshell");
        }
        exit(EXIT_FAILURE);
        
    } else if (pid < 0){ //if parent
        perror("custom-shell-c");

    } else{ //if smth failed
        if (!background){
            do{
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }else {
            printf("[background pid %d started]\n", pid);
        }
    }
    return 1;
}

int execute_piped(char *line) {
    char *cmds[64];
    int num_cmds = 0;

    char *cmd = strtok(line, "|");
    while (cmd != NULL){
        cmds[num_cmds++] = cmd;
        cmd = strtok(NULL, "|");
    }

    int pipefd[2];
    int in_fd = 0;

    for (int i = 0; i< num_cmds; i++){
        pipe(pipefd);
        pid_t pid = fork();
        if (pid ==0){
            dup2(in_fd, 0);

            if (i < num_cmds -1){
                dup2(pipefd[1],1);
            }

            close(pipefd[0]);
            close(pipefd[1]);

            // split current command into args
            char **args = split_line(cmds[i]);
            handle_redirection(args);
            execvp(args[0], args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else {
            wait(NULL);
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }
    return 1;
}

int dispatch(char **args, char *line){
    if (args[0] == NULL) {
        return 1;
    }

    if (strcmp(args[0], "cd") == 0) return cd(args);
    if (strcmp(args[0], "help") == 0) return help(args);
    if (strcmp(args[0], "exit") == 0) return shell_exit(args);
    return execute(args);
}

int cd(char **args){
    if (args[1] == NULL){
        fprintf(stderr, "expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0){
            perror("custom-shell-c");
        }
    }
    return 1;
}

int help(char **args){
    printf("custom shell in C\n");
    printf("built-in commands:\n");
    printf(" cd [dir]   change directory\n");
    printf(" help       show this help message\n");
    printf(" exit       exit the shell\n");
    printf("other commands are executed as system programs\n");
    return 1;
}

int shell_exit(char **args) {
    return 0;
}

void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\ncustom-shell-c> ", 17);
    fflush(stdout);
}

void handle_redirection(char **args){
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i+1] == NULL){
                fprintf(stderr, "expected filename after '>'\n");
                return;
            }
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644); //open the file for writing
            if (fd < 0){
                perror("custom-shell-c");
                return;
            }
            dup2(fd, STDOUT_FILENO); //redirect
            close(fd);
            args[i] = NULL; // remove '>' and filename from args
    
        }else if (strcmp(args[i], "<") == 0){
            if (args[i+1] == NULL){
                fprintf(stderr, "expected filename after '<'\n");
                return;
            }
            int fd = open(args[i+1], O_RDONLY);
            if (fd < 0){
                perror("custom-shell-c");
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL; // remove '<' and filename from args
        }
        i++;
    }
}

int main(){
    char *line = NULL;
    size_t bufsize = 0;

    signal(SIGINT, sigint_handler); // handle Ctrl+C
    signal(SIGTSTP, SIG_IGN); // ignore Ctrl+Z

    printf("Welcome to mshell! Type 'exit' to quit.\n");

    while(1){
        printf("custom-shell-c>");
        ssize_t bytesRead = getline(&line, &bufsize, stdin);
        if (bytesRead == -1) {
            perror("getline");
            exit(EXIT_FAILURE);
        }

        line[strcspn(line, "\n")] = '\0';

        int status;
        if (strchr(line, '|') != NULL) {
            status = execute_piped(line);  // already executes everything
        } else {
            char **args = split_line(line);
            status = dispatch(args, line); // safe because no pipe
            free(args);
        }

        if (status == 0) {
            printf("byebye!");
            break;
        }
    }  
}