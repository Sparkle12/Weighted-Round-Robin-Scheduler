#include<signal.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<time.h>
#include<bits/sigaction.h>
#include"program.h"
#include<errno.h>

volatile sig_atomic_t proc_flag_finish = 0;
typedef struct ProcessQueue {
    Process p;
    struct ProcessQueue* next;
    struct ProcessQueue* prev;
}ProcessQueue;

typedef struct CPU
{
    Process *head,*tail,*curr;
    int id,nr_proc,q;
    float run_time;

    void (*add_proc)(Process *,struct CPU*);
    void (*remove_proc)(Process *,struct CPU*);
    void (*run_proc)(struct CPU*);
    void (*run)(struct CPU*);
}CPU;

// VARIABILE GLOBALE
ProcessQueue* head_proc; ProcessQueue* tail_proc;
Program* Program_list;
int number_of_programs = 2, MainPid;
int GlobalFlag_Generare_Proc = 1;
int GlobalFlag_Procesare = 1;
const int number_of_cpus = 1;
pthread_mutex_t ProcessQueueMutex, CPUQueueMutex;
CPU cpus[1];
int total_processes_created = 0;

void add_proc(Process *proc, CPU *self)
{
    pthread_mutex_lock(&CPUQueueMutex);
    if(self->head == NULL)
    {
        self->head = proc;
        self->head->next = self->head;
        self->head->prev = self->head;
        self->curr = self->head;
    }
    else
    if(self->tail == NULL)
    {
        self->tail = proc;
        self->head->next = self->tail;
        self->tail->prev = self->head;
        self->tail->next = self->head;
        self->head->prev = self->tail;
    }
    else
    {
        self->tail->next = proc;
        proc->prev = self->tail;
        self->tail = proc;
        self->tail->next = self->head;
        self->head->prev = self->tail;
    }
    self->nr_proc++;
    pthread_mutex_unlock(&CPUQueueMutex);
}

void remove_proc(Process *to_pop,CPU *cpu)
{
    pthread_mutex_lock(&CPUQueueMutex);
    printf("Process %d has been completed with a waiting time of %d\n",to_pop->pid, to_pop->wait_time);
    Program_list[to_pop->id].Process_Details[to_pop->pid].wait_time = to_pop->wait_time;
    if(cpu->nr_proc == 0) {
        pthread_mutex_unlock(&CPUQueueMutex);
        return;
    }
    if(cpu->nr_proc == 1)
    {
        cpu->head = NULL;
        cpu->curr = NULL;
    }

    cpu->nr_proc--;
    if(to_pop == cpu->head)
    {
        cpu->head = cpu->head->next;
        if(cpu->head == cpu->tail)
        {
            cpu->tail = NULL;
        }
    }

    if(to_pop == cpu->tail)
    {
        cpu->tail = cpu->tail->prev;
        if(cpu->head == cpu->tail)
            cpu->tail = NULL;
    }

    to_pop->prev->next = to_pop->next;
    to_pop->next->prev = to_pop->prev;
    free(to_pop);
    to_pop = NULL;
    pthread_mutex_unlock(&CPUQueueMutex);
}

void run_proc(CPU *self)
{

    struct timespec t,rem;
    t.tv_sec = (self->q*self->curr->weight) / 1000;
    t.tv_nsec = ((self->q*self->curr->weight) % 1000) * 1000000;

    kill(self->curr->pid,SIGCONT);
    //printf("quanta: %d weight :%d, product: %d\n", self->q, self->curr->weight, self->q*self->curr->weight);
    //printf("timp de dormit %ld:%ld\n", t.tv_sec, t.tv_nsec);
    int ret = nanosleep(&t,&rem);
    printf("Core %d a executat %d din %s si mai are %d\n",self->id, self->curr->pid, Program_list[self->curr->id].name, self->nr_proc);
    if(proc_flag_finish == 1){
        self->remove_proc(self->curr,self);
        proc_flag_finish = 0;
    }
    else
        kill(self->curr->pid,SIGSTOP);
}

void increment_wait_times(Process* start, int time){
    Program_list[start->id].Process_Details[start->pid].enters++;
    Process* aux = start->next;
    while(aux != start){
        //if(aux->id != start->id)
            Program_list[aux->id].total_wait_time += time;
        aux->wait_time += time;
        aux = aux->next;
    }
}

void run(CPU *self)
{
    while(GlobalFlag_Procesare){
        if(self->head != NULL) {
            if(self->nr_proc > 1)
                increment_wait_times(self->curr, self->curr->weight * self->q);
            else
                if(self->nr_proc == 1) {
                    if (self->curr->pid == 0)
                        printf("SUNT %d\n", getpid());
                    Program_list[self->curr->id].Process_Details[self->curr->pid].enters++;
                }
            self->run_proc(self);
            if(self->curr != NULL)
                self->curr = self->curr->next;
        }
    }
}

void init(CPU* self, int id)
{
    self->head = NULL;
    self->tail = NULL;
    self->curr = NULL;
    self->nr_proc = 0;
    self->id = id;
    self->run_time = 0;
    self->add_proc = &add_proc;
    self->remove_proc = &remove_proc;
    self->run_proc = &run_proc;
    self->q = 120;
}

void handlerSigchld(int snum)
{
    proc_flag_finish = 1;
    waitpid(-1,NULL,WNOHANG);
}


void* cpu_thread(void* args){
    struct sigaction psa;
    sigset_t block_mask;
    sigfillset(&block_mask);
    sigdelset(&block_mask,SIGCHLD);
    psa.sa_mask = block_mask;
    psa.sa_flags = SA_NOCLDSTOP;
    psa.sa_handler = handlerSigchld;
    sigaction(SIGCHLD,&psa,NULL);
    int id = *((int*)args);
    run(&cpus[id]);
    return 0;
}





