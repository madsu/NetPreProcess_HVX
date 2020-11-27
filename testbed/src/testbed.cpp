#include "base.h"

int main()
{
    OpenCLInit();
    ////////////////
    OpenCLRun("../src/cl/vec_add.cl", "vec_add");
    ////////////////
    OpenCLUnInit();
    return 0;
}