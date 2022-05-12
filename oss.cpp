/***********************************************************************
OSS code for project 4:

Author: Tyler Martin
Date: 11/4/2021

- OSS is the Operating System Simulator. It acts as an OS and Scheduler
  with scheduling implemented via a round robin algorithm with priority
  and one blocked queue. It takes a few parameters to control the max
  running time and log file enumeration. 2 Other variables are controllable
  from config.h that control the max spawn time and max spawn number of the
  child processes. It has many message queues and shared objects that control
  the communication between this program and the child processes. It starts 
  by creating the queues and spawning the first child. It sends a message 
  with a message containing the PID of the new child to the new child. The
  child logs it's own PID and returns the message. From there the Round Robin
  Schedulign starts. Both the low and high priority queues are treated equally
  and both get the same running time (if there are processes ready). Processes
  are told to start running by setting the shared semaphore to the PID of the desired
  process. That process then sees the semaphore being set, and does it's execution.
  It then sets the semaphore to 0 and messages the OSS again with the details
  of it's execution. If it needs to be blocked, the OSS places it in blocked and 
  repeats until it can be freed, whilst also managing other processes. Priority
  is given depending on what type of block is needed. IF the child requests
  and IO device, it is placed in high priority and if it needs a resource that
  the cpu delegates, it is placed in low priority (all simulated of course, 
  the actual request is a single character 'C' or 'I' in the message). This continues
  until all child processes are handled. Averages are then printed out and the program
  exits. IMPORTANT-NOTE: the time quantum I defined for the children to use was 500000000.
  So usage times were between 0-500000000ns for execution.

Testing steps:
Program allocates shared memory and message queues. It then creates two 
additional threads and begins forking children while running a round robin
scheduler until a termination is signaled due to force, running out of processes
or time elapsing. It logs all events while it executes and prints general
statistics at the end of the program's execution.
***********************************************************************/
#include <stdio.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <algorithm>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <chrono>
#include <random>
#include <vector>
#include <thread>
#include <tuple>
#include "sys/msg.h"
#include "string.h"
#include "config.h"

//key for address location
#define SHM_KEY 0x98273
#define PARENT_KEY 0x94512
#define PCT_KEY 0x92612
#define TURN_KEY 0x95712
#define KEY 0x00043 //child-to-parent key
#define CLOCK_KEY 0x05912
#define PCB_KEY 0x46781
#define RR_KEY 0x46583
#define SCHEDULER_KEY 0x46513
#define FINAL_STATS_KEY 0x396561


bool amChild = false;

//Struct object for final stats
struct STATS finalStats; 

//argv[0] perror header
std::string exe;

//message object
struct message msg;

//logfile name and max exec time
std::string logfile = "logfile";
int maxExecutionTime = 120;

/***********************************************************************
 create shared message queues 

 Queues: 
        Queue for child-to-parent communication
        Queue for parent check-up
        Queue for all-to-clock communications
        Queue for PCB access
        Queue for talking to scheduler
***********************************************************************/
int msgid = msgget(KEY, 0666 | IPC_CREAT); 

int parent_msgid = msgget(PARENT_KEY, 0666 | IPC_CREAT);

int clock_msgid = msgget(CLOCK_KEY, 0666 | IPC_CREAT); 

int PCB_msgid = msgget(PCB_KEY, 0666 | IPC_CREAT); 

int scheduler_msgid = msgget(SCHEDULER_KEY, 0666 | IPC_CREAT); 

/***********************************************************************
 create shared message memory segments 

 Objects:
        Clock Object for time management
        PCT object for process management
        Turn Management Semaphore for starting children
        The Round Robin Queues
        The PIDS vector (made shared even though only runsim wil ever use it
                         so that all threads see it)
        Struct that contains the final stats that the children
                         will log to
***********************************************************************/
int shmid = shmget(SHM_KEY, sizeof(struct Clock), 0600|IPC_CREAT);
struct Clock *currentClockObject = (static_cast<Clock *>(shmat(shmid, NULL, 0)));

int PCT_shmid = shmget(PCT_KEY, sizeof(struct PCT), 0600|IPC_CREAT);
struct PCT *currentProcessTable = (static_cast<PCT *>(shmat(PCT_shmid, NULL, 0)));

