# Software

harishell: A Unix shell written in C++ that supports foreground/background process management, job control and Unix signal handling.

---

## Features

- Execute **real commands** with arguments, anything you'd use in a regular terminal (cd, git, pwd, ...)
- **Stop** and **resume** processes with `stop <pid>` / `cont <pid>` commands
- Run processes in the **background** using `&`
- Suspend the foreground process with `Ctrl+Z` (SIGTSTP)
- Terminate the foreground process with `Ctrl+Z` (SIGINT)
- Error handling for invalid input and signals sent without a running process (setjmp)
- Automatic zombie-process cleanup via `SIGCHLD`
- **logout** command with safety checks for running background processes
- Current working path displayed before each prompt, as in a standard shell
- Background and stopped processes listed after each command for full visibility

---

## Build and run

Requires a Linux or macOS environment. On Windows, use WSL (Windows Subsystem for Linux) or a virtual machine. Clone the repository, navigate to the project folder, then run:
- `g++ myshell.cpp -o myshell` 
- `./myshell`

---

## Signal Handling

| Signal  | Behavior |
|---------|----------|
| SIGCHLD | Reaps zombie processes; removes them from the process list |
| SIGTSTP | Stops the current foreground process (Ctrl+Z) |
| SIGINT | Terminates the current foreground process (Ctrl+C) |

---

## Implementation Notes

- Each child process is placed in its own process group (`setpgid(0, 0)`) so signals don't propagate unintentionally to the shell itself.
- Background processes ignore `SIGHUP` and keep running until explicitly stopped.
- `waitpid` with `WUNTRACED` is used for foreground processes so the shell detects both stops and termination. `WIFEXITED` and `WIFSIGNALED` are checked to remove processes from the list when they are no longer running.
- The process list tracks all active background processes and stopped ones.

---

/*## Limitations

- No support for I/O redirection (`>`, `<`, `|`)
- No built-in command history
- No tab completion
- No GUI

---*/

## Authors and ressources
Antsa Haritiana Ramparany — computer science student at Hochschule Darmstadt (h_da), Germany
