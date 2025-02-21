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

 DELETE THIS LATER - docker run -ti -v .:/root/phase2 ghcr.io/russ-lewis/usloss-m1

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
    QUIT  
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
    struct process *next; // used for run queuse
    struct process *parent;        
    struct process *first_child; 
    struct process *next_sibling;
    int exit_status;                
    int (*startFunc)(void *);
    void *arg;                       
    void *stack; 
    struct process *zapList;  // Head of zap list
    struct process *nextZap;  // Next zapper in the list
    int timeUsed; // Track time used in current quantum (in ms)
                   
} process;
/*
This struct is a runqueue
*/  
typedef struct RunQueue {
    process *head;  
    process *tail;  
} RunQueue;


/*

PROTOTYPES

*/
// Helper Functions
int isKernel(void);
void wrapper(void);
void restorePsr(int psr);
int enableInterrupts(void);
int disableInterrupts(void);

// Process Control and Scheduling
void removeChild(process *parent, process *child);
void enqueue(process *proc);
void removeFromRunQueue(process *proc);
process *dequeue(int priority);
process *select_next_process(void);
void context_switch(process *next_proc);

// Run Queue Management
void addToRunQueue(process *proc); // This is referenced in unblockProc but not implemented
void dumpRunQueue(void);
/*

GLOBAL VARIABLES

*/
process processTable[MAXPROC];
process *currentProcess;
int currentPid = 2;     
int numberOfProcesses = 0;
RunQueue run_queues[6]; 


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
    int result = USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
    if (result != 0) {
        USLOSS_Console("ERROR: Failed to set PSR in wrapper\n");
        USLOSS_Halt(1);
    }
    int status = currentProcess->startFunc(currentProcess->arg);
    quit(status);
}

/**
This helper restores the old psr
*/
void restorePsr(int psr){
    int result =  USLOSS_PsrSet(psr);
    if(result != 0){
        USLOSS_Console("ERROR: Failed to set PSR in restorePsr\n");
        USLOSS_Halt(1);
    }
}

/*
This is the function that will be called when the process is running
*/
void init_main() {

    //USLOSS_Console("[DEBUG] Process init_main called\n");
    // bootstrap
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    
    // create testcase_main process
    int test_pid = spork("testcase_main", (int (*)(void *))testcase_main, NULL, USLOSS_MIN_STACK, 3);
    
    if (test_pid < 0) {
        USLOSS_Console("ERROR: Failed to create testcase_main process\n");
        USLOSS_Halt(1);  
    }

    dispatcher();

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
        processTable[i].nextZap = NULL;
        processTable[i].zapList = NULL;
        processTable[i].timeUsed = 0;
        
    }
    for (int i = 0; i < 6; i++) {
        run_queues[i].head = NULL;
        run_queues[i].tail = NULL;
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
    enqueue(new_proc);
    numberOfProcesses+=1;
}


/**
This function creates a child process of the current process and returns the pid of the new child
*/
int spork(char *name, int(*func)(void *), void *arg, int stacksize, int priority){

    //USLOSS_Console("[DEBUG] spork() called by PID %d (%s)", currentProcess->pid, currentProcess->name);
    //USLOSS_Console("spork() creating child process with name: %s\n", name);
    // check if in user mode
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: Someone attempted to call spork while in user mode!\n");
        USLOSS_Halt(1);
    }


    int psr = disableInterrupts();

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
        restorePsr(psr);
        return -1;
    }

    // check for valid parameters
    if (!name || !func) {
        restorePsr(psr);
        return -1;
    }
    if (priority < 1 || priority > 5) {
        restorePsr(psr);
        return -1;
    }
    if (stacksize < USLOSS_MIN_STACK) {
        restorePsr(psr);
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
    
    // enqueue the new process
    enqueue(childProcess);

    if (childProcess->priority < currentProcess->priority) {        
        currentProcess->state = READY;
        enqueue(currentProcess);
        dispatcher();
    }  
    // restores the old ps 
    restorePsr(psr);
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

    int psr = disableInterrupts();

    process *curr = currentProcess->first_child; 
    while (curr != NULL) {
        if (curr->state == FINISHED || curr->state == QUIT) {
            *status = curr->exit_status; 
            int pid = curr->pid;

            removeChild(currentProcess, curr);
            numberOfProcesses--;
            restorePsr(psr);
            return pid;  
        }
        curr = curr->next_sibling;
    }
     // No terminated children found, so block the current process
     currentProcess->state = BLOCKED;

     // Call the dispatcher to switch to another process
     dispatcher();
 
     // When the process is unblocked, it will resume here
     // Now check again for terminated children
     curr = currentProcess->first_child;
     while (curr != NULL) {
         if (curr->state == FINISHED || curr->state == QUIT) {
             // Found a terminated child
             *status = curr->exit_status;
             int pid = curr->pid;
 
             // Remove the child from the parent's list
             removeChild(currentProcess, curr);
             numberOfProcesses--;
 
             // Restore interrupts and return the child's PID
             restorePsr(psr);
             return pid;
         }
         curr = curr->next_sibling;
     }
 
    restorePsr(psr);
    // should not happen
    return -2;
}

void quit(int status){
    
    //USLOSS_Console("[DEBUG] quit() called by PID %d (%s)\n", currentProcess->pid, currentProcess->name);
    
    // ensure kernal mode
    if(isKernel() != 1){
        USLOSS_Console("ERROR: Someone attempted to call quit while in user mode!\n");
        USLOSS_Halt(1);
    }
    
    // get current process
    process *curr = currentProcess;
    
    // check if the current process has children 
    if (curr->first_child != NULL) {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", curr->pid);
        USLOSS_Halt(1);
    }
     // Store exit status
     curr->exit_status = status;
     curr->state = FINISHED;
     //USLOSS_Console("[DEBUG] quit(): Process %d (%s) is quitting with status %d, state changed to %s \n", curr->pid, curr->name, status, curr->state == FINISHED ? "FINISHED" : "QUIT");
     
     // Wake up the parent if waiting in join()
     if (curr->parent && curr->parent->state == BLOCKED) {
        //USLOSS_Console("[DEBUG] quit(): Waking up parent PID %d\n", curr->parent->pid);
        curr->parent->state = READY;
        enqueue(curr->parent);  // Add parent back to the run queue
    }

     // Wake up all processes that zapped this process
    process *zapper = curr->zapList;
    while (zapper != NULL) {
        zapper->state = READY;
        addToRunQueue(zapper);
        zapper = zapper->nextZap;
    }

    // Switch context since current process is quitting   
    dispatcher(); 
    //this should never be reached
    USLOSS_Halt(1);    
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
        if (p->state != EMPTY && p->pid != -1) {  
            const char *state;
            char stateBuff[32];

            switch (p->state) {
                case RUNNING:
                    state = "Running";
                    break;
                case READY:
                    state = "Runnable";
                    break;
                case QUIT:
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

/*
this enable interrupts
*/
int enableInterrupts() {
    unsigned int old_psr = USLOSS_PsrGet();
    unsigned int new_psr = old_psr | USLOSS_PSR_CURRENT_INT;
    int result = USLOSS_PsrSet(new_psr);
    if(result != 0){
        USLOSS_Console("ERROR: Failed to set PSR in enableInterrupts\n");
        USLOSS_Halt(1);
    }
    return old_psr;
}

/**
This helper disable interrupts 
*/
int  disableInterrupts() {
    unsigned int old_psr = USLOSS_PsrGet();
    unsigned int new_psr = old_psr & ~USLOSS_PSR_CURRENT_INT;
    int result = USLOSS_PsrSet(new_psr);
    if (result != 0) {
        USLOSS_Console("ERROR: Failed to set PSR in disableInterrupts\n");
        USLOSS_Halt(1);
    }
    return old_psr;
}

void dispatcher() {
    //USLOSS_Console("[DEBUG] dispatcher(): Entering dispatcher.\n");
    // Check if the current process is in kernel mode
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: Someone attempted to call dispatcher while in user mode!\n");
        USLOSS_Halt(1);
    }
    //USLOSS_Console("[DEBUG] Dispatcher called. Current process: %d\n", currentProcess ? currentProcess->pid : -1);
    int old_psr = disableInterrupts();

    process *next_process = select_next_process();
    
    //USLOSS_Console("[DEBUG] dispatcher(): Switching to PID %d (%s)\n", next_process->pid, next_process->name);
    if (next_process != currentProcess) {
        context_switch(next_process);
    }
    restorePsr(old_psr);
}

process *select_next_process() {
    for (int priority = 0; priority < 6; priority++) {
        if (run_queues[priority].head != NULL) {
            process *next_process = run_queues[priority].head;

            // Remove the process from the head of the queue
            run_queues[priority].head = next_process->next;

            // If the queue is now empty, update the tail pointer
            if (run_queues[priority].head == NULL) {
                run_queues[priority].tail = NULL;
            }

            // Clear the next pointer of the selected process
            next_process->next = NULL;

            return next_process;
        }
    }
    // No runnable processes found
    //USLOSS_Console("[ERROR] No runnable processes found. Halting.\n");
    USLOSS_Halt(1);
    return NULL;
} 

/*
helper function to enqueue the process into the run queue
*/
void enqueue(process *proc) {
   // Enqueuing process %d (%s) in priority queue %d\n", proc->pid, proc->name, proc->priority);
 
    int priority = proc->priority - 1; 
    // eror check
    if (priority < 0 || priority >= 6) {
        return;
    }

    if (run_queues[priority].head == NULL) {
        run_queues[priority].head = proc;
        run_queues[priority].tail = proc;
    } else {
        run_queues[priority].tail->next = proc;
        run_queues[priority].tail = proc;
    }
    //USLOSS_Console("[DEBUG] DUMPING QUEUSE AFter ENQUEUE\n");
    // Ensure the process's next pointer is NULL
    proc->next = NULL;
}
/*
helper function to remove a process from the run queue
*/
void removeFromRunQueue(process *proc) {
    //USLOSS_Console("[DEBUG] Removing process %d (%s) from priority queue %d\n", proc->pid, proc->name, proc->priority);
    int priority = proc->priority - 1; 
    if (priority < 0 || priority >= 6) {
        return;
    }

    process *current = run_queues[priority].head;
    process *prev = NULL;

    while (current != NULL && current != proc) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return;
    }

    if (prev == NULL) {
        run_queues[priority].head = current->next;
        if (run_queues[priority].head == NULL) {
            run_queues[priority].tail = NULL;
        }
    } else {
        prev->next = current->next;
        if (current->next == NULL) {
            run_queues[priority].tail = prev;
        }
    }
    current->next = NULL;
}
/*
helper function to dequeue the process from the run queue
*/
process *dequeue(int priority) {
    int prio = priority - 1;
    if (run_queues[prio].head == NULL) return NULL;

    process *proc = run_queues[prio].head;
    run_queues[prio].head = proc->next;

    if (run_queues[prio].head == NULL) {
        run_queues[prio].tail = NULL; 
    }

    proc->next = NULL;
    return proc;
}
/*
This function switches the context to the new process
*/
void context_switch(process *next_proc) {
    if (currentProcess == next_proc) {
        return; 
    }

    // reset timer  
    next_proc->timeUsed = 0;

    // Update the current process pointer
    process *old_proc = currentProcess;
    currentProcess = next_proc;

    // Load the next process's state
    if(old_proc == NULL){
        USLOSS_ContextSwitch(NULL, &next_proc->context);
    }else{
        //USLOSS_Console("[DEBUG] CONTEXT Switching from PID %d (%s) to PID %d (%s)\n", currentProcess->pid, currentProcess->name, next_proc->pid, next_proc->name);
        USLOSS_ContextSwitch(&old_proc->context, &next_proc->context);
    }
}

void blockMe() {
    // Disable interrupts to avoid concurrency issues
    int old_psr = disableInterrupts();

    // Mark the current process as BLOCKED
    currentProcess->state = BLOCKED;

    // Remove the process from its run queue
    removeFromRunQueue(currentProcess);

    // Call the dispatcher to switch to another process
    dispatcher();

    // Restore interrupts
    restorePsr(old_psr);
}

int unblockProc(int pid) {
    // Disable interrupts to avoid concurrency issues
    int old_psr = disableInterrupts();

    // Find the process in the process table
    int slot = pid % MAXPROC;
    process *proc = &processTable[slot];

    // Check if the process exists and is blocked
    if (proc->state != BLOCKED || proc->pid != pid) {
        restorePsr(old_psr);
        return -2; // Process not blocked or doesn't exist
    }

    // Mark the process as RUNNABLE
    proc->state = READY;

    // Add the process back to its run queue
    addToRunQueue(proc);

    // Call the dispatcher to check if the newly unblocked process should run
    dispatcher();

    // Restore interrupts
    restorePsr(old_psr);

    return 0; // Success
}

void addToRunQueue(process *proc) {
    int priority = proc->priority - 1; // Convert priority (1-6) to index (0-5)
    if (priority < 0 || priority >= 6) {
        // Handle error: invalid priority
        return;
    }

    // Add the process to the end of the queue
    if (run_queues[priority].head == NULL) {
        // If the queue is empty, this process becomes the head
        run_queues[priority].head = proc;
        run_queues[priority].tail = proc;
    } else {
        // Otherwise, add the process to the tail
        run_queues[priority].tail->next = proc;
        run_queues[priority].tail = proc;
    }

    // Ensure the process's next pointer is NULL
    proc->next = NULL;
}

void zap(int pid) {
    if (isKernel() != 1) {
        USLOSS_Console("ERROR: Someone attempted to call zap while in user mode!\n");
        USLOSS_Halt(1);
    }

    process *current = &processTable[currentPid];

    // Check if the target PID is the same as the current process's PID
    if (pid == currentProcess->pid) {
        USLOSS_Console("ERROR: Attempt to zap() itself.\n");
        USLOSS_Halt(1);
    }

     // Check if the target PID is 1
     if (pid == 1) {
        USLOSS_Console("ERROR: Attempt to zap() init.\n");
        USLOSS_Halt(1);
    }
    // Find the target process in the process table
    int slot = pid % MAXPROC;
    process *target = &processTable[slot];

    // Check if the target process exists
    if (target->state == EMPTY || target->pid != pid) {
        USLOSS_Console("ERROR: Attempt to zap() a non-existent process.\n", pid);
        USLOSS_Halt(1);
    }

    // Check if the target process has already terminated
    if (target->state == FINISHED || target->state == QUIT) {
        USLOSS_Console("ERROR: Attempt to zap() a process that is already in the process of dying.\n");
        USLOSS_Halt(1);
        }
    // Add the current process to the zap list of the target
    current->nextZap = target->zapList;
    target->zapList = current;

    // Block the zapper
    current->state = BLOCKED;
    dispatcher();
}

void dumpRunQueue() {
    USLOSS_Console("\n=========================\n");
    USLOSS_Console(" DUMPING RUN QUEUES\n");
    USLOSS_Console("=========================\n");

    for (int priority = 0; priority < 6; priority++) {
        USLOSS_Console("[Priority %d] -> ", priority + 1);
        process *current = run_queues[priority].head;
        
        if (current == NULL) {
            USLOSS_Console("(empty)");
        }

        while (current != NULL) {
            USLOSS_Console("PID %d (%s) -> ", current->pid, current->name);
            current = current->next;
        }
        USLOSS_Console("NULL\n");
    }
    USLOSS_Console("=========================\n\n");
}