int Turn_shmid = shmget(TURN_KEY, sizeof(struct TurnSemaphore), 0600|IPC_CREAT);
struct TurnSemaphore *currentTurnSemaphore = (static_cast<TurnSemaphore *>(shmat(Turn_shmid, NULL, 0)));

int RR_shmid = shmget(RR_KEY, sizeof(struct RR), 0600|IPC_CREAT);
struct RR *RRQueues = (static_cast<RR *>(shmat(RR_shmid, NULL, 0)));

int PIDSVec_shmid = shmget(/*key defined here cause it isn't needed by any other process*/0x571254, sizeof(struct PIDSVector), 0600|IPC_CREAT);
struct PIDSVector *PIDSVec = (static_cast<PIDSVector *>(shmat(PIDSVec_shmid, NULL, 0)));

int Final_Log_shmid = shmget(FINAL_STATS_KEY, sizeof(struct SHARED_STATS), 0600|IPC_CREAT);
struct SHARED_STATS *currentSharedStats = (static_cast<SHARED_STATS *>(shmat(Final_Log_shmid, NULL, 0)));

/***********************************************************************
Function: docommand(char*)

Description: Function to call the grandchildren 
             (testsim). Uses execl to open a bash
             instance and then simply run the 
             program by providing the name of the bin
             and the relative path appended to the 
             beginning of the command.
***********************************************************************/
void docommand(char* command){

    //add relative path to bin
    std::string relativeCommand = "./";
    relativeCommand.append(command);

    //execute bin
    execl("/bin/bash", "bash", "-c", &relativeCommand[0], NULL);
}

/***********************************************************************
Function: deleteMemory(char*)

Description: Function to delete all shared memory
***********************************************************************/
void deleteMemory(){

    //delete each memory/queue share
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);
    shmctl(PCT_shmid, IPC_RMID, NULL);
    shmctl(PIDSVec_shmid, IPC_RMID, NULL);
    msgctl(parent_msgid, IPC_RMID, NULL);
    msgctl(clock_msgid, IPC_RMID, NULL);
    msgctl(PCB_msgid, IPC_RMID, NULL);
    shmctl(Turn_shmid, IPC_RMID, NULL);
    msgctl(scheduler_msgid, IPC_RMID, NULL);
    shmctl(RR_shmid, IPC_RMID, NULL);
    shmctl(Final_Log_shmid, IPC_RMID, NULL);
}

/***********************************************************************
Function: printFinalStats(struct STATS)

Description: Function that prints the final statistics after
             the program is finished running. It prints it in 
             a table format for easy reading and displays all
             values in a seconds-nanoseconds format
***********************************************************************/
void printFinalStats(struct STATS finalStats)
{
    //print final stats out 
    std::cout << std::endl << "                       Final Running Statistics" << std::endl;
    std::cout << "========================================================================" << std::endl;
    std::cout << "CPU STATS:" << std::endl;
    std::cout << "----------" << std::endl;
    std::cout << "Total Time Spent Idling: " << finalStats.totalIdleTime/1000000000 << " seconds and " << finalStats.totalIdleTime%1000000000 << " nanoseconds" << std::endl << std::endl;
    std::cout << "CPU BOUND PROCESS STATS:" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << currentSharedStats->getAverageTimeInSystem(true) << std::endl;
    std::cout << currentSharedStats->getAverageWaitTime(true) << std::endl;
    std::cout << currentSharedStats->getAverageCPUUsageTime(true) << std::endl;
    std::cout << currentSharedStats->getAverageTimeBlocked(true) << std::endl << std::endl;
    std::cout << "IO BOUND PROCESS STATS:" << std::endl;
    std::cout << "-----------------------" << std::endl;
    std::cout << currentSharedStats->getAverageTimeInSystem(false) << std::endl;
    std::cout << currentSharedStats->getAverageWaitTime(false) << std::endl;
    std::cout << currentSharedStats->getAverageCPUUsageTime(false) << std::endl;
    std::cout << currentSharedStats->getAverageTimeBlocked(false) << std::endl;
}

/***********************************************************************
Function: threadReturn()

Description: Function that a thread will inhabit with
             the sole purpose and intent of keeping 
             track of which child processes are still
             running
***********************************************************************/
void threadReturn()
{
    while(1){
        if(waitpid(-1, NULL, WNOHANG) > 0)
            true;
    }
}

/***********************************************************************
Function: cleanUp(bool)

Description: Function that receives control of the OSS program
             when the main scheduler is finished. It's job is to
             cleanup and delegate the deletion of shared memory, and make
             sure that no zombie processes are found.
             (Functionally a copy of threadkill)
***********************************************************************/
void cleanUp(bool show = true)
{
    //wait for children that are still de-coupling
    while(wait(NULL) > 0);

    //kill all children
    for(int i=0; i < PIDSVec->PIDS.size(); i++)
        kill(std::get<0>(PIDSVec->PIDS[i]), SIGTERM);

    //show final stats
    if(show)
        printFinalStats(finalStats);

    //clear memory 
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: siginthandler(int)

Description: Function that receives control of
             termination when program has received 
             a termination signal
***********************************************************************/
void siginthandler(int param)
{
    printf("SigInt Received. Deallocating all memory and exiting.\n\n");
    printf("Final Stats being printed, but keep in mind that execution\n");
    printf("was interrupted, likely skewing the numbers.\n\n");

    //kill all children
    for(int i=0; i < PIDSVec->PIDS.size(); i++)
        kill(std::get<0>(PIDSVec->PIDS[i]), SIGTERM);

    //show final stats
    printFinalStats(finalStats);

    //clear memory
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: threadKill(std::chrono::steady_clock::time_point)

Description: Function that receives control of
             termination when program has received 
             a termination signal from the time 
             contained in config.h being ellapsed.
***********************************************************************/
void threadKill(std::chrono::steady_clock::time_point start, int max_run_time)
{
    //don't execute until time has ellapsed.
    while((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < maxExecutionTime) && amChild != true);

    //signal both scheduler and all children that exit time has arrived
    currentTurnSemaphore->setTurn(-1);

    //finish cleaning up and exit
    cleanUp();
}

/***********************************************************************
Function: printHelp()

Description: Prints the Help message triggered by the use of the -h
flag and aborts execution when printing is complete.

***********************************************************************/
void printHelp(){
    std::cout << "Operating System Simulator: " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Execution Method: ./oss <command switches> " << std::endl;
    std::cout << "Switch Options:" << std::endl;
    std::cout << "[-h] :- Displays the 'help' message" << std::endl;
    std::cout << "[-s t] :- Designates how long the program should be allowed to run" << std::endl;
    std::cout << "[-l f]:- Writes the output to the designated logfile" << std::endl;
    cleanUp(false);
}

/***********************************************************************
Function: isReadyToUnblock(std::string)

Description: Function that splits the timestamp for
             blocked usage and currrent clock and
             checks if the blocked process is ready
             to continue
***********************************************************************/
bool isReadyToUnblock(std::string stamp){

    //get current stamp
    std::string currentStamp = currentClockObject->getTimeStamp();

    //convert the colons to spaces for ss
    std::replace(stamp.begin(), stamp.end(), ':', ' ');
    std::replace(currentStamp.begin(), currentStamp.end(), ':', ' ');

    //ss operators and holding variables
    std::vector<unsigned int> array;
    std::vector<unsigned int> array2;
    std::stringstream ss(stamp);
    std::stringstream css(currentStamp);
    unsigned int temp;

    //read string into vector
    while (ss >> temp)
        array.push_back(temp);
    
    //read string into vector
    while (css >> temp)
        array2.push_back(temp);

    //return value based on whether time has elapsed
    return (array[0] < array2[0] || array[0] == array2[0] && array[1] <= array2[1]);
}

/***********************************************************************
Function: updateFinalBlockedStats(std::stamp, struct STATS, int)

Description: Function that updates the blocked portion of the 
             finalStats object that is shown at the end of
             execution.
***********************************************************************/
void updateFinalBlockedStats(std::string stamp, struct STATS *finalStats, int priority){

    //get current stamp
    std::string currentStamp = currentClockObject->getTimeStamp();

    //convert the colons to spaces for ss
    std::replace(stamp.begin(), stamp.end(), ':', ' ');
    std::replace(currentStamp.begin(), currentStamp.end(), ':', ' ');

    //ss operators and holding variables
    std::vector<unsigned int> array;
    std::vector<unsigned int> array2;
    std::stringstream ss(stamp);
    std::stringstream css(currentStamp);
    unsigned int temp;

    //read string into vector
    while (ss >> temp)
        array.push_back(temp);
    
    //read string into vector
    while (css >> temp)
        array2.push_back(temp);

    //make the time vectors a bit more usable
    //(seconds then nanoseconds always in handling)
    array.push_back(array2[0] - array[0]);
    array.push_back(array2[1] - array[1]);
    array.push_back((array[2] * 1000000000) + array[3]);

    //record based on priority number
    if(priority == 1){
        finalStats->averageTimeInBlockedIO += array[4];
        finalStats->averageTimeInBlockedIOUpdated += 1;
    }
    else{
        finalStats->averageTimeInBlockedCPU += array[4];
        finalStats->averageTimeInBlockedCPUUpdated += 1;
    }
    
}

/***********************************************************************
Function: convertTimeStamp(std::string)

Description: Function that splits the timestamp for
             blocked usage
***********************************************************************/
unsigned int convertTimeStamp(std::string stamp, int choice){

    //convert the colons to spaces for ss
    std::replace(stamp.begin(), stamp.end(), ':', ' ');

    //ss operators and holding variables
    std::vector<unsigned int> array;
    std::stringstream ss(stamp);
    unsigned int temp;

    //read string into vector
    while (ss >> temp)
        array.push_back(temp);

    //return value based on desired requirement
    return (choice == 1)*(array[0]) + (choice == 2)*(array[1]);
}

/***********************************************************************
Function: childProcess(std::string, pid_t, int)

Description: Function that executes docommand and 
             waits for child process to finish.
***********************************************************************/
void childProcess(std::string currentCommand, pid_t wpid, int status){
    //child code
    amChild = true;

    //run docommand
    docommand(&currentCommand[0]);
}

/***********************************************************************
Function: initTable(int child_pid)

Description: Function that initializes the process control table
             for the new process which has just confirmed it's creation
***********************************************************************/
void initTable(int child_pid, int count){

    //since initialization passed, setup a PCT PCB for the new child
    //ask for permission
    if (msgrcv(PCB_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        std::cout << "stuck waiting for permission to look at PCB" << std::endl;
        perror(&exe[0]);
    }

    //set an empty PCB
    for(int i =0; i < 19; i++){
        if(currentProcessTable->Process_Table[i].getPid() == 0)
            currentProcessTable->Process_Table[i].setPid(child_pid);
    }

    //Since initialization passed, push pid to stack
    PIDSVec->PIDS.push_back(std::make_tuple (child_pid, count));

    //cede access to the PCB back
    if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Can't cede control back to worker from PCB thread!\n";
        perror(&exe[0]);
    }

    //push the new process into the RR queues for the scheduler to handle
    RRQueues->high_priority.push_back(child_pid);
}

/***********************************************************************
Function: scheduleNewProcess(pid_t child_pid)

Description: Function that executes the scheduling initilization duties
             creates children as well as assigns pids and
             waits for confimation from children
***********************************************************************/
void scheduleNewProcess(pid_t* child_pid, pid_t* wpid, int* status, int* count){

    //keep going until we hit max allowed processes (50 default)
    if(*count < MAX_PROCESSES){ 

        //don't let more than 20 processes run at any time
        if(PIDSVec->PIDS.size() < 18){
            //make a child to test
            //check to see if process is child or parent
            if((*child_pid = fork()) < 0){
                perror(&exe[0]);
            }
            if (*child_pid  == 0) {
                //start child process
                childProcess("testsim", *wpid, *status);
            }

            //Fill message with PID
            std::sprintf(msg.text, "%d", *child_pid);

            //Parent send message to child with PID enclosed
            if(msgsnd(msgid, &msg, strlen(msg.text)+1, 0) == -1){
                std::cout << "Message Failed to send!\n";
                perror(&exe[0]);
            }
            //wait for confirmation
            if (msgrcv(parent_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                std::cout << "Getting stuck at the Schedule New Process" << std::endl;
                perror(&exe[0]);
            }

            //init the table entry
            initTable(*child_pid, *count);

            //update count
            *count = *count + 1;
            
        }
    }
}

/***********************************************************************
Function: schedulingProcess()

Description: Function that actually handles all scheduling. It has a direct
             line to the child functions, and manages the popping of PIDS
             off of the PID vector, as well as clearing entries within the 
             PCB and making sure they are available in the PCT for the parent
             to enumerate later. Also handles the RR scheduling algorithm.
***********************************************************************/
void schedulingProcess(pid_t* child_pid, pid_t* wpid, int* instatus){
    //hold status && PID
    int tmppid, maskedPID;
    bool flipbit = true;
    int count = 0;

    //more modern use of C++11
    //random seeds and generators
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> blockedRand(1.0, 1000000.0);
    std::uniform_real_distribution<double> randomTimeTaken(1.0, 10000.0);
    std::uniform_real_distribution<double> timeToNewProcess(0.0, 2.0);

    //variables to write to output file
    std::ofstream outdata;
    outdata.open(logfile);

    //string to store return information from child process
    std::string status;

    //sentinel booleans that control access to various 
    //escapes or loops 
    bool none = true;
    bool hasStarted = false;
    bool timerElapsed = false;

    //variable to hold the timer before a new process is allowed to spawn
    double spawnTime = timeToNewProcess(mt);
    double previousSpawnedTime =0;

    //initial starting time for use during averages and timing the spawning of children
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    while(1){  
        //if it's time to end, return
        if (RRQueues->blocked.empty() && RRQueues->high_priority.empty() && RRQueues->low_priority.empty() && hasStarted && timerElapsed){
            outdata << "OSS: All processes Handled. Exiting." << std::endl;

            //signal any zombies that it's time to exit
            currentTurnSemaphore->setTurn(-1);
            return;
        }

        //reset the boolean that checks whether or not we are just waiting
        //(if we are, we need to add a larger time slice because the simulator
        //needs to have time advance so that we get our blocked queues ready to run again)
        none = true;

        //schedule new process if possible 
        //condition that prevents even entering is if time has elapsed
        if(spawnTime + previousSpawnedTime < currentClockObject->getFractionalStamp()){
            if((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() <= MAX_INITIALIZATION_TIME)){
                previousSpawnedTime = currentClockObject->getFractionalStamp();
                scheduleNewProcess(child_pid, wpid, instatus, &count);
            }
            else{
                //if timer is in excess of 3 seconds, stop spawning processes
                timerElapsed = true;
            }
        }   

        ///////////////////////////////////////////
        //Blocked Manager
        ///////////////////////////////////////////     
        
        //if blocked queue is not empty, then check it, otherwise, don't waste time
        if(!RRQueues->blocked.empty()){
            bool worked = false;

            //check if any of the processes are ready to unblock
            for (int i =0; i < RRQueues->blocked.size(); i++){
                if(isReadyToUnblock(RRQueues->blockedTimers[i])){
                    std::string stamp = RRQueues->blockedTimers[i];
                    //unblock and add to correct array
                    //remove process from RR queue
                    tmppid = RRQueues->blocked[i];
                    maskedPID = PIDSVec->getMask(tmppid);
                    RRQueues->blocked.erase(RRQueues->blocked.begin()+i);
                    RRQueues->blockedTimers.erase(RRQueues->blockedTimers.begin()+i);

                    //add to correct queue based on priority
                    for(int i =0; i < 19; i++){
                        if(currentProcessTable->Process_Table[i].getPid() == tmppid){
                            worked = !worked;
                            //update final stats and log the unblock and actually move it
                            if(currentProcessTable->Process_Table[i].getPriority() == 1){
                                RRQueues->high_priority.push_back(tmppid);
                                updateFinalBlockedStats(stamp, &finalStats, currentProcessTable->Process_Table[i].getPriority());
                                outdata << "OSS: Process "<< maskedPID << " has been moved from blocked to high-priority" << std::endl;
                                }
                            else {
                                RRQueues->low_priority.push_back(tmppid);
                                updateFinalBlockedStats(stamp, &finalStats, currentProcessTable->Process_Table[i].getPriority());
                                outdata << "OSS: Process "<< maskedPID << " has been moved from blocked to low-priority" << std::endl;
                                }
                        }
                        break;
                    }
                }
            }

            //update the Time Stamp (only say spent time readying processes if 'worked' has actually been set)
            if(!worked){
                unsigned int timeTaken = rand() % 100 + 0;
                currentClockObject->setTimeStamp(timeTaken);
                finalStats.totalIdleTime += timeTaken;
            }
            else{
                unsigned int timeTaken = blockedRand(mt);
                currentClockObject->setTimeStamp(timeTaken);
                outdata << "OSS: Total Time spent checking the blocked queues and readying processes:  " << timeTaken << std::endl;
            }
        }

        ///////////////////////////////////////////
        //Actual Scheduling Part of the Algorithm
        ///////////////////////////////////////////

        if(!(RRQueues->high_priority.empty())){
            //let sentinel know that we actually started working
            hasStarted = true;
            
            //show that we did work
            none = false;

            //let the first child run
            while(!currentTurnSemaphore->getTurn(0));
            currentTurnSemaphore->setTurn(RRQueues->high_priority[0]);

            //get mask for PID
            maskedPID = PIDSVec->getMask(RRQueues->high_priority[0]);

            //print scheduled notification
            unsigned int timeTaken = randomTimeTaken(mt);
            srand (time(NULL));
            outdata << "OSS: Dispatching process with PID " << maskedPID << " from queue 0 at time " << currentClockObject->getTimeStamp() <<  std::endl;
            outdata << "OSS: Total time this dispatch was " << std::to_string(timeTaken) << " nanoseconds" << std::endl;
            currentClockObject->setTimeStamp(timeTaken);
            
            //wait for child to finish work and send details
            if (msgrcv(scheduler_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                /*perror(&exe[0])*/1;
            }

            //read status
            std::string highstatus(msg.text);
            status = highstatus;

            //update the clock
            currentClockObject->setTimeStamp(stoi(status.substr(2,status.length())));

            //write out status
            outdata << "OSS: Process " << maskedPID << " has returned with status: " << status[0] << " and ran for " << status.substr(2,status.length()) << "ns" <<  std::endl;
            
            //if status reads "T" for "terminated", remove from PCB and eject from queues
            if(status[0] == 'T'){

                //remove process from RR queue
                tmppid = RRQueues->high_priority[0];
                RRQueues->high_priority.erase(RRQueues->high_priority.begin());

                //Remove the child from the process table
                //ask for permission
                if (msgrcv(PCB_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                    /*perror(&exe[0])*/1;
                }

                //set an empty PCB
                for(int i =0; i < 19; i++){
                    if(currentProcessTable->Process_Table[i].getPid() == RRQueues->high_priority[0])
                        currentProcessTable->Process_Table[i].nullify();
                }

                //remove from the shared PID vector
                for(int i=0; i < PIDSVec->PIDS.size(); i++){
                    if(std::get<0>(PIDSVec->PIDS[i]) == RRQueues->high_priority[0]){
                        PIDSVec->PIDS.erase(PIDSVec->PIDS.begin() + i);
                    }
                }

                //cede access to the PCB back
                if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
                    std::cout << "Can't cede control back to worker from PCB thread!\n";
                    perror(&exe[0]);
                }
                
            }

            //if status reads "B" for "blocked" determine if it goes in low or high priority
            else {

                //if status reads "C" for "CPU" send to end of high priority
                if(status[1] == 'C'){

                    //CPU-Bound Processess Get Priority 1 && need their PCB updated
                    for(int i =0; i < 19; i++){
                        if(currentProcessTable->Process_Table[i].getPid() == RRQueues->high_priority[0])
                            currentProcessTable->Process_Table[i].setPriority(1);
                            currentProcessTable->Process_Table[i].setlastBurstTime(stoi(status.substr(2,status.length())));
                            currentProcessTable->Process_Table[i].setTotalCpuTime(currentProcessTable->Process_Table[i].getTotalCpuTime() + stoi(status.substr(2,status.length())));
                    }

                    //remove from queue
                    tmppid = RRQueues->high_priority[0];
                    RRQueues->high_priority.erase(RRQueues->high_priority.begin());

                    //determine how long the process is blocked for
                    std::string stamp = currentClockObject->getTimeStamp();
                    unsigned int seconds = convertTimeStamp(stamp, 1);
                    unsigned int ns = convertTimeStamp(stamp, 2);

                    //add to blocked queue and add desired un-block time
                    RRQueues->blocked.push_back(tmppid);
                    RRQueues->blockedTimers.push_back((std::to_string(seconds + (rand() % 5 + 0)) + ":" + std::to_string(ns + (rand() % 10000 + 0))));
                }

                //if status reads "I" for "IO" send to end of low priority
                else {

                    //IO-Bound Processess Get Priority 2 && need their PCB updated
                    for(int i =0; i < 19; i++){
                        if(currentProcessTable->Process_Table[i].getPid() == RRQueues->high_priority[0])
                            currentProcessTable->Process_Table[i].setPriority(2);
                            currentProcessTable->Process_Table[i].setlastBurstTime(stoi(status.substr(2,status.length())));
                            currentProcessTable->Process_Table[i].setTotalCpuTime(currentProcessTable->Process_Table[i].getTotalCpuTime() + stoi(status.substr(2,status.length())));
                    }

                    //remove from queue
                    tmppid = RRQueues->high_priority[0];
                    RRQueues->high_priority.erase(RRQueues->high_priority.begin());

                    //determine how long the process is blocked for
                    std::string stamp = currentClockObject->getTimeStamp();
                    unsigned int seconds = convertTimeStamp(stamp, 1);
                    unsigned int ns = convertTimeStamp(stamp, 2);

                    //add to blocked queue and add desired un-block time
                    RRQueues->blocked.push_back(tmppid);
                    RRQueues->blockedTimers.push_back((std::to_string(seconds + (rand() % 5 + 0)) + ":" + std::to_string(ns + (rand() % 10000 + 0))));
                }
            }
        }

        //if low priority has a process ready, let it run
        if(!RRQueues->low_priority.empty() && flipbit == false){

            //show that we did work
            none = false;

            //left child run
            while(!currentTurnSemaphore->getTurn(0));
            currentTurnSemaphore->setTurn(RRQueues->low_priority[0]);

            //get mask for PID
            maskedPID = PIDSVec->getMask(RRQueues->low_priority[0]);

            //print scheduled notification
            srand (time(NULL)+RRQueues->low_priority[0]);
            unsigned int timeTaken = randomTimeTaken(mt);
            outdata << "OSS: Dispatching process with PID " << maskedPID << " from queue 1 at time " << currentClockObject->getTimeStamp() <<  std::endl;
            outdata << "OSS: Total time this dispatch was " << std::to_string(timeTaken) << " nanoseconds" << std::endl;
            currentClockObject->setTimeStamp(timeTaken);

            //wait for child to finish work and send details
            if (msgrcv(scheduler_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                /*perror(&exe[0])*/1;
            }

            //read status
            std::string lowstatus(msg.text);
            status = lowstatus;

            //update the clock
            currentClockObject->setTimeStamp(stoi(status.substr(2,status.length())));

            //write out status
            outdata << "OSS: Process " << maskedPID << " has returned with status: " << status[0] << " and ran for " << status.substr(2,status.length()) << "ns" <<  std::endl;

            //if status reads "T" for "terminated", remove from PCB and eject from queues
            if(status[0] == 'T'){

                //remove process from RR queue
                tmppid = RRQueues->low_priority[0];
                RRQueues->low_priority.erase(RRQueues->low_priority.begin());

                //Remove the child from the process table
                //ask for permission
                if (msgrcv(PCB_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                    /*perror(&exe[0])*/1;
                }

                //set an empty PCB
                for(int i =0; i < 19; i++){
                    if(currentProcessTable->Process_Table[i].getPid() == RRQueues->low_priority[0])
                        currentProcessTable->Process_Table[i].nullify();
                }

                //remove from the shared PID vector
                for(int i=0; i < PIDSVec->PIDS.size(); i++){
                    if(std::get<0>(PIDSVec->PIDS[i]) == RRQueues->low_priority[0]){
                        PIDSVec->PIDS.erase(PIDSVec->PIDS.begin() + i);
                    }
                }

                //cede access to the PCB back
                if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
                    std::cout << "Can't cede control back to worker from PCB thread!\n";
                    perror(&exe[0]);
                }
                
            }

            //if status reads "B" for "blocked" determine if it goes in low or high priority
            else {

                //if status reads "C" for "CPU" send to end of high priority
                if(status[1] == 'C'){

                    //CPU-Bound Processess Get Priority 1 && need their PCB updated
                    for(int i =0; i < 19; i++){
                        if(currentProcessTable->Process_Table[i].getPid() == RRQueues->high_priority[0])
                            currentProcessTable->Process_Table[i].setPriority(1);
                            currentProcessTable->Process_Table[i].setlastBurstTime(stoi(status.substr(2,status.length())));
                            currentProcessTable->Process_Table[i].setTotalCpuTime(currentProcessTable->Process_Table[i].getTotalCpuTime() + stoi(status.substr(2,status.length())));
                    }

                    //remove from queue
                    tmppid = RRQueues->low_priority[0];
                    RRQueues->low_priority.erase(RRQueues->low_priority.begin());

                    //determine how long the process is blocked for
                    std::string stamp = currentClockObject->getTimeStamp();
                    unsigned int seconds = convertTimeStamp(stamp, 1);
                    unsigned int ns = convertTimeStamp(stamp, 2);

                    //add to blocked queue and add desired un-block time
                    RRQueues->blocked.push_back(tmppid);
                    RRQueues->blockedTimers.push_back((std::to_string(seconds + (rand() % 5 + 0)) + ":" + std::to_string(ns + (rand() % 10000 + 0))));
                }

                //if status reads "I" for "IO" send to end of low priority
                else {

                    //IO-Bound Processess Get Priority 2 && need their PCB updated
                    for(int i =0; i < 19; i++){
                        if(currentProcessTable->Process_Table[i].getPid() == RRQueues->high_priority[0])
                            currentProcessTable->Process_Table[i].setPriority(2);
                            currentProcessTable->Process_Table[i].setlastBurstTime(stoi(status.substr(2,status.length())));
                            currentProcessTable->Process_Table[i].setTotalCpuTime(currentProcessTable->Process_Table[i].getTotalCpuTime() + stoi(status.substr(2,status.length())));
                    }

                    //remove from queue
                    tmppid = RRQueues->low_priority[0];
                    RRQueues->low_priority.erase(RRQueues->low_priority.begin());

                    //determine how long the process is blocked for
                    std::string stamp = currentClockObject->getTimeStamp();
                    unsigned int seconds = convertTimeStamp(stamp, 1);
                    unsigned int ns = convertTimeStamp(stamp, 2);

                    //add to blocked queue and add desired un-block time
                    RRQueues->blocked.push_back(tmppid);
                    RRQueues->blockedTimers.push_back((std::to_string(seconds + (rand() % 5 + 0)) + ":" + std::to_string(ns + (rand() % 10000 + 0))));
                }
            }
        }
        
        //change the flipbit
        flipbit = !flipbit;

        //if we made it down here and 'none' was never flipped, then we need to
        //increase time so that our blocked and spawn paramters can continue
        //(Literally Just means that our CPU was wasting time idling)
        if(none){
            unsigned int timeadded = 125000000;
            finalStats.totalIdleTime += timeadded;
            currentClockObject->setTimeStamp(timeadded);
        }

    }
}

/***********************************************************************
Function: main(int, char*)

Description: Main function. Starts by checking parameters and
             allocated memory. It then ensures that the correct 
             pthreads are created and dispatched. It then inits 
             the queue and then starts the scheduler. 
***********************************************************************/
int main(int argc, char *argv[])
{
    //Handle Command Line Switches
    int opt;
    while ((opt = getopt(argc, argv, ":l:s:h")) != -1) {
        switch (opt) {

        //If the given flag is -h, give a help message
        case 'h':
            printHelp();
            break;
        
        // if the flag -s is given, set maximum run time
        case 's':
            sscanf(argv[2], "%d", &maxExecutionTime);
            break;
        
        // if the flag -l is given, set logfile name
        case 'l':
            logfile = std::string(argv[argc-1]);
            break;

        // an unknown argument is supplied
        case '?':
            std::cout << "Invalid option supplied. Terminating." << std::endl;
            cleanUp();
            break;
        }

    }
    
    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //capture sigint 
    signal(SIGINT, siginthandler);

    //stream holders
    std::string currentCommand;

    //forking variables
    pid_t child_pid, wpid;
    int max_run_time = 30;
    int status = 0;

    //check sharedmem
    if(shmid == -1 || PCT_shmid == -1|| Turn_shmid == -1 || RR_shmid == -1 || PIDSVec_shmid == -1 || Final_Log_shmid == -1){
        std::cout << "Shared Memory Error!\n";
        perror(&exe[0]);
    }

    //initialize the PCB queue
    if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Message Failed to send!\n";
        perror(&exe[0]);
    }

    //make our time-watching thread and logging start-time
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::thread timeWatcher (threadKill, start, max_run_time);

    //childwatcher
    std::thread childrenWatcher (threadReturn);

    //call parent function
    schedulingProcess(&child_pid, &wpid, &status);

    //clean up sharedmemory, make sure no zombies are present, and exit
    cleanUp();
}
