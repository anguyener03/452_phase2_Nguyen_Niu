#include <stdio.h>
#include <usloss.h>
#include <phase1.h>



int XXp1(void *), XXp2(void *), XXp3(void *), XXp4(void *);
int pid1;

int testcase_main()
{
    int status, pid2, kidpid;

    USLOSS_Console("testcase_main(): started\n");
// TODO    USLOSS_Console("EXPECTATION: TBD\n");
    USLOSS_Console("QUICK SUMMARY: Some interactions of fork/join/zap.  Because of priorities, the processes run such that zap() will produce an error.\n");

    pid1 = spork("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, 1);
    USLOSS_Console("testcase_main(): after fork of child %d\n", pid1);

    pid2 = spork("XXp2", XXp2, "XXp2", USLOSS_MIN_STACK, 2);
    USLOSS_Console("testcase_main(): after fork of child %d\n", pid2);

    USLOSS_Console("testcase_main(): performing first join\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): exit status for child %d is %d\n", kidpid, status);

    USLOSS_Console("testcase_main(): performing second join\n");
    kidpid = join(&status);
    USLOSS_Console("testcase_main(): exit status for child %d is %d\n", kidpid, status);

    return 0;
}

int XXp1(void *arg)
{
    int pid3, kidpid, status;

    USLOSS_Console("XXp1(): started\n");
    USLOSS_Console("XXp1(): arg = '%s'\n", arg);

    pid3 = spork("XXp3", XXp3, "XXp3FromXXp1", USLOSS_MIN_STACK, 2);
    USLOSS_Console("XXp1(): after fork of child %d\n", pid3);

    USLOSS_Console("XXp1(): performing first join at this point\n");
    kidpid = join(&status);
    USLOSS_Console("XXp1(): exit status for child %d is %d\n", kidpid, status);

    quit(1);
}

int XXp2(void *arg)
{
    USLOSS_Console("XXp2(): started\n");
    USLOSS_Console("XXp2(): arg = '%s'\n", arg);

    USLOSS_Console("XXp2(): calling zap(%d)\n", pid1);
    zap(pid1);
    USLOSS_Console("XXp2(): zap(%d) returned\n", pid1);

    quit(2);
}

int XXp3(void *arg)
{
    USLOSS_Console("XXp3(): started\n");
    USLOSS_Console("XXp3(): arg = '%s'\n", arg);

    quit(3);
}

