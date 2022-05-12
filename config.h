#include <string>
#include <vector>
#include <tuple>

//Bakery maximum process amount
#define PROCESS_COUNT 20

//Maximum time to run the program
#define MAX_RUN_TIME 100

//Maximum time to allow child creation
#define MAX_INITIALIZATION_TIME 3

//Maximum child processes to allow
#define MAX_PROCESSES 50

//Chance that a user process will terminate
//(This isn't used as a hard percentage, but used
// as the bassline in the testsim that functions as
// the possibility for both IO bound and CPU processes)
#define TERMINATION_CHANCE 0.33

//Message Queue Struct
struct message{
    long identifier =1;
    char text[200];
};

//Clock Object 
struct Clock
{
    private:
       unsigned int seconds;
       unsigned int nanoseconds;

    public:

        /***********************************************************************
        Function: Clock->getTimeStamp()

        Description: Function to get a Time Stamp at the requested
                     point in time.
        ***********************************************************************/
        std::string getTimeStamp(){
            return (std::to_string(seconds) + ":" + std::to_string(nanoseconds));
        }

        /***********************************************************************
        Function: Clock->getFractionalStamp()

        Description: Function to get the clock as a measurement of only the 
                     ellapsed factional seconds
        ***********************************************************************/
        double getFractionalStamp(){
            return (seconds + (nanoseconds/1000000000));
        }

        /***********************************************************************
        Function: Clock->setTimeStamp(unsigned int nanoseconds)

        Description: Function to set a Time Stamp at the requested
                     point in time.
        ***********************************************************************/
        void setTimeStamp(unsigned int newnanoseconds){

            //add the nanoseconds
            nanoseconds += newnanoseconds;

            //rollover into seconds
            if(nanoseconds >= 1000000000){
                seconds += 1;
                nanoseconds -= 1000000000;
            }
        }
};

//Process Block Object
struct PCB 
{
    private:
        unsigned int cpuTime=0;
        unsigned int totalTimeInSystem=0;
        unsigned int lastBurstTime=0;
        int pid=0;
        int priority=0;
    
    public:
        //constructor
        PCB(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e){
            cpuTime = a;
            totalTimeInSystem =b;
            lastBurstTime = c;
            pid = d;
            priority = e;
        }

        //Pseudo "destructor" 
        void nullify(){
            cpuTime = 0;
            totalTimeInSystem =0;
            lastBurstTime = 0;
            pid = 0;
            priority = 0;
        }

        //GETTERS
        /***********************************************************************
        Function: PCB->getTotalCpuTime()

        Description: Function to get total cpu time used
        ***********************************************************************/
        unsigned int getTotalCpuTime(){
            return cpuTime;
        }

        /***********************************************************************
        Function: PCB->getTotalTimeInSystem()

        Description: Function to get total system time used
        ***********************************************************************/
        unsigned int getTotalTimeInSystem(){
            return totalTimeInSystem;
        }

        /***********************************************************************
        Function: PCB->getlastBurstTime()

        Description: Function to get previous burst time used
        ***********************************************************************/
        unsigned int getlastBurstTime(){
            return lastBurstTime;
        }

        /***********************************************************************
        Function: PCB->getPID()

        Description: Function to get process id
        ***********************************************************************/
        int getPid(){
            return pid;
        }

        /***********************************************************************
        Function: PCB->getPriority()

        Description: Function to get process affinity
        ***********************************************************************/
        int getPriority(){
            return priority;
        }

        //SETTERS
        /***********************************************************************
        Function: PCB->setPID()

        Description: Function to set process id
        ***********************************************************************/
        void setPid(int pidt){
            pid = pidt;
        }

        /***********************************************************************
        Function: PCB->setTotalCpuTime()

        Description: Function to set total cpu time used
        ***********************************************************************/
        void setTotalCpuTime(unsigned int time){
            cpuTime = time;
        }

        /***********************************************************************
        Function: PCB->setTotalTimeInSystem()

        Description: Function to set total system time used
        ***********************************************************************/
        void setTotalTimeInSystem(unsigned int time){
            totalTimeInSystem = time;
        }

        /***********************************************************************
        Function: PCB->setlastBurstTime()

        Description: Function to set previous burst time used
        ***********************************************************************/
        void setlastBurstTime(unsigned int time){
            lastBurstTime = time;
        }

        /***********************************************************************
        Function: PCB->setPriority()

        Description: Function to set process affinity
        ***********************************************************************/
        void setPriority(unsigned int input){
            priority = input;
        }
};

//Process Table
struct PCT{
    public:
        struct PCB Process_Table[20]; 
};

//Semaphore for Turn Taking
struct TurnSemaphore{

    private:
        int turn =0;

    public:
        /***********************************************************************
        Function: TurnSemaphore->setTurn(int PID)

        Description: Function to set which child's turn it is
        ***********************************************************************/
        void setTurn(int PID){

            //update the semaphore
            turn = PID;
        }

        /***********************************************************************
        Function: TurnSemaphore->returnTurn(int PID)

        Description: Function to set turn to 0
        ***********************************************************************/
        void returnTurn(){
            turn = 0;
        }

        /***********************************************************************
        Function: TurnSemaphore->getTurn(int PID)

        Description: Function to set which child's turn it is
        ***********************************************************************/
        bool getTurn(int PID){
            //return true if PID matches turn
            return (PID == turn);
        }
};


//Round Robin Queues
struct RR{
    public:
        std::vector<int> high_priority;
        std::vector<int> low_priority;
        std::vector<int> blocked;
        std::vector<std::string> blockedTimers;
};

//vector for storing PIDS
struct PIDSVector{
    public:
        std::vector<std::tuple<int,int>> PIDS;

        int getMask(int PID){
            //find the desired PID mask and return it
            for(int i=0; i < PIDS.size(); i++){
                if(std::get<0>(PIDS[i]) == PID){
                    return std::get<1>(PIDS[i]);
                }                
            }
        }
};

//Semaphore for SIGINT Handling
struct SigSemaphore{

    private:
        bool kill = false;
        bool received = false;

    public:
        /***********************************************************************
        Function: SigSemaphore->signal()

        Description: Function to set whether signint has been passed
        ***********************************************************************/
        void signal(){

            //update the semaphore
            kill = true;
        }

        /***********************************************************************
        Function: SigSemaphore->signalhandshake()

        Description: Function to set whether signint has been passed
        ***********************************************************************/
        void signalhandshake(){

            //update the semaphore
            received = true;
        }

        /***********************************************************************
        Function: SigSemaphore->check()

        Description: Function to get whether signint has been passed
        ***********************************************************************/
        bool check(){
            return kill;
        }

        /***********************************************************************
        Function: SigSemaphore->handshake()

        Description: Function to get whether signint has been passed
        ***********************************************************************/
        bool handshake(){

            return received;
        }
};

//Final Data Table Used For Final Statistics
struct STATS{
    public:
        unsigned int averageWaitingTime = 0;
        int averageWaitingTimeUpdated = 0;
        unsigned int averageHighCpuUtilization = 0;
        int averageHighCpuUtilizationUpdated = 0;
        unsigned int averageLowCpuUtilization = 0;
        int averageLowCpuUtilizationUpdated = 0;
        unsigned int averageTimeInSystem = 0;
        int averageTimeInSystemUpdated = 0;
        unsigned int averageTimeInBlockedCPU = 0;
        int averageTimeInBlockedCPUUpdated = 0;
        unsigned int averageTimeInBlockedIO = 0;
        int averageTimeInBlockedIOUpdated = 0;
        unsigned int totalIdleTime = 0;
};

//Shared Final Data Table Used For Final Statistics
struct SHARED_STATS{
    public:
        //Data structures to keep track of process utilization
        struct IOSTATS{
            public:
                unsigned int averageWaitingTime = 0;
                int averageWaitingTimeUpdated = 0;
                unsigned int averageCpuUtilization = 0;
                int averageCpuUtilizationUpdated = 0;
                unsigned int averageTimeInSystem = 0;
                int averageTimeInSystemUpdated = 0;
                unsigned int averageTimeInBlocked = 0;
                int averageTimeInBlockedUpdated = 0;
        }IO;
        struct CPUSTATS{
            public:
                unsigned int averageWaitingTime = 0;
                int averageWaitingTimeUpdated = 0;
                unsigned int averageCpuUtilization = 0;
                int averageCpuUtilizationUpdated = 0;
                unsigned int averageTimeInSystem = 0;
                int averageTimeInSystemUpdated = 0;
                unsigned int averageTimeInBlocked = 0;
                int averageTimeInBlockedUpdated = 0;
        }CPU;

        //functions for convenience 
        //set the average waiting time
        void setAverageWaitTime(unsigned int time, bool proctype){
            if(proctype){ //CPU
                CPU.averageWaitingTime += time;
                CPU.averageWaitingTimeUpdated += 1;
            }
            else{
                IO.averageWaitingTime += time;
                IO.averageWaitingTimeUpdated += 1;
            }
        }
        
        //set the average cpu usage time
        void setAverageCPUUsageTime(unsigned int time, bool proctype){
            if(proctype){ //CPU
                CPU.averageCpuUtilization += time;
                CPU.averageCpuUtilizationUpdated += 1;
            }
            else{
                IO.averageCpuUtilization += time;
                IO.averageCpuUtilizationUpdated += 1;
            }
        }

        //set the average time in system
        void setAverageTimeInSystem(unsigned int time, bool proctype){
            if(proctype){ //CPU
                CPU.averageTimeInSystem += time;
                CPU.averageTimeInSystemUpdated += 1;
            }
            else{
                IO.averageTimeInSystem += time;
                IO.averageTimeInSystemUpdated += 1;
            }
        }
    
