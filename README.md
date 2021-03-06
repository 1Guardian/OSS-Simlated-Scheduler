# OSS-Simulated-Scheduler

## Notes

config.h contains all data-structures and contains the configurable #define 
variables. There are many shared memory segments and the program relies on 
all of them to run.

Inside of the log file, there are a few different messages that can appear.
The first of them is a dispatch message which looks like:

    OSS: Dispatching process with PID 0 from queue 0 at time 0:0
    OSS: Total time this dispatch was 2805 nanoseconds

There are also returned log messages which show information about how
long a process ran and what the determination of the run was: "B" for blocked
and "T" for terminated

    OSS: Process 0 has returned with status: B and ran for 3126171ns

The final portion of the logging messages are the blocked messages. 
The program logs what was done in the blocked queues, and whether or not
the OSS scheduler actually had to schedule things from it, or just checked
through the queue. If it just checked, the second message below is not displayed.

    OSS: Process 0 has been moved from blocked to low-priority
    OSS: Total Time spent checking the blocked queues and readying processes:  883902


# OSS and TestSim (Test Process): Overview

OSS has many different functions built into it to manage various aspects of 
the program. The bulkiest function is the schedulingProcess() function which
handles all of the scheduling and is in-charge of calling the function that creates
new processess as well. It also checks and manages the blocked queue on each pass
through both the high and low priority queues. There are two aditional threads 
one that watches the children return and makes sure to allow them to exit without
being stuck in <destruc>, and a second which manages the input maximum time. When that
time elapses, all shared memory is cleaned up and the children are signaled to exit.

Testsim is the test user-process binary which is being scheduled by OSS. Upon creation it
determines whether or not it is IO bound or CPU bound, and then determines it's PID
by looking at a shared semaphore which has controlled access by two message queues and 
retains mutual exclusion by the fact that only one child can ever be in the process of
looking for it's PID at a time. It then rests until the semaphore is set back to the 
PID of the child, at which time it sets the semaphore to 0 and 'runs' either terminating 
or getting blocked, and generating a random amount of time that it ran. It then sends this
information back to the OSS and waits or exits.


# Getting Started (Compiling)

To compile both the runsim and testsim programs, simple use of 'make'
will compile both for your use. 'make clean' will remove the created object files and the
executable binary that was compiled using the object files
