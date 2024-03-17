#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "signal.h"
#include "wait.h"
#include "pthread.h"

typedef struct Process Process;
typedef struct Statistics{
    int creation_number;
    int wait_time;
    int enters;
}Statistics;
typedef struct Program{
    int id;
    char name[30];
    int weight, average_exec_time, frequency, total_wait_time, variance_exec_time;
    int total_requests;
    int pids_of_requests[1000];
    void (*create_process)(struct Program*, struct Process*);
    Statistics* Process_Details;
}Program;
void init_Program(Program* p, int id, int weight, int freq, int avg_time, int var_time, char name[]);

typedef struct Process{
    int id;
    pid_t pid;
    int weight;
    int exec_time, wait_time;
    int enters;
    struct Process *next, *prev;
}Process;

void create_process(struct Program* p, struct Process* myProc){
    myProc->weight = p->weight + rand() % 5 - 3; // variance of [-2, +2] around weight
    if(myProc->weight < 1)
        myProc->weight = 1;
    myProc->exec_time = p->average_exec_time + rand() % (p->variance_exec_time * 2) - p->variance_exec_time;
    myProc->wait_time = 0;
    myProc->next = NULL;
    myProc->prev = NULL;
    myProc->id = p->id;
    myProc->pid = fork();
    if(myProc->pid == 0){
        int ret;
        struct timespec ts;
        ts.tv_sec = myProc->exec_time / 1000;
        ts.tv_nsec = (myProc->exec_time % 1000) * 1000000;
        ret = nanosleep(&ts, NULL);
        return;
    }
    else{
        p->pids_of_requests[p->total_requests] = myProc->pid;
        p->total_requests++;
        //printf("Am creat %d\n",myProc->pid);
        kill(myProc->pid, SIGSTOP);
    }
}

void init_Program(Program* p, int id, int weight, int freq, int avg_time, int var_time, char name[]){
    strcpy(p->name, name);
    p->weight = weight;
    p->id = id;
    p->frequency = freq;
    p->average_exec_time = avg_time;
    p->variance_exec_time = var_time;
    p->create_process = &create_process;
    p->total_requests = 0;
    p->total_wait_time = 0;
    p->Process_Details = (Statistics*)malloc(15000 * sizeof(Program));
}