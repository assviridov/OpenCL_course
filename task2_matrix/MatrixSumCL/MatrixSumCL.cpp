// MatrixSumCL.cpp — GPU-версия (Задача 2: разбор в классе).
// Новое относительно задачи 1: ДВУМЕРНАЯ сетка исполнителей (2D NDRange).
//   - один work-item = один ЭЛЕМЕНТ матрицы;
//   - get_global_id(0) -> номер столбца (быстрое измерение),
//     get_global_id(1) -> номер строки;
//   - размер запуска: {N столбцов, M строк}.

#include <iostream>
#include <chrono>   // steady_clock: монотонные часы с наносекундным разрешением
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <cstdlib>

#define CHECK(err, what) \
    if ((err) != CL_SUCCESS) { \
        std::cout << "OpenCL error (" << what << "): " << (err) << std::endl; \
        return 1; \
    }

// ЯДРО: каждый исполнитель обрабатывает ровно один элемент матрицы.
// Сравните с циклом for(i) for(j) из CPU-версии:
//   внешний цикл превратился в get_global_id(1) (строка),
//   внутренний            — в get_global_id(0) (столбец).
const char* kernelSource =
"__kernel void sum(__global const int *A,\n"
"                  __global const int *B,\n"
"                  __global       int *C,\n"
"                  const int M,\n"
"                  const int N)\n"
"{\n"
"    int col = get_global_id(0);            // номер столбца\n"
"    int row = get_global_id(1);            // номер строки\n"
"    int i = row * N + col;                 // тот же индекс, что в CPU-версии\n"
"    C[i] = A[i] + B[i];\n"
"}\n";

void print(const int* X, int m, int n, const char* name)
{
    std::cout << name << ":" << std::endl;
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++)
            std::cout << X[i * n + j] << "\t";
        std::cout << std::endl;
    }
}

int main()
{
    const int M = 8;   // строк  — как в CPU-версии!
    const int N = 5;   // столбцов

    int* A = new int[M * N];
    int* B = new int[M * N];
    int* C = new int[M * N];

    std::srand(42);   // те же данные, что и в CPU-версии
    for (int i = 0; i < M * N; i++) {
        A[i] = std::rand() % 201 - 100;
        B[i] = std::rand() % 201 - 100;
    }

    // --- OpenCL setup (как в задаче 1) ---
    cl_int err;
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;

    CHECK(clGetPlatformIDs(1, &platform, NULL), "GetPlatformIDs");
    CHECK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL), "GetDeviceIDs");

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    CHECK(err, "CreateContext");

#ifdef __APPLE__
    queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
#else
    // OpenCL 2.0+: массив пар "ключ -> значение", 0 - терминатор;
    // значение ключа CL_QUEUE_PROPERTIES - битовое поле свойств очереди
    cl_queue_properties props[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
    queue = clCreateCommandQueueWithProperties(context, device, props, &err);
#endif
    CHECK(err, "CreateQueue");

    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    CHECK(err, "CreateProgram");
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        std::cout << "kernel build failed:\n" <<  std::endl;
        return 1;
    }
    kernel = clCreateKernel(program, "sum", &err);
    CHECK(err, "CreateKernel");

    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * M * N, NULL, &err); CHECK(err, "bufA");
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * M * N, NULL, &err); CHECK(err, "bufB");
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * M * N, NULL, &err); CHECK(err, "bufC");

    // --- Измерение 1: полный цикл (обмен данными + ядро) ---
    auto tStart = std::chrono::steady_clock::now();

    clEnqueueWriteBuffer(queue, bufA, CL_TRUE, 0, sizeof(int) * M * N, A, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufB, CL_TRUE, 0, sizeof(int) * M * N, B, 0, NULL, NULL);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);
    clSetKernelArg(kernel, 3, sizeof(int),    &M);
    clSetKernelArg(kernel, 4, sizeof(int),    &N);

    // ГЛАВНОЕ ОТЛИЧИЕ ЗАДАЧИ 2: двумерный размер запуска.
    // размерность 0 (быстрая, соседние id отличаются на 1) -> столбцы,
    // размерность 1                                       -> строки.
    size_t global[2] = { (size_t)N, (size_t)M };

    cl_event kernelEvent = NULL;
    clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, NULL, &kernelEvent);
    clFinish(queue);

    clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, sizeof(int) * M * N, C, 0, NULL, NULL);

    double tPipelineNs = std::chrono::duration<double, std::nano>(
                             std::chrono::steady_clock::now() - tStart).count();

    // --- Измерение 2: только ядро, по событию ---
    cl_ulong tKernelStart = 0, tKernelEnd = 0;
    if (kernelEvent != NULL &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &tKernelStart, NULL) == CL_SUCCESS &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &tKernelEnd,   NULL) == CL_SUCCESS)
    {
        std::cout << "GPU kernel only: " << (tKernelEnd - tKernelStart) << " ns" << std::endl;
        clReleaseEvent(kernelEvent);
    }

    print(C, M, N, "C = A+B");
    std::cout << "GPU time (transfer + kernel): " << (long long)tPipelineNs << " ns" << std::endl;

    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    delete[] A;
    delete[] B;
    delete[] C;
    return 0;
}
