// FilterCL.cpp — фильтрация сигнала на GPU (СТУДЕНЧЕСКИЙ ШАБЛОН)
// Задание: заполните TODO 1, TODO 2 и TODO 3.
//
// Как проверять:
//   Часть A: запустить на example.txt (M=5, K=3 из условия) — массив r
//            должен совпасть с иллюстрацией; result = 3.1500.
//   Часть B: сгенерировать данные (FilterGen) и сверить result с FilterCheck.
//
// Вспоминайте задачи из занятия:
//   сетка (канал x позиция) — та же топология, что у матрицы 8x5,
//   только в каждой клетке сетки дополнительно цикл по K.

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
// ЯДРО. Один work-item = одна позиция окна одного канала.
// Сетка запуска: размерность 0 — позиции pos (0..M-1),
//                размерность 1 — каналы    ch  (0..N-1).
// Окно размера K с центром в позиции pos покрывает отсчеты
// [pos - half, pos + half], где half = (K-1)/2.
// Если окно выходит за границы канала — r = 0.
// Иначе r = сумма произведений отсчетов окна на коэффициенты фильтра.
// ============================================================
const char* kernelSource =
"__kernel void filter(__global const float *A,   // отсчеты: канал ch, строка длины M\n"
"                     __global const float *B0,  // коэффициенты фильтра 1 (K штук)\n"
"                     __global const float *B1,  // коэффициенты фильтра 2 (K штук)\n"
"                     __global const int   *sel, // 0 или 1 - какой фильтр у канала\n"
"                     __global       float *R,   // результат: тот же размер, что и A\n"
"                     const int M,\n"
"                     const int K)\n"
"{\n"
"    int pos = get_global_id(0);        // позиция (центр окна)\n"
"    int ch  = get_global_id(1);        // канал\n"
"\n"
"    // TODO 1: вычислите r для позиции pos канала ch и запишите в R.\n"
"    // Шаг 1. pad = (K-1)/2. ВНИМАНИЕ: имя half в OpenCL занято (тип данных)!\n"
"    // Шаг 2. Выберите фильтр: sel[ch]==0 -> B0, иначе -> B1.\n"
"    // Шаг 3. Если окно выходит за границы канала (pos < pad\n"
"    //        или pos >= M - pad) — r = 0. Это краевые отсчеты.\n"
"    // Шаг 4. Иначе r = сумма win[k]*b[k], k = 0..K-1,\n"
"    //        где win = A + ch*M + pos - pad  (начало окна).\n"
"    // Шаг 5. R[ch*M + pos] = r;   (не забудьте привести индексы к size_t)\n"
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

    // --- OpenCL setup (дано, не меняйте; как на занятии) ---
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
        std::cout << "Kernel build failed. Most likely TODO 1 is not filled.\n"
                  << "--- compiler log ---\n" << log << std::endl;
        return 1;
    }
    cl_kernel kernel = clCreateKernel(program, "filter", &err);
    CHECK(err, "CreateKernel");

    // --- Память на устройстве (дано, не меняйте) ---
    cl_mem bufA  = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)N * M, NULL, &err); CHECK(err, "bufA");
    cl_mem bufB0 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)K,     NULL, &err); CHECK(err, "bufB0");
    cl_mem bufB1 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float) * (size_t)K,     NULL, &err); CHECK(err, "bufB1");
    cl_mem bufS  = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int)    * (size_t)N,    NULL, &err); CHECK(err, "bufSel");
    cl_mem bufR  = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * (size_t)N * M, NULL, &err); CHECK(err, "bufR");

    // --- Измерение: полный цикл (обмен данными + ядро) ---
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

    cl_event kernelEvent = NULL;   // по этому событию меряется время ядра

    // TODO 2: запустите ядро. Исполнителей M*N (позиции x каналы),
    // двумерный NDRange — вспомните задачу 2 (сумма матриц).
    // Подсказка: size_t global[2] = { ___, ___ };
    //            clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, NULL, &kernelEvent);
    // ВАЖНО: последний аргумент &kernelEvent обязателен — без него
    // измерение времени ядра (ниже) работать не будет.

    clFinish(queue);

    // копируем результат обратно (дано)
    clEnqueueReadBuffer(queue, bufR, CL_TRUE, 0, sizeof(float) * (size_t)N * M, r, 0, NULL, NULL);

    double tPipelineNs = std::chrono::duration<double, std::nano>(
                             std::chrono::steady_clock::now() - tStart).count();

    // --- Измерение: только ядро, по событию (дано, не меняйте) ---
    cl_ulong tKernelStart = 0, tKernelEnd = 0;
    if (kernelEvent != NULL &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &tKernelStart, NULL) == CL_SUCCESS &&
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &tKernelEnd,   NULL) == CL_SUCCESS)
    {
        std::cout << "GPU kernel only: " << (tKernelEnd - tKernelStart) << " ns" << std::endl;
        clReleaseEvent(kernelEvent);
    } else {
        std::cout << "No profiling data: &kernelEvent was not passed in TODO 2" << std::endl;
    }

    // полный цикл: обмен данными + ядро. Сравните с kernel-only:
    // разница - время передачи данных (на unified memory она мала)
    std::cout << "GPU time (transfer + kernel): " << (long long)tPipelineNs << " ns" << std::endl;

    // --- Часть A: печать массива r (дано; печатается, если данных немного) ---
    if ((size_t)N * M <= 100) {
        for (int ch = 0; ch < N; ch++) {
            std::cout << "channel " << ch + 1 << ": ";
            for (int pos = 0; pos < M; pos++)
                std::printf("%g ", r[(size_t)ch * M + pos]);
            std::cout << std::endl;
        }
    }

    // --- Часть B: среднее по всем N*M отсчетам ---
    // TODO 3: просуммируйте все элементы массива r (их N*M),
    // поделите сумму на (double)N * M и напечатайте:
    //     std::printf("result: %.4f\n", avg);

    delete[] a; delete[] b0; delete[] b1; delete[] sel; delete[] r;
    clReleaseMemObject(bufA);  clReleaseMemObject(bufB0); clReleaseMemObject(bufB1);
    clReleaseMemObject(bufS);  clReleaseMemObject(bufR);
    clReleaseKernel(kernel);   clReleaseProgram(program);
    clReleaseCommandQueue(queue); clReleaseContext(context);
    return 0;
}
