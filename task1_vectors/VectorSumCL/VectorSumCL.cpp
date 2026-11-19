// VectorSumCL.cpp — GPU-версия (Задача 1: разбор в классе).
// Полная рабочая версия: считаем поэлементную сумму двух векторов на GPU.
//
// Идея: на CPU один исполнитель крутит цикл for по всем i.
//       На GPU исполнителей столько же, сколько элементов,
//       и у каждого цикл состоит из ОДНОЙ итерации.
//       Номер своей итерации исполнитель узнает через get_global_id(0).

#include <iostream>
#include <fstream>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <chrono>   // steady_clock: монотонные часы с наносекундным разрешением

#define CHECK(err, what) \
    if ((err) != CL_SUCCESS) { \
        std::cout << "OpenCL error (" << what << "): " << (err) << std::endl; \
        return 1; \
    }

// ============================================================
// ЯДРО — функция, которую параллельно исполняет каждый "исполнитель" GPU.
// Подпись готова (аргументы A, B, C, N переданы ниже через clSetKernelArg).
// Все ядро — это ОДНА итерация старого цикла for.
// ============================================================
const char* kernelSource =
"__kernel void sum(__global const int *A,\n"
"                  __global const int *B,\n"
"                  __global       int *C,\n"
"                  const int N)\n"
"{\n"
"    int i = get_global_id(0);   // номер элемента = номер исполнителя\n"
"    C[i] = A[i] + B[i];         // одна итерация цикла из CPU-версии\n"
"}\n";

int main()
{
    const char* filename = "vector.txt";

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "file error " << filename << "!" << std::endl;
        return 1;
    }

    int N = 0;
    file >> N;                        // первая строка файла — количество пар

    int* A = new int[N];
    int* B = new int[N];
    int* C = new int[N];

    for (int i = 0; i < N; i++)
        file >> A[i] >> B[i];
    file.close();

    // --- OpenCL setup (дано, не меняйте) ---
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
    // в macOS SDK доступна только версия API из OpenCL 1.2;
    // включаем профилирование, чтобы измерять время ядра через событие
    queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
#else
    // OpenCL 2.0+: массив пар "ключ -> значение", 0 - терминатор;
    // значение ключа CL_QUEUE_PROPERTIES - битовое поле свойств очереди
    cl_queue_properties props[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
    queue = clCreateCommandQueueWithProperties(context, device, props, &err);
#endif
    CHECK(err, "CreateQueue");

    // --- Компиляция ядра (дано, не меняйте) ---
    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    CHECK(err, "CreateProgram");

    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        std::cout << "Kernel build failed." << std::endl;
        return 1;
    }
    kernel = clCreateKernel(program, "sum", &err);
    CHECK(err, "CreateKernel");

    // --- Память на устройстве (дано, не меняйте) ---
    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * N, NULL, &err); CHECK(err, "bufA");
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * N, NULL, &err); CHECK(err, "bufB");
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * N, NULL, &err); CHECK(err, "bufC");

    size_t global = N;   // количество "исполнителей" = количество элементов

    // --- Измерение 1: полный цикл (обмен данными + ядро) ---
    auto tStart = std::chrono::steady_clock::now();

    // копируем входные данные на устройство
    clEnqueueWriteBuffer(queue, bufA, CL_TRUE, 0, sizeof(int) * N, A, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufB, CL_TRUE, 0, sizeof(int) * N, B, 0, NULL, NULL);

    // передаем ядру его аргументы (в порядке объявления в kernelSource)
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);
    clSetKernelArg(kernel, 3, sizeof(int),    &N);

    cl_event kernelEvent = NULL;   // по этому событию меряется время ядра

    // запуск ядра: N исполнителей, одномерный NDRange.
    // Последний аргумент &kernelEvent нужен для измерения времени ядра.
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, &kernelEvent);

    clFinish(queue);   // ждем, пока GPU завершит все команды

    // копируем результат обратно с устройства
    clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, sizeof(int) * N, C, 0, NULL, NULL);

    double tPipelineNs = std::chrono::duration<double, std::nano>(
                             std::chrono::steady_clock::now() - tStart).count();

    // --- Измерение 2: только ядро, по событию (дано, не меняйте) ---
    cl_ulong tKernelStart = 0, tKernelEnd = 0;
    if (kernelEvent == NULL ||
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &tKernelStart, NULL) != CL_SUCCESS ||
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &tKernelEnd,   NULL) != CL_SUCCESS)
    {
        std::cout << "No profiling data: &kernelEvent was not passed in TODO 2" << std::endl;
    } else {
        std::cout << "GPU kernel only: " << (tKernelEnd - tKernelStart) << " ns" << std::endl;
        clReleaseEvent(kernelEvent);
    }

    // --- Вывод результата ---
    for (int i = 0; i < 10 && i < N; i++)
        std::cout << C[i] << " ";
    std::cout << (N > 10 ? "..." : "") << std::endl;

    std::cout << "GPU time (transfer + kernel): " << (long long)tPipelineNs << " ns" << std::endl;

    // --- Очистка (дано, не меняйте) ---
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
