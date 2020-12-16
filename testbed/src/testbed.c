#include "base_engine.h"

#include <stdio.h>

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
    unsigned int len = 32 * 10;
    short *vec_buf = (short *)malloc(sizeof(short) * len);

    srand(time(NULL));
    for (int i = 0; i < len; ++i) {
        vec_buf[i] = (short)(rand());
    }

    PrintVec(vec_buf, len);
    vec_abs(vec_buf, len);
    PrintVec(vec_buf, len);

    free(vec_buf);
    return 0;
}