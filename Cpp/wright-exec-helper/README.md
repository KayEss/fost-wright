# Wright execution helper

Launches a number of worker processes and feeds them work to do.


## Process design

In order to get this working on a UNIX system a layered approach is needed. This is due to the insanity that is `fork`. At it's core the execution helper needs to read work to do from some source and distribute it amongst child processes.

The core protocol that it uses is to read jobs from stdin and print the work that has been executed to stdout. Each job is a single line, and the process that is used to execute the jobs reads a line of input and then writes that line back out when it has completed the line.

1. The parent process starts. This top level process is called the manager.
2. For each child it creates pipes for stdout, stdin and stderr and forks a process supervisor.
3. The process supervisor starts the correct worker process. By default a work simulator is run.
4. The parent now reads lines from its stdin and pushes then into the child process pipes.
5. When it reads a line of output from a child pipe it checks to see if it is the line it is expecting. If so, it queues another job for the worker to execute.

The supervisor process assumes that the process that does the work might crash. After a crash the worker needs to be restarted and any outstanding work is given to it again for processing.

## Using `wright-exec-helper`

You must pipe jobs into `wright-exec-helper`, but it can still be run from the command line for testing.

    cat /usr/share/dict/words | wright-exec-helper > output.txt

This will run the program in self test mode, where it also simulates failures of the workers at quite a high rate. You'll see output like this:

    Wright execution helper
    Copyright (c) 2016-2017, Felspar Co. Ltd.
    2018-05-22T05:13:00.026729Z warning wright/exec-helper/5
                                Child stderr
    {
        "child" : 6701,
        "stderr" : "Crash after work... 6721"
    }

    2018-05-22T05:13:00.652127Z warning wright/exec-helper/7
                                Child stderr
    {
        "child" : 6703,
        "stderr" : "Crash during work... 6723"
    }


Informative and log messages are printed to `stderr` and the completed jobs are printed to `stdout`. After running the jobs that have completed are listed in `output.txt`.


### The Manager

By default the manager will run in self-test mode. To do anything else you must speicfy a `-x` option to set the command line for the correct worker process.

* `-x [:json-array]` -- Set the command line options for the child worker process.
* `-w :children` -- The count for the number of worker processes wanted. This value [cannot currently be zero](https://github.com/KayEss/fost-wright/issues/1), even a network server must have at least one local worker.


#### Networked management

There are two options that are used to control networked behaviour:

* `-p :port` -- The port number to use
* `-c :hostname` -- The host to connect to

The manager process will run in either server or client mode. To run in server mode use the `-p` option but not the `-c` option. This will tell the server the port number to listen for client connections from.

The client should then connect to the server using both the `-p` and the `-c` options.

Note that the client will not receive any configuration form the server. The client is not told the server's `-x` option, which must be specified. The client will also need its own `-w` to control the number of children if the default is not wanted.

More than one networked client can be used. If the networked client dies for any reason, or the network connection is lost, then the outstanding work for that client is redistributed amongst the other clients and local workers.


### The Work Simulator


The work simulator reads a job and then sleeps for a period of time, to simulate the time taken to execute the job, and then prints the job line back out. By default it will also exit (to simulate a crash) at random points.

The default work time is 1,000 milliseconds with a standard deviation of 500. There is about a 16% chance that the worker exits immediatly after it sleeps (before reporting the work complete) and if it survives, a 16% chance that it exists after reporting the work complete.

Although the sleep times are normally distributed, the process can't sleep for a negative amount of time so the distribution of actual work times won't be normally distributed.

* `--simulate true` -- Must be set to enable the work simulation.
* `-d false` -- Turn off crash simulation for the work simulation.
* `--sim-mean :ms` -- Number of milliseconds to use for the mean of the sleep normal distribution.
* `--sim-sd :sd` -- Number for the standard deviation for the normal distribution used by the sleep.


### The Process Supervisor

The process supervisor is started by `wright-exec-helper` to manage the worker processes. If you look at the process table you will find these processes and the options listed below are documented here to help you understand what is going on.

* `--child :number` -- Sets the child number. Children numbers start at one (child zero is the manager).
* `-b false` -- Turns the banner display off.
* `-rfd :fd` -- Sets the file descriptor that the logging messages are passed to the manager with.
* `-x :command` -- A JSON array specifying the command  line for the worker. For a typical simulated worker this might look like:
        ["bin/wright-exec-helper","--simulate","true","-b","false"]

