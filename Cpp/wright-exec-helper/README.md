# Wright execution helper

Launches a number of worker processes and feeds them work to do.

## Process design

In order to get this working on a UNIX system a layered approach is needed. This is due to the insanity that is `fork`. At it's core the execution helper needs to read work to do from some source and distribute it amongst child processes.

The core protocol that it uses is to read jobs from stdin and print the work that has been executed to stdout. Each job is a single line, and the process that is used to execute the jobs reads a line of input and then writes that line back out when it has completed the line.

1. The parent process starts
2. For each child it creates pipes for stdout, stdin and stderr and forks a process supervisor.
3. The parent now reads lines from its stdin and pushes then into the child process pipes.
4. When it reads a line of output from a child pipe it checks to see if it is the line it is expecting. If so, it queues another job for the worker to execute.

The supervisor process assumes that the process that does the work might crash. After a crash the worker needs to be restarted and any outstanding work is given to it again for processing.

