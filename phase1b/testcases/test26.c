#include <stdio.h>
#include <usloss.h>
#include <phase1.h>

/*
 * The purpose of this test is to demonstrate that
 * an attempt to join by a process with no children
 * or no unjoined children returns -2.
 *
 * Expected output:
 * testcase_main(): started
 * testcase_main(): after fork of child 3
 * testcase_main(): after fork of child 4
 * testcase_main(): performing first join
 * XXp1(): started
 * XXp1(): arg = 'XXp1'
 * XXp1(): performing join with no children
 * XXp1(): value returned by join is -2 expected value was -2
 * testcase_main(): exit status for child 3 is -1
 * testcase_main(): performing second join
 * XXp2(): started
 * XXp2(): arg = 'XXp2'
 * testcase_main(): exit status for child 4 is -2
 * testcase_main(): performing third join, have no unjoined children
 * testcase_main(): value returned by join is -2 expected value was -2
 */

int XXp1(void *), XXp2(void *), XXp3(void *);

int testcase_main()
{
    int status, pid1, pid2, kidpid;

    USLOSS_Console("testcase_main(): started\n");
// TODO    USLOSS_Console("EXPECTATION: TBD\n");
    USLOSS_Console("QUICK SUMMARY: Checking to see what happens when you call join() with no children at all.\n");

    pid1 = spork("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, 2);
    USLOSS_Console("testcase_main(): after fork of child %d\n", pid1);

    pid2 = spork("XXp2", XXp2, "XXp2", USLOSS_MIN_STACK, 2);
    USLOSS_Console("testcase_main(): after fork of child %d\n", pid2);

    USLOSS_Console("testcase_main(): performing first join\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): exit status for child %d is %d\n", kidpid, status);

    USLOSS_Console("testcase_main(): performing second join\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): exit status for child %d is %d\n", kidpid, status);

    USLOSS_Console("testcase_main(): performing third join, have no unjoined children\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): value returned by join is %d expected value was -2\n", kidpid);

    return 0;
}

int XXp1(void *arg)
{
    int status, kidpid;

    USLOSS_Console("XXp1(): started\n");
    USLOSS_Console("XXp1(): arg = '%s'\n", arg);

    USLOSS_Console("XXp1(): performing join with no children\n");
    kidpid = join(&status);
    USLOSS_Console("XXp1(): value returned by join is %d expected value was -2\n", kidpid);

    quit(1);
}

int XXp2(void *arg)
{
    USLOSS_Console("XXp2(): started\n");
    USLOSS_Console("XXp2(): arg = '%s'\n", arg);

    quit(2);
}

