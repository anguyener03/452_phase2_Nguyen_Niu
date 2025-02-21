#include <usloss.h>
#include <phase1.h>

#include <stdio.h>
#include <assert.h>



/* we need to run the dispatcher if the clock interrupt fires */
static void trivial_clock_handler(int dev,void *arg)
{
    dispatcher();
}

void startup(int argc, char **argv)
{
    USLOSS_IntVec[USLOSS_CLOCK_INT] = trivial_clock_handler;

    phase1_init();
    dispatcher();
}



USLOSS_PTE *phase5_mmu_pageTable_alloc(int pid)
{
    return NULL;
}

void phase5_mmu_pageTable_free(int pid, USLOSS_PTE *page_table)
{
    assert(page_table == NULL);
}



void phase2_start_service_processes()
{
    USLOSS_Console("%s() called -- currently a NOP\n", __func__);
}

void phase3_start_service_processes()
{
    USLOSS_Console("%s() called -- currently a NOP\n", __func__);
}

void phase4_start_service_processes()
{
    USLOSS_Console("%s() called -- currently a NOP\n", __func__);
}

void phase5_start_service_processes()
{
    USLOSS_Console("%s() called -- currently a NOP\n", __func__);
}



int currentTime()
{
    /* I don't care about interrupts for this process.  If it gets interrupted,
     * then it will either give the correct value for *before* the interrupt,
     * or after.  But if the caller hasn't disabled interrupts, then either one
     * is valid
     */

    int retval;

    int usloss_rc = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &retval);
    assert(usloss_rc == USLOSS_DEV_OK);

    return retval;
}



void finish(int argc, char **argv)
{
    USLOSS_Console("%s(): The simulation is now terminating.\n", __func__);
}

void test_setup  (int argc, char **argv) {}
void test_cleanup(int argc, char **argv) {}



void TEMP_switchTo(int pid)
{
    USLOSS_Console("This function must never be called!  I simply implemented it to make sure that you had *removed* it from your phase 1 code.\n");
    assert(0);
}

