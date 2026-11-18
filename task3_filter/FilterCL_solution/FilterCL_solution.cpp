// FilterCL_solution.cpp — ПРЕПОДАВАТЕЛЬСКАЯ ВЕРСИЯ (ответы к шаблону).
// Отмечено: === ОТВЕТ TODO n ===

#include <iostream>
#include <fstream>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <cstdio>
#include <chrono>   // steady_clock: монотонные часы с наносекундным разрешением

#define CHECK(err, what) \
    if ((err) != CL_SUCCESS) { \
        std::cout << "OpenCL error (" << what << "): " << (err) << std::endl; \
        return 1; \
    }

// ============================================================
// === ОТВЕТ TODO 1 ===
// ============================================================
const char* kernelSource =
"__kernel void filter(__global const float *A,\n"
"                     __global const float *B0,\n"
"                     __global const float *B1,\n"
"                     __global const int   *sel,\n"
"                     __global       float *R,\n"
"                     const int M,\n"
"                     const int K)\n"
"{\n"
"    int pos = get_global_id(0);\n"
"    int ch  = get_global_id(1);\n"
"\n"
"    int pad = (K - 1) / 2;\n"
"    __global const float* b = sel[ch] ? B1 : B0;\n"
"    float r = 0.0f;\n"
"    if (pos >= pad && pos < M - pad) {\n"
"        __global const float* win = A + (size_t)ch * M + (pos - pad);\n"
"        for (int k = 0; k < K; k++)\n"
"            r += win[k] * b[k];\n"
"    }\n"
"    R[(size_t)ch * M + pos] = r;\n"
"}\n";

int main()
{
    const char* filename = "input.txt";

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "file error " << filename << "!" << std::endl;
        return 1;
    }

    int N, M, K;
    file >> N >> M >> K;

    float* b0 = new float[K];
    float* b1 = new float[K];
    double d;
    for (int i = 0; i < K; i++) { file >> d; b0[i] = (float)d; }
    for (int i = 0; i < K; i++) { file >> d; b1[i] = (float)d; }

    // В условии опечатка ("1 или 1"): на самом деле значения 1 или 2.
    int* sel = new int[N];
    for (int i = 0; i < N; i++) {
        int v; file >> v;
        sel[i] = v - 1;                 // 1 или 2 -> индекс 0 или 1
    }

    float* a = new float[(size_t)N * M];
    for (size_t i = 0; i < (size_t)N * M; i++) {
        file >> d;
        a[i] = (float)d;
    }
    file.close();

    float* r = new float[(size_t)N * M];

    cl_int err;
    cl_platform_id platform;
    cl_device_id device;
    CHECK(clGetPlatformIDs(1, &platform, NULL), "GetPlatformIDs");
    CHECK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL), "GetDeviceIDs");
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err); CHECK(err, "CreateContext");
#ifdef __APPLE__
    cl_command_queue queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
#else
    // OpenCL 2.0+: массив пар "ключ -> значение", 0 - терминатор;
    // значение ключа CL_QUEUE_PROPERTIES - битовое поле свойств очереди
    cl_queue_properties props[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, &err);
#endif
    CHECK(err, "CreateQueue");

    cl_program program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    CHECK(err, "CreateProgram");
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char log[8192];
        log[0] = '\0';
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
        std::cout << "kernel build failed:\n" << log << std::endl;
        return 1;
    }
    cl_kernel kernel = clCreateKernel(program, "filter", &err);
    CHECK(err, "CreateKernel");

    cl_mem bufA  = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)N * M, NULL, &err); CHECK(err, "bufA");
    cl_mem bufB0 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)K,     NULL, &err); CHECK(err, "bufB0");
    cl_mem bufB1 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)K,     NULL, &err); CHECK(err, "bufB1");
    cl_mem bufS  = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int)    * (size_t)N,    NULL, &err); CHECK(err, "bufSel");
    cl_mem bufR  = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * (size_t)N * M, NULL, &err); CHECK(err, "bufR");

    auto tStart = std::chrono::steady_clock::now();

    clEnqueueWriteBuffer(queue, bufA,  CL_TRUE, 0, sizeof(float) * (size_t)N * M, a,  0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufB0, CL_TRUE, 0, sizeof(float) * (size_t)K,     b0, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufB1, CL_TRUE, 0, sizeof(float) * (size_t)K,     b1, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufS,  CL_TRUE, 0, sizeof(int)    * (size_t)N,    sel, 0, NULL, NULL);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB0);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufB1);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &bufS);
    clSetKernelArg(kernel, 4, sizeof(cl_mem), &bufR);
    clSetKernelArg(kernel, 5, sizeof(int),    &M);
    clSetKernelArg(kernel, 6, sizeof(int),    &K);

    // === ОТВЕТ TODO 2 ===
    size_t global[2] = { (size_t)M, (size_t)N };
    cl_event kernelEvent = NULL;
    clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, NULL, &kernelEvent);

    clFinish(queue);

    clEnqueueReadBuffer(queue, bufR, CL_TRUE, 0, sizeof(float) * (size_t)N * M, r, 0, NULL, NULL);

    double tPipelineMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - tStart).count();

    cl_ulong tKernelStart = 0, tKernelEnd = 0;
    if (kernelEvent != NULL &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &tKernelStart, NULL) == CL_SUCCESS &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &tKernelEnd,   NULL) == CL_SUCCESS)
    {
        std::cout << "GPU kernel only: " << (tKernelEnd - tKernelStart) << " ns ("
                  << (tKernelEnd - tKernelStart) / 1000.0 << " us)" << std::endl;
        clReleaseEvent(kernelEvent);
    }

    if ((size_t)N * M <= 100) {
        for (int ch = 0; ch < N; ch++) {
            std::cout << "channel " << ch + 1 << ": ";
            for (int pos = 0; pos < M; pos++)
                std::printf("%g ", r[(size_t)ch * M + pos]);
            std::cout << std::endl;
        }
    }

    // === ОТВЕТ TODO 3 ===
    double res = 0.0;
    for (size_t i = 0; i < (size_t)N * M; i++)
        res += r[i];
    std::printf("result: %.4f\n", res / ((double)N * M));

    std::cout << "GPU time (transfer + kernel): " << tPipelineMs << " ms" << std::endl;

    delete[] a; delete[] b0; delete[] b1; delete[] sel; delete[] r;
    clReleaseMemObject(bufA);  clReleaseMemObject(bufB0); clReleaseMemObject(bufB1);
    clReleaseMemObject(bufS);  clReleaseMemObject(bufR);
    clReleaseKernel(kernel);   clReleaseProgram(program);
    clReleaseCommandQueue(queue); clReleaseContext(context);
    return 0;
}