        //set the average time in block
        void setAverageTimeBlocked(unsigned int time, bool proctype){
            if(proctype){ //CPU
                CPU.averageTimeInBlocked += time;
                CPU.averageTimeInBlockedUpdated += 1;
            }
            else{
                IO.averageTimeInBlocked += time;
                IO.averageTimeInBlockedUpdated += 1;
            }
        }
        //get the average waiting time
        //Each process was meant to mainuplate floats to prevent division by zero, but I forgot I was not using python...
        //so down the drain that idea went. The rounding has no effect on the value so I left it and added in an alternate
        //line of text to each return in-case the entry times was 0, thus preventing a division by 0
        std::string getAverageWaitTime(bool proctype){
            if(proctype){ //CPU
                if(CPU.averageWaitingTimeUpdated)
                    return "Average of Total Time Spent Not Running: " + std::to_string(static_cast<unsigned int>(((CPU.averageWaitingTime/float(.0000000000001 + CPU.averageWaitingTimeUpdated))/1000000000))) + " seconds and " + std::to_string(((CPU.averageWaitingTime/static_cast<unsigned int> (float(.0000000000001 + CPU.averageWaitingTimeUpdated))%1000000000))) + " nanoseconds";
                else 
                    return "These processes spent no time waiting.";
            }
            else{
                if(IO.averageWaitingTimeUpdated)
                    return "Average of Total Time Spent Not Running: " + std::to_string(static_cast<unsigned int>(((IO.averageWaitingTime/float(.0000000000001 + IO.averageWaitingTimeUpdated))/1000000000))) + " seconds and " + std::to_string(((IO.averageWaitingTime/static_cast<unsigned int> (float(.0000000000001 + IO.averageWaitingTimeUpdated))%1000000000))) + " nanoseconds";
                else 
                    return "These processes spent no time waiting.";
            }
        }
        
        //get the average cpu usage time
        std::string getAverageCPUUsageTime(bool proctype){
            if(proctype){ //CPU
                if(CPU.averageCpuUtilizationUpdated)
                    return "Average of Total Time Spent Using The CPU: " + std::to_string(static_cast<unsigned int>(((CPU.averageCpuUtilization/float(.0000000000001 + CPU.averageCpuUtilizationUpdated))/1000000000))) + " seconds and " + std::to_string(((CPU.averageCpuUtilization/static_cast<unsigned int> (float(.0000000000001 + CPU.averageCpuUtilizationUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time using the cpu.";
            }
            else{
                if(IO.averageCpuUtilizationUpdated)
                    return "Average of Total Time Spent Using The CPU: " + std::to_string(static_cast<unsigned int>(((IO.averageCpuUtilization/float(.0000000000001 + IO.averageCpuUtilizationUpdated))/1000000000))) + " seconds and " + std::to_string(((IO.averageCpuUtilization/static_cast<unsigned int> (float(.0000000000001 + IO.averageCpuUtilizationUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time using the cpu.";
            }
        }

        //get the average time in system
        std::string getAverageTimeInSystem(bool proctype){
            if(proctype){ //CPU
                if(CPU.averageTimeInSystemUpdated)
                    return "Average of Total Time Spent In The System: " + std::to_string(static_cast<unsigned int>(((CPU.averageTimeInSystem/float(.0000000000001 + CPU.averageTimeInSystemUpdated))/1000000000))) + " seconds and " + std::to_string(((CPU.averageTimeInSystem/static_cast<unsigned int> (float(.0000000000001 + CPU.averageTimeInSystemUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time in the system.";
            }
            else{
                if(IO.averageTimeInSystemUpdated)
                    return "Average of Total Time Spent In The System: " + std::to_string(static_cast<unsigned int>(((IO.averageTimeInSystem/float(.0000000000001 + IO.averageTimeInSystemUpdated))/1000000000))) + " seconds and " + std::to_string(((IO.averageTimeInSystem/static_cast<unsigned int> (float(.0000000000001 + IO.averageTimeInSystemUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time in the system.";
            }
        }
    
        //get the average time in block
        std::string getAverageTimeBlocked(bool proctype){
            if(proctype){ //CPU
                if(CPU.averageTimeInBlockedUpdated)
                    return "Average of Time Spent In The Blocked Queue: " + std::to_string(static_cast<unsigned int>(((CPU.averageTimeInBlocked/float(.0000000000001 + CPU.averageTimeInBlockedUpdated))/1000000000))) + " seconds and " + std::to_string(((CPU.averageTimeInBlocked/static_cast<unsigned int> (float(.0000000000001 + CPU.averageTimeInBlockedUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time in the blocked Queue.";
            }
            else{
                if(IO.averageTimeInBlockedUpdated)
                    return "Average of Time Spent In the Blocked Queue: " + std::to_string(static_cast<unsigned int>(((IO.averageTimeInBlocked/float(.0000000000001 + IO.averageTimeInBlockedUpdated))/1000000000))) + " seconds and " + std::to_string(((IO.averageTimeInBlocked/static_cast<unsigned int> (float(.0000000000001 + IO.averageTimeInBlockedUpdated)))%1000000000)) + " nanoseconds";
                else 
                    return "These processes spent no time in the blocked Queue.";
            }
        }
};