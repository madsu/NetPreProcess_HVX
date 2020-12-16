#include "base_engine.h"

#include <stdio.h>

#if defined(__hexagon__)
#include "hexagon_standalone.h"
#include "subsys.h"
#endif

#include "io.h"
#include "hvx.cfg.h"

void PrintVec(short *vec, unsigned int len)
{
    printf("print vec: ");
    for (int i = 0; i < 10; ++i) {
        printf("%d ", vec[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    long long start_time, total_cycles;

    unsigned int len = 32 * 10;
    short *vec_buf = (short *)malloc(sizeof(short) * len);

    srand(time(NULL));
    for (int i = 0; i < len; ++i) {
        vec_buf[i] = (short)(rand());
    }

#if defined(__hexagon__)
    subsys_enable();
    SIM_ACQUIRE_HVX;
#if LOG2VLEN == 7
    SIM_SET_HVX_DOUBLE_MODE;
#endif
#endif 

    /* -----------------------------------------------------*/
    /*  Call fuction                                        */
    /* -----------------------------------------------------*/
    PrintVec(vec_buf, len);

    RESET_PMU();
    start_time = READ_PCYCLES();

    vec_abs(vec_buf, len);

    total_cycles = READ_PCYCLES() - start_time;
    DUMP_PMU();

    PrintVec(vec_buf, len);

    printf("run cycles %d\n", total_cycles);

    free(vec_buf);
    return 0;
}