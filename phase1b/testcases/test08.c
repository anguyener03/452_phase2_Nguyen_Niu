/* This test is to check whether fork tests for the mode being == kernel */

#include <stdio.h>
#include <usloss.h>
#include <phase1.h>

int XXp1(void *);

int testcase_main()
{
    int status, pid1, kidpid;
    int result;

    USLOSS_Console("testcase_main(): started\n");
    USLOSS_Console("EXPECTATION: The simulation should be terminated as soon as fork() is called, since we are not in kernel mode.\n");

    /* sanity check: are interrupts enabled so far? */
    if ( (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT) == 0)
    {
        USLOSS_Console("ERROR: It looks like you are running testcase_main() with interrupts disabled, instead of enabled.  Fix that!\n");
        USLOSS_Halt(1);
    }

    result = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    if ( result != USLOSS_DEV_OK ) {
        USLOSS_Console("testcase_main: USLOSS_PsrSet returned %d\n", result);
        USLOSS_Console("Halting...\n");
        USLOSS_Halt(1);
    }
    pid1 = spork("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, 2);
    USLOSS_Console("testcase_main(): after fork of child %d\n", pid1);
    USLOSS_Console("testcase_main(): performing join\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): exit status for child %d is %d\n", kidpid, status); 

    return 0;
}

int XXp1(void *arg)
{
    USLOSS_Console("XXp1(): started -- should not see this message!!!!!!!!\n");
    return 1;
}

