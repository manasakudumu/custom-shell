# Custom Unix Shell in C

This project implements a custom Unix shell in C. It supports command parsing, built-in commands, background and foreground execution, file redirection, piping, job control, and basic signal handling.

## Features

- Command parsing using `strtok` and dynamic memory allocation
- Built-in commands:
  - `cd [dir]` – change directory
  - `help` – show help message
  - `exit` – exit the shell
  - `jobs` – list background/stopped jobs
  - `fg %jobid` – bring background job to foreground
  - `bg %jobid` – resume a stopped job in the background
- Foreground and background process execution using `&`
- File input/output redirection using `<` and `>`
- Pipe handling for multi-stage commands using `|`
- Job management using a linked list for background/stopped jobs
- Signal handling:
  - `SIGINT` (Ctrl+C): prevents shell from exiting
  - `SIGTSTP` (Ctrl+Z): marks a job as stopped and displays a message

## Compilation

```bash
gcc -o custom-shell-c src/main.c
