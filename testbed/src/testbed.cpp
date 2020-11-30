#include "base.h"

int main()
{
    OpenCLInit();
    ////////////////
    OpenCLRun("../src/cl/vec_add.cl", "vector_add");
    ////////////////
    OpenCLUnInit();
    return 0;
}