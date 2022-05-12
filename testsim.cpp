/***********************************************************************
testsim code for project 4:

Author: Tyler Martin
Date: 11/4/2021

-Testsim is the binary that actually runs as a service that our OSS
 manages. IT pretends to do work and then immediately exit. It exits 
 by sending a message back to OSS containing if it finished or blocked,
 what it wanted if it blocked, and how much time it used. It continues 
 until it reaches a termination, decided by a random number between 0-1.0
 with preference held from .33.  IMPORTANT-NOTE: the time quantum I defined 
 for the children to use was 500000000. So usage times were between 
 0-500000000ns for execution.

Testing steps:
Program accesses the shared memory structs and message queues and receives
its PID. It then watches the shared semaphore until it's signaled to run 
again. Once that happens, it runs and decides on what it does and reports
it back. It cleans up on termination.
***********************************************************************/

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <random>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <ios>
#include "string.h"
#include "sys/msg.h"
#include "config.h"

//key for address location
#define SHM_KEY 0x98273
#define PARENT_KEY 0x94512
#define PCT_KEY 0x92612
#define TURN_KEY 0x95712
#define KEY 0x00043
#define CLOCK_KEY 0x05912
#define SCHEDULER_KEY 0x46513
#define FINAL_STATS_KEY 0x396561

//perror header
std::string exe;

//message object
struct message msg;

/***********************************************************************
 create shared message queues 

 Queues: 
        Queue for child-to-parent communication
        Queue for parent check-up
        Queue for all-to-clock communications
        Queue for talking to the scheduler 
***********************************************************************/
int msgid= msgget(KEY, 0666 | IPC_CREAT);

int parent_msgid = msgget(PARENT_KEY, 0666 | IPC_CREAT);

int clock_msgid = msgget(CLOCK_KEY, 0666 | IPC_CREAT); 

int scheduler_msgid = msgget(SCHEDULER_KEY, 0666 | IPC_CREAT); 

/***********************************************************************
 create shared message memory segments 

 Objects:
        Clock Object for time management
        Turn object for turn taking
        Struct that contains the final stats that the children
                         will log to
***********************************************************************/
int shmid = shmget(SHM_KEY, sizeof(struct Clock), 0600|IPC_CREAT);
struct Clock *currentClockObject = (static_cast<Clock *>(shmat(shmid, NULL, 0)));

int Turn_shmid = shmget(TURN_KEY, sizeof(struct TurnSemaphore), 0600|IPC_CREAT);
struct TurnSemaphore *currentTurnSemaphore = (static_cast<TurnSemaphore *>(shmat(Turn_shmid, NULL, 0)));

int Final_Log_shmid = shmget(FINAL_STATS_KEY, sizeof(struct SHARED_STATS), 0600|IPC_CREAT);
struct SHARED_STATS *currentSharedStats = (static_cast<SHARED_STATS *>(shmat(Final_Log_shmid, NULL, 0)));

/***********************************************************************
Function: int getPID()

Description: Function that executes when child is created, gets
             PID from shared message queue, logs it, and sends
             message back to parent letting it know it's ready
***********************************************************************/
int GetPID(){
    int PID = 0;

    //get our pid
    if (msgrcv(msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //update our pid
    sscanf(msg.text, "%d", &PID);

    //Let Parent know that we are ready
    if(msgsnd(parent_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Message Failed to send!\n";
        perror(&exe[0]);
    }

    return PID;
}

/***********************************************************************
Function: getTotalTime(std::stamp)

Description: Function that converts time stamps for logging use
***********************************************************************/
unsigned int getTotalTime(std::string stamp){

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

    //return final ns count
    return array[4];
    
}

/***********************************************************************
Function: int doWork(int PID)

Description: Function that executes when child is put into the running
             state as indicated by the shared semaphore that controls
             the running PID. It will spit out a random number of time
             that it was running for, and then it needs to decide whether
             or not the process is going to terminate. If it does not 
             terminate, then it needs to decide whether or not it uses 
             the entire time slice, or it gets blocked.
***********************************************************************/
void doWork(int PID){
    
    //keep track of whether or not we terminated
    bool terminated = false;
    int i = 0;

    //random seeds and generators
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> runningTime(1.0, 5000000.0);
    std::uniform_real_distribution<double> type(0.0, 1.0);

    //am I a CPU-Bound process or an IO process?
    bool processType = (type(mt) > 0.4); // if true, then process is CPU bound

    //Get process Start Time
    std::string startingStamp = currentClockObject->getTimeStamp();

    //set variable for blocked time logging
    std::string blockedStart="0";

    //keep track of total time not waiting for later use
    unsigned int totalActiveTime = 0;

    while(!terminated){

        //blocked status holding string
        std::string blocked;

        //busy wait until turn is granted (with error handling)
        while(!currentTurnSemaphore->getTurn(PID)){
            if(currentTurnSemaphore->getTurn(-1)) //worst case, something went wrong, program can be told to terminate
                return;
        };

        //update the total blocked stat
        if(blockedStart != "0"){
            currentSharedStats->setAverageTimeBlocked(getTotalTime(blockedStart), processType);
            blockedStart == "0";
        }

        bool bound;
        //get chance 
        //different chance of blocking for different process types
        if (processType){ // CPU 
            bound = (type(mt) < TERMINATION_CHANCE); // if true, it's bound and won't terminate (33%)
        }
        else { // IO
            bound = (type(mt) > TERMINATION_CHANCE); // if true, it's bound and won't terminate (66%)
        }
        

        //reset semaphore
        currentTurnSemaphore->returnTurn();

        //determine how long it ran
        //FIXME: CHANGE COUT TO MESSAGE PASSING
        if(bound){
            //determine if it is cpu bound or io bound
            if(processType){
                unsigned int timeRunning = static_cast<unsigned int>(runningTime(mt));
                blocked =  std::string("B") + std::string("C") + std::to_string(timeRunning);

                //update the stats struct
                currentSharedStats->setAverageCPUUsageTime(timeRunning, processType);
                totalActiveTime += timeRunning;

                //set flag to show that we have been blocked
                blockedStart = currentClockObject->getTimeStamp();
            }
            else {
                unsigned int timeRunning = static_cast<unsigned int>(runningTime(mt));
                blocked =  std::string("B") + std::string("I") + std::to_string(timeRunning);

                //update the stats struct
                currentSharedStats->setAverageCPUUsageTime(timeRunning, processType);
                totalActiveTime += timeRunning;

                //set flag to show that we have been blocked
                blockedStart = currentClockObject->getTimeStamp();
            }

            //copy string status to the msg queue text
            //strcpy(msg.text, blocked.c_str());
            blocked.copy(msg.text, 200);
        }
        else {
            //return terminated status and timing
            blocked =  std::string("T") + std::string("N") + std::to_string(static_cast<unsigned int>(runningTime(mt)));

            //copy string status to the msg queue text
            //strcpy(msg.text, blocked.c_str());
            blocked.copy(msg.text, 200);
            terminated = true;
            i += 1;

            //log total time in system
            currentSharedStats->setAverageTimeInSystem(getTotalTime(startingStamp),processType);

            //update the total time spent waiting
            currentSharedStats->setAverageWaitTime((getTotalTime(startingStamp) - totalActiveTime), processType);
        }

        //Cede control back to OSS
        if(msgsnd(scheduler_msgid, &msg, strlen(msg.text)+1, 0) == -1){
            std::cout << "Can't cede control back to scheduler from child process!\n";
            perror(&exe[0]);
        }

        //save a bit of time and return if time
        if(terminated){
            return;
        }
    }
}

int main(int argc, char *argv[]){

    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //check all memory allocations
    if(shmid == -1) {
        std::cout <<"Pre-Program Memory allocation error. Program must exit.\n";
        errno = 11;
        perror(&exe[0]);
    }

    //Get own Pid
    int PID = GetPID();

    //actually do work when allowed
    doWork(PID);

    //close memory
    shmdt((void *) currentTurnSemaphore);
    shmdt((void *) currentClockObject);
    shmdt((void *) currentSharedStats);

    exit(EXIT_SUCCESS);
}