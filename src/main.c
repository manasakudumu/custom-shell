#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_TOKENS 64
#define DELIMITERS " \t\r\n\a"

typedef struct Job {
    pid_t pid;
    char command[1024];
    int job_id;
    int is_stopped;
    struct Job *next;
} Job;

Job *job_list = NULL;
int next_job_id = 1;

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

//handles input/output redirection (>, <)
void redirect(char **args){
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

//adds a background/stopped job to the job list
void add_job(pid_t pid, const char *cmd, int stopped){
    Job *new_job = malloc(sizeof(Job));
    new_job->pid = pid;
    new_job->job_id = next_job_id++;
    strncpy(new_job->command, cmd, 1023);
    new_job->is_stopped = stopped;
    new_job->next = job_list;
    job_list = new_job;
}

// finds a job by its id
Job* find_job(int id){
    Job *curr = job_list;
    while (curr){
        if (curr->job_id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

// removes a job by its id
void remove_job(pid_t pid){
    Job *curr = job_list;
    Job *prev = NULL;
    while (curr){
        if (curr->pid == pid){
            if (prev){
                prev->next = curr->next;
            } else {
                job_list = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

//changes current working directory
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

//shows help text for builtin commands
int help(char **args){
    printf("custom shell in C\n");
    printf("built-in commands:\n");
    printf(" cd [dir]   change directory\n");
    printf(" help       show this help message\n");
    printf(" exit       exit the shell\n");
    printf("other commands are executed as system programs\n");
    return 1;
}

// exits the shell
int shell_exit(char **args) {
    return 0;
}

// executes foreground/background command
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
    pid = fork();
    if (pid == 0){ //if child
        redirect(args);
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
            char full_cmd[1024] = "";
            for (int j = 0; args[j] != NULL; j++) {
                strcat(full_cmd, args[j]);
                strcat(full_cmd, " ");
            }
            add_job(pid, args[0], 0);
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
            redirect(args);
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

// ctrl+c handler (avoids shell termination)
void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\ncustom-shell-c> ", 17);
    fflush(stdout);
}

// handles ctrl+z for job stopping
void sigtstp_handler(int sig) {
    printf("\n[stopped]\n");
    fflush(stdout);
}

// checks for builtin vs external
int dispatch(char **args, char *line){
    if (args[0] == NULL) {
        return 1;
    }
    if (strcmp(args[0], "cd") == 0) return cd(args);
    if (strcmp(args[0], "help") == 0) return help(args);
    if (strcmp(args[0], "exit") == 0) return shell_exit(args);
    if (strcmp(args[0], "jobs") == 0) {
        Job *curr = job_list;
        while (curr) {
            printf("[%d] %s %s\n", curr->job_id,
                curr->is_stopped ? "stopped" : "running",
                curr->command);
            curr = curr->next;
        }
        return 1;
        }
    if (strcmp(args[0], "fg") == 0) {
        if (!args[1] || args[1][0] != '%') {
            fprintf(stderr, "usage: fg %%jobid\n");
            return 1;
        }
        int job_id = atoi(args[1]+1);
        Job *job = find_job(job_id);
        if (!job) {
            fprintf(stderr, "no such job\n");
            return 1;
        }
        kill(job->pid, SIGCONT);
        waitpid(job->pid, NULL, 0);
        remove_job(job->pid);
        return 1;
    }
    if (strcmp(args[0], "bg") == 0) {
        if (!args[1] || args[1][0] != '%') {
            fprintf(stderr, "Usage: bg %%jobid\n");
            return 1;
        }
        int job_id = atoi(args[1]+1);
        Job *job = find_job(job_id);
        if (!job) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
        kill(job->pid, SIGCONT);
        job->is_stopped = 0;
        printf("[%d] resumed %s\n", job->job_id, job->command);
        return 1;
    }
    return execute(args);
}

int main(){
    char *line = NULL;
    size_t bufsize = 0;
    signal(SIGINT, sigint_handler); 
    signal(SIGTSTP, sigtstp_handler);
    printf("welcome, type 'exit' to quit\n");
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
            status = execute_piped(line); 
        } else {
            char **args = split_line(line);
            status = dispatch(args, line); // no pipe
            free(args);
        }
        if (status == 0) {
            break;
        }
    }  
}