#include "subsys.h"
#include "hwio.h"

void subsys_enable()
{
        int i;
        /* From HPG 4.7.8 */
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_CLK_CTL,QDSP6SS_CP_CLK_CTL_DISABLE);
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_RESET,QDSP6SS_CP_RESET_ASSERT);
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_PWR_CTL,QDSP6SS_CP_PWR_CTL_CLAMP_IO_ON);
        for (i = 0; i < 100; i++) asm volatile (" nop " ) ;
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_PWR_CTL,QDSP6SS_CP_PWR_CTL_CLAMP_IO_OFF);
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_RESET,QDSP6SS_CP_RESET_DEASSERT);
        HWIO_OUT(QDSP6SS_PUB_BASE+QDSP6SS_CP_CLK_CTL,QDSP6SS_CP_CLK_CTL_ENABLE);
}
