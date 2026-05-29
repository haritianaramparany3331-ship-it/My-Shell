# myshell

A minimal Unix shell written in C++ that supports foreground/background process management and Unix signal handling.

---

## Features

- Execute arbitrary programs with arguments
- Run processes in the **background** using `&`
- **Stop** and **resume** processes with `stop` / `cont` commands
- Suspend the foreground process with `Ctrl+Z` (SIGTSTP)
- Automatic zombie-process cleanup via `SIGCHLD`
- **logout** command with safety checks for running background processes
- Live process list displayed after each command

---

## Build and debug

Only in Linux/MacOS
- `g++ myshell.cpp -o myshell` 
- `./myshell`

---

### Run a program

You can try this:
```
myshell> firefox
^Z [SIGTSTP: pid 5613]
myshell> cont 5613
^Z [SIGTSTP: pid 5613]
myshell> cont 5613
...end here the foreground process 5613
myshell> firefox &
[Hintergrund: 5622]
myshell> stop 5622
myshell> cont 5622
myshell> logout
logout not possible. The process PIDs: 5622 is still running in the background.
... end here the process 5622 ...
myshell> logout 
Do you really want to logout (Y/N)? >
```

---

## Signal Handling

| Signal  | Behavior |
|---------|----------|
| SIGCHLD | Reaps zombie processes; removes them from the process list |
| SIGTSTP | Stops the current foreground process (Ctrl+Z) |

---

## Implementation Notes

- Each child process is placed in its own process group (`setpgid(0, 0)`) so signals don't propagate unintentionally to the shell itself.
- Background processes ignore `SIGHUP`.
- `waitpid` with `WUNTRACED` is used for foreground processes so the shell detects stops as well as termination.
- The process list is stored as a `std::vector<Prozess>` and printed after each command.

---

## Limitations

- No support for I/O redirection (`>`, `<`, `|`)
- No built-in command history
- No tab completion
- `cont` on a background process sends SIGCONT but does not bring it to the foreground
