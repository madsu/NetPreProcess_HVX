#include "base_engine.h"

#include <chrono>
#include <gtest/gtest.h>

class ccosttime {
public:
    ccosttime(const char* info)
    {
        strcpy(info_, info);
        start = std::chrono::system_clock::now();
    }

    ~ccosttime()
    {
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        printf("%s Elapsed time: %ld ms\n", info_, millis);
    }

private:
    char info_[1024];
    std::chrono::time_point<std::chrono::system_clock> start, end;
};

class OpenCLTest : public ::testing::Test {
protected:
    OpenCLTest()
    {
        OpenCLInit();
    }

    ~OpenCLTest() override
    {
        OpenCLUnInit();
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

void GenerateRandom(float* array, int len)
{
    srand(time(nullptr));
    for(int i = 0; i < len; ++i) {
        array[i] = static_cast<float>(rand());
    }
}

TEST_F(OpenCLTest, CheckPrecision)
{
    cl_kernel kernel = GetOpenClKernel("../src/cl/vec_add.cl", "vector_add");
    //////////////////////////
    constexpr int len = 10240;
    auto arrayA = std::make_unique<float[]>(len);
    auto arrayB = std::make_unique<float[]>(len);
    GenerateRandom(arrayA.get(), len);
    GenerateRandom(arrayB.get(), len);

    cl_mem bufA = CreateBuffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, len * sizeof(float), arrayA.get());
    cl_mem bufB = CreateBuffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, len * sizeof(float), arrayB.get());

    int idx = 0;
    clSetKernelArg(kernel, idx++, sizeof(cl_mem), &bufA);
    clSetKernelArg(kernel, idx++, sizeof(cl_mem), &bufB);

    auto arrayC = std::make_unique<float[]>(len);
    auto resultC = std::make_unique<float[]>(len);
    cl_mem bufC = CreateBuffer(CL_MEM_WRITE_ONLY , len * sizeof(float), nullptr); 
    clSetKernelArg(kernel, idx++, sizeof(cl_mem), &bufC);

    {
        ccosttime a("cpu run");
        int num = 100;
        while (num-- > 0) {
            for (int i = 0; i < len; ++i) {
                arrayC[i] = arrayA[i] + arrayB[i];
            }
        }
    }

    std::vector<size_t> gws{len};
    {
        ccosttime a("gpu run");
        int num = 100;
        while (num-- > 0) {
            RunKernel(kernel, gws);
            ReadBuffer(bufC, resultC.get(), sizeof(float) * len);
        }
    }

    for(int i = 0; i < len; ++i) {
        EXPECT_FLOAT_EQ(arrayC[i], resultC[i]);
    }

    //////////////////////////
    ReleaseOpenCLKernel(kernel);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}