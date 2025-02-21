/**
Class: CSC452 
Authors: Adler Nguyen & May Niu
Project: phase1.c 
Description: 
    This project simulates a process manager of an operating system. 
    Methods include: 
        - spork()
        - join()
        - quit_phase1a
        - TEMP_switchTo
*/

#include <stdio.h>
#include <phase1.h>
#include <string.h>  
#include <stdlib.h> 
#include <usloss.h>

typedef enum {
    EMPTY,
    READY,       
    RUNNING,     
    FINISHED,
    BLOCKED,  
} processState;

/**
This struct will act as a Process Control Block
*/
typedef struct process {
    int pid; 
    processState state;
    USLOSS_Context context;
    int priority; 
    char name[MAXNAME];  
    struct process *parent;        
    struct process *first_child; 
    struct process *next_sibling;
    int exit_status;                
    int (*startFunc)(void *);
    void *arg;                       
    void *stack;                    
} process;  

/*
GLOBAL VARIABLES
*/
process processTable[MAXPROC];
process *currentProcess;
int currentPid = 2;     
int numberOfProcesses = 0;

/**
Checks if the current process is in kernel mode
*/
int isKernel(){
    return (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) != 0;
}

/**
This helper function acts as a converter between Spork()'s parameter int(*func) (void *) and the Context_init type void(*func)(void *)
*/
void wrapper(void){
    int psr = USLOSS_PsrGet();
    USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
    int status = currentProcess->startFunc(currentProcess->arg);
    quit_phase_1a(status, currentProcess->parent->pid);
}

/**
This helper enables interrupts 
*/
int  enableInterrupts() {
    unsigned int old_psr = USLOSS_PsrGet();

    unsigned int new_psr = old_psr & ~USLOSS_PSR_CURRENT_INT;

    int result = USLOSS_PsrSet(new_psr);
    if (result != USLOSS_DEV_OK) {
        USLOSS_Console("ERROR: Failed to enable interrupts. USLOSS_PsrSet() returned %d\n", result);
        USLOSS_Halt(1);
    }

    return old_psr;
}

/**
This helper restores the old psr
*/
void restoreInterrupts(int psr){
    (void) USLOSS_PsrSet(psr);
}

/*
This is the function that will be called when the process is running
*/
void init_main() {
    // bootstrap
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using spork() to create it.\n");

    // create testcase_main process
    int test_pid = spork("testcase_main", (int (*)(void *))testcase_main, NULL, USLOSS_MIN_STACK, 3);
    
    if (test_pid < 0) {
        USLOSS_Console("ERROR: Failed to create testcase_main process\n");
        USLOSS_Halt(1);  
    }

    TEMP_switchTo(test_pid);

    // Wait for all child processes to finish (JOIN)
    while (1) {
        int status;
        int joined_pid = join(&status);
        if (joined_pid == -2) {
            USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
            USLOSS_Halt(1);
        }
    }

}

/**
Initaizes the values
*/
void phase1_init(void){
    for(int i = 0 ; i < MAXPROC; i++){
        processTable[i].pid = -1;
        processTable[i].state = EMPTY;
        processTable[i].parent = NULL;
        processTable[i].first_child = NULL;
        processTable[i].next_sibling = NULL;
        processTable[i].stack = NULL;
        processTable[i].exit_status = -1;
        
    }
    // create the init process
    int index = 1 % MAXPROC;
    process *new_proc = &processTable[index];

    new_proc->pid = 1; 
    new_proc->priority = 6;
    new_proc->state = READY;
    new_proc->exit_status = 0;
    strncpy(new_proc->name, "init", MAXNAME);
    new_proc->startFunc = (int (*)(void *)) init_main; 
    new_proc->arg = NULL;
    new_proc->stack = malloc(USLOSS_MIN_STACK);
    if (new_proc->stack == NULL) {
        USLOSS_Console("Failed to allocate stack for init\n");
        USLOSS_Halt(1);
    }
    if (!new_proc->stack) {
        USLOSS_Console("ERROR: Memory allocation failed for init process.\n");
        USLOSS_Halt(1);
    }

    USLOSS_Context *oneContextPtr = &processTable[index].context;
    USLOSS_ContextInit(oneContextPtr, new_proc->stack, USLOSS_MIN_STACK, NULL, (void (*)(void))init_main);
    
    numberOfProcesses+=1;
}


/**
This function creates a child process of the current process and returns the pid of the new child
*/
int spork(char *name, int(*func)(void *), void *arg, int stacksize, int priority){

    // check if in user mode
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: Someone attempted to call spork while in user mode!\n");
        USLOSS_Halt(1);
    }


    int psr = enableInterrupts();

    // find an empty slot of the child
    int pid = -1;
    process *childProcess = NULL;
    for(int i = 0; i < MAXPROC; i++){
        int candPid = currentPid;
        int slot = currentPid % MAXPROC; 
        if (processTable[slot].state == EMPTY) {
            pid = candPid;
            childProcess = &processTable[slot];
            currentPid = (currentPid + 1);  
            break;
        }
        currentPid++;
    }

    if(childProcess == NULL){
        restoreInterrupts(psr);
        return -1;
    }

    // check for valid parameters
    if (!name || !func) {
        restoreInterrupts(psr);
        return -1;
    }
    if (priority < 1 || priority > 5) {
        restoreInterrupts(psr);
        return -1;
    }
    if (stacksize < USLOSS_MIN_STACK) {
        restoreInterrupts(psr);
        return -2;
    }

    // add child to current parent process
    childProcess->pid = pid;
    strncpy(childProcess->name, name, MAXNAME);            
    childProcess->priority = priority; 
    childProcess->state = READY;
    childProcess->parent = currentProcess;
    childProcess->stack = malloc(stacksize);
    childProcess->exit_status = -1;
    childProcess->startFunc = func;
    childProcess->arg = arg;

    childProcess->next_sibling = currentProcess->first_child;
    currentProcess->first_child = childProcess;

    numberOfProcesses+=1;

    USLOSS_ContextInit(&childProcess->context, childProcess->stack, stacksize, NULL, wrapper);

    restoreInterrupts(psr);
    return pid;
}

/**
Removes a child from the parent's list.
*/
void removeChild(process *parent, process *child) {
    if (!parent || !child) {
        return;
    }

    if (parent->first_child == child) {
        parent->first_child = child->next_sibling;
    } else {
        process *prev = parent->first_child;
        while (prev && prev->next_sibling != child) {
            prev = prev->next_sibling;
        }
        if (prev) {
            prev->next_sibling = child->next_sibling;
        }
    }

    child->state = EMPTY;  
    child->next_sibling = NULL;
    child->parent = NULL;
    child->pid = -1;
    free(child->stack);
}


/**
Blocks the current process until one of its children terminates
*/
int join(int *status){
    // check user mode
    if (isKernel() != 1){
        USLOSS_Halt(1);
    }

    // checks for valid status
    if (!status) {
        return -3;
    }

    // no children
    if (currentProcess->first_child == NULL) {
        return -2;
    }
    int psr = enableInterrupts();

    process *curr = currentProcess->first_child; 
    while (curr != NULL) {
        if (curr->state == FINISHED) {
            *status = curr->exit_status; 
            int pid = curr->pid;

            removeChild(currentProcess, curr);
            numberOfProcesses--;
            restoreInterrupts(psr);
            return pid;  
        }
        curr = curr->next_sibling;
    }

    restoreInterrupts(psr);
    return -2;
}


void quit_phase_1a(int status, int switchToPid){
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: Someone attempted to call quit_phase_1a while in user mode!\n");
        USLOSS_Halt(1);
    }

    if (currentProcess->first_child != NULL) {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", currentProcess->pid);
        USLOSS_Halt(1);
    }

    int psr = enableInterrupts();

    currentProcess->state = FINISHED;
    currentProcess->exit_status = status; 

    numberOfProcesses--;  
    restoreInterrupts(psr);
    TEMP_switchTo(switchToPid);
}


/**
Gets the current processes pid
*/
int getpid(void){
    return (currentProcess) ? currentProcess->pid : -1;
}

/**
Prints the process table.
*/
void dumpProcesses(void) {
    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");

    for (int i = 0; i < MAXPROC; i++) {
        process *p = &processTable[i];

        if (p->state != EMPTY) {  
            const char *state;
            char stateBuff[32];

            switch (p->state) {
                case RUNNING:
                    state = "Running";
                    break;
                case READY:
                    state = "Runnable";
                    break;
                case FINISHED:
                    snprintf(stateBuff, sizeof(stateBuff), "Terminated(%d)", p->exit_status);
                    state = stateBuff;
                    break;
                default:
                    state = "UNKNOWN";
            }
            USLOSS_Console("%4d %5d  %-16s %4d      %s\n",
                p->pid,
                (p->parent == NULL) ? 0 : p->parent->pid,  
                p->name,
                p->priority,
                state);
        }
    }
}


/**
Switches the context from the old process to the new process.
*/
void TEMP_switchTo(int pid){
    
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: TEMP_switchTo called from user mode\n");
        USLOSS_Halt(1);
    }
    // DEBUG
    process *new_proc = &processTable[pid % MAXPROC];
    if (new_proc->state != READY) {
        USLOSS_Console("ERROR: Process %d is not ready\n", pid);
        return;
    }
    int psr = enableInterrupts();

    process *old_proc = currentProcess;
    currentProcess = new_proc;
    new_proc->state = RUNNING;

    // ONLY update status if not already FINISHED
    if (old_proc != NULL && old_proc->state != FINISHED) { 
        old_proc->state = READY; 
    }
    // first switch is null
    if(old_proc == NULL){
        USLOSS_ContextSwitch(NULL, &new_proc->context);
    }else{
        USLOSS_ContextSwitch(&old_proc->context, &new_proc->context);

    }
    restoreInterrupts(psr);
}