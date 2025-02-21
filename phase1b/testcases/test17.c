/*
 * Check that getpid() functions correctly.
 */

#include <stdio.h>
#include <usloss.h>
#include <phase1.h>

#define FLAG1 2

int XXp1(void *), XXp2(void *), XXp3(void *), XXp4(void *);
int pidlist[5];

int testcase_main()
{
    int ret1, ret2, ret3;
    int status;

    USLOSS_Console("testcase_main(): started\n");
    USLOSS_Console("EXPECTATION: testcase_main() will create 3 children, all running XXp1().  They have priority 3, so that they will not interrupt testcase_main().  The PID of each child is stored into a global array.  Then testcase_main() blocks in join() (three times).  The child processes should run in the same order they were created (we use a FIFO for ordering dispatch), and so each can call getpid() to confirm that it has the same value as stored in the global array.\n");

    /* BUGFIX: This testcase originally created children of the same priority
     *         as the parent.  So it's possible that they might run before
     *         spork() returns - although my implementation doesn't do that.
     *
     *         The fix for this is to run them at *lower* priority than the
     *         parent, so the parent is guaranteed to fill out the array pidlist[]
     *         before any child runs.
     */

    pidlist[0] = spork("XXp1", XXp1, NULL, USLOSS_MIN_STACK, 4);
    pidlist[1] = spork("XXp1", XXp1, NULL, USLOSS_MIN_STACK, 4);
    pidlist[2] = spork("XXp1", XXp1, NULL, USLOSS_MIN_STACK, 4);

    USLOSS_Console("testcase_main(): pidlist[] = [%d,%d,%d, ...]\n", pidlist[0],pidlist[1],pidlist[2]);

    ret1 = join(&status);
    USLOSS_Console("testcase_main: joined with child %d\n", ret1);

    ret2 = join(&status);
    USLOSS_Console("testcase_main: joined with child %d\n", ret2);

    ret3 = join(&status);
    USLOSS_Console("testcase_main: joined with child %d\n", ret3);

    return 0;
}

int XXp1(void *arg)
{
    static int i = 0;

    USLOSS_Console("One of the XXp1() process has started, getpid()=%d\n", getpid());

    if (getpid() != pidlist[i])
    {
        USLOSS_Console("************************************************************************");
        USLOSS_Console("* TESTCASE FAILED: getpid() returned different value than spork() did. *");
        USLOSS_Console("************************************************************************");
    }
    else
        USLOSS_Console("This process's getpid() matched what spork() returned.\n");

    i++;

    USLOSS_Console("This XXp1() process will now terminate.\n");
    quit(FLAG1);
}

