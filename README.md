# NYU-sh: New Yet Usable SHell

## A basic command-line interpreter for Linux OS

## Background

The shell is the main command-line interface between a user and the operating system, and it is an essential part of the daily lives of computer scientists, software engineers, system administrators, and so on. It makes heavy use of many OS features. This project provided a simplified version of the Unix shell called the New Yet Usable SHell, or nyush for short.

## Enviroment

This shell is built on **Linux** with **C99** language.

## Compilation

If you have installed CMake on your Linux System, you can directly run command `make` to compile; Otherwise, you should run command

`gcc nyush.c -std=gnu99 -o nyush`

to compile.
If you compiled successfully, you will see an executable file named **nyush** in current directory.

## Usage

### Start the shell

Type in the command `./nyush`, then you will see the prompt, where the _dir_ is the basename of the current working directory.

```bash
[nyush dir]$ _
```

### Grammar for valid commands

```bash
[command] := ""; or
          := [cd] [arg]; or
          := [exit]; or
          := [fg] [arg]; or
          := [jobs]; or
          := [cmd] '<' [filename] [recursive]; or
          := [cmd] '<' [filename] [terminate]; or
          := [cmd] [recursive]; or
          := [cmd] [terminate] < [filename]; or
          := [cmd] [terminate].

[recursive] := '|' [cmd] [recursive]; or
            := '|' [cmd] [terminate].

[terminate] := ""; or
            := '>' [filename].
            := '>>' [filename].

[cmd] := [cmdname] [arg]*

[cmdname] := A string without any space, tab, > (ASCII 62), < (ASCII 60), | (ASCII 124), * (ASCII 42), ! (ASCII 33), ` (ASCII 96), ' (ASCII 39), nor " (ASCII 34) characters. Besides, the cmdname is not cd, exit, fg, jobs.

[arg] := A string without any space, tab, > (ASCII 62), < (ASCII 60), | (ASCII 124), * (ASCII 42), ! (ASCII 33), ` (ASCII 96), ' (ASCII 39), nor " (ASCII 34) characters.

[filename] := A string without any space, tab, > (ASCII 62), < (ASCII 60), | (ASCII 124), * (ASCII 42), ! (ASCII 33), ` (ASCII 96), ' (ASCII 39), nor " (ASCII 34) characters.
```

If there is any error in parsing the command, then the shell will print the following error message to STDERR and prompt for the next command.

```bash
Error: invalid command
```

### Locating programs

You can specify a program by either an absolute path, a relative path, or base name only. If one specifies only the base name without any slash ```/```, then the shell will search for the program under ```/bin``` and ```/usr/bin``` (in such order). In any case, if the program cannot be located, the shell will print the following error message to STDERR and prompt for the next command.

```bash
Error: invalid program
```

### Signal handling

If a user presses Ctrl-C or Ctrl-Z , they will be ingored by the shell but will not be ingored by the child process created by shell. For example:

```bash
[nyush dir]$ cat
^C
[nyush dir]$ _
```

### I/O redirection

- Input redirection ```<``` is supported.
- Output redirection  ```>``` and ```>>``` are supported.
- Pipe ```|``` is supported.

### Built-in commands

#### ```cd``` command

```bash
[nyush dir]$ cd [dir]
```

This command changes the current working directory of the shell. It takes exactly one argument: the directory, which may be an absolute or relative path. If the directory does not exist, the shell will print the following error message to STDERR and prompt for the next command.

```bash
Error: invalid directory
```

#### ```jobs``` command

```bash
[nyush dir]$ jobs
```

This command prints a list of currently suspended jobs to STDOUT in the following format:

 ```bash
[index] command
```

For example:

```bash
[nyush dir]$ jobs
[1] cat
[2] top | cat
[3] cat > output.txt
[nyush dir]$ _
```

#### ```fg``` command

```bash
[nyush dir]$ fg [index]
```

This command resumes a job in the foreground.

For example:

```bash
[nyush dir]$ jobs
[1] cat
[2] top | cat
[3] cat > output.txt
[nyush dir]$ fg 2
```

If the job index does not exist in the list of currently suspended jobs, the shell will print the following error message to STDERR and prompt for the next command.

```bash
Error: invalid job
```

#### ```exit``` command

```bash
[nyush dir]$ exit
```

This command terminates the shell. However, if there are currently suspended jobs, the shell will not terminate. Instead, it will print the following error message to STDERR and prompt for the next command.

```bash
Error: there are suspended jobs
```

## Acknowledgement

This project came from course **Operating Systems (CSCI-GA.2250-002)** in **NYU**, offered by **Prof. Yang Tang**.
