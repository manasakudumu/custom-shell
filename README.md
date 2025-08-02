# Custom Unix Shell in C

This project implements a custom Unix shell in C. It supports command parsing, built-in commands, foreground and background execution, file redirection, piping, and basic signal handling.


## Features

- Command parsing using `strtok` and dynamic memory allocation
- Built-in commands:
  - `cd [dir]` – change directory
  - `help` – show help message
  - `exit` – exit the shell
- Foreground and background process execution (`&`)
- File input/output redirection (`<`, `>`)
- Pipe handling for multi-stage commands (`|`)
- Signal handling for `SIGINT` and `SIGTSTP`