void pop_proc_queue(pthread_mutex_t* ProcQueueMutex){
    pthread_mutex_lock(ProcQueueMutex);
    if(head_proc->next == NULL){
        pthread_mutex_unlock(ProcQueueMutex);
        return;
    }
    ProcessQueue* aux = head_proc;
    head_proc = head_proc->next;
    head_proc->prev = NULL;
    //free(aux);
    pthread_mutex_unlock(ProcQueueMutex);
}

int empty_proc_queue(pthread_mutex_t* ProcQueueMutex){
    pthread_mutex_lock(ProcQueueMutex);
    if(head_proc == tail_proc){
        pthread_mutex_unlock(ProcQueueMutex);
        return 1;
    }
    pthread_mutex_unlock(ProcQueueMutex);
    return 0;
}

void push_proc_queue(Process* p, pthread_mutex_t* ProcQueueMutex){
    pthread_mutex_lock(ProcQueueMutex);
    tail_proc->p = *p;
    tail_proc->next = (ProcessQueue *) malloc(sizeof(ProcessQueue));
    tail_proc->next->prev = tail_proc;
    tail_proc = tail_proc->next;
    tail_proc->next = NULL;
    pthread_mutex_unlock(ProcQueueMutex);
}

Process* front_proc_queue(pthread_mutex_t* ProcQueueMutex){
    pthread_mutex_lock(ProcQueueMutex);
    if(head_proc->p.pid == 0)
        printf("pua\n");
    Process* p = &head_proc->p;
    pthread_mutex_unlock(ProcQueueMutex);
    return p;
}
Process* end_proc_queue(pthread_mutex_t* ProcQueueMutex){
    pthread_mutex_lock(ProcQueueMutex);
    Process* p = &tail_proc->prev->p;
    pthread_mutex_unlock(ProcQueueMutex);
    return p;
}

void* create_process_loop(void *){
    while(GlobalFlag_Generare_Proc == 1 && getpid() == MainPid){
        for (int i = 0; i < number_of_programs && getpid() == MainPid; i++) {
            int random_value = rand() % 9000000;
            if(random_value <= Program_list[i].frequency){
                total_processes_created++;
                Process* p = (Process *) malloc(sizeof(Process ));
                Program_list[i].create_process(&Program_list[i], p);
                Program_list[i].Process_Details[p->pid].creation_number = total_processes_created;
                Program_list[i].Process_Details[p->pid].enters = 0;
                push_proc_queue(p, &ProcessQueueMutex);
            }
        }
    }
    if(getpid() == MainPid){
        printf("..........Process creation was stopped..........\n");
    }
    return 0;
}

int main()
{
    srand(time(0));

    MainPid = getpid();
    if ( pthread_mutex_init (&ProcessQueueMutex , NULL )) {
        perror ( NULL );
        return errno;
    }
    if ( pthread_mutex_init (&CPUQueueMutex , NULL )) {
        perror ( NULL );
        return errno;
    }

    Program_list = (Program*)malloc(number_of_programs * sizeof(Program));
    head_proc = (ProcessQueue *) malloc(sizeof(ProcessQueue));
    tail_proc = head_proc;
    head_proc->next = NULL;
    head_proc->prev = NULL;
    init_Program(&Program_list[0],0,  1, 3, 90, 10, "Chrome");
    init_Program(&Program_list[1], 1, 15, 0, 7000, 40, "CS 1.6");

    pthread_t thr;
    if ( pthread_create (& thr , NULL , create_process_loop , NULL)) {
        perror ( NULL );
        return errno;
    }
    if(getpid() != MainPid){
        return 0;
    }

    pthread_t cpu_t[number_of_cpus];
    for(int i = 0; i < number_of_cpus; i++) {
        init(&cpus[i], i);
        if (pthread_create(&cpu_t[i], NULL, cpu_thread, &cpus[i].id)) {
            perror(NULL);
            return errno;
        }
    }
    //sleep(1);
    clock_t start = clock(), end;
    int last_sec = 0;
    double time_taken = 0;
    while(time_taken < 10){
        if(!empty_proc_queue(&ProcessQueueMutex)) {
            Process *p = front_proc_queue(&ProcessQueueMutex);
            cpus[0].add_proc(p, &cpus[0]);
            pop_proc_queue(&ProcessQueueMutex);
        }
        end = clock();
        time_taken = ((double)(end-start))/(2.5e+6);
        if(last_sec < time_taken) {
            printf("%lf\n", time_taken);
            last_sec++;
        }
    }
    GlobalFlag_Generare_Proc = 0;
    while(wait(NULL) > 0);
    GlobalFlag_Procesare = 0;
    for(int i = 0; i < number_of_cpus; i++){
        if ( pthread_join ( cpu_t[i], NULL)) {
            perror ( NULL );
            return errno ;
        }
    }
    if ( pthread_join ( thr , NULL)) {
        perror ( NULL );
        return errno ;
    }
    for(int i = 0; i < number_of_programs; i++){
        printf("Process %s has waited a total of %d milliseconds for %d processes\n", Program_list[i].name, Program_list[i].total_wait_time, Program_list[i].total_requests);
        for(int j = 0; j < Program_list[i].total_requests; j++){
            int pid = Program_list[i].pids_of_requests[j];
            printf("Process %d was created %d and has waited %d for %d entries\n", pid, Program_list[i].Process_Details[pid].creation_number, Program_list[i].Process_Details[pid].wait_time, Program_list[i].Process_Details[pid].enters);
        }
    }
    return 0;
}