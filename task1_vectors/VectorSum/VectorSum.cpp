// VectorSum.cpp — CPU-версия (эталон для проверки)
// Считает поэлементную сумму двух векторов и печатает время работы.
// Результат работы GPU-версии (VectorSumCL) должен совпасть с этим выводом.

#include <iostream>
#include <fstream>
#include <chrono>   // steady_clock: монотонные часы с наносекундным разрешением

void sum(const int* A, const int* B, int* C, int n)
{
    for (int i = 0; i < n; i++)
        C[i] = A[i] + B[i];
}

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

    // --- Измерение времени (без учета чтения файла) ---
    // clock() на Windows имеет грубую гранулярность; steady_clock меряет наносекунды.
    auto tStart = std::chrono::steady_clock::now();

    sum(A, B, C, N);

    double tNs = std::chrono::duration<double, std::nano>(
                     std::chrono::steady_clock::now() - tStart).count();

    for (int i = 0; i < 10 && i < N; i++)
        std::cout << C[i] << " ";
    std::cout << (N > 10 ? "..." : "") << std::endl;

    std::cout << "CPU time: " << tNs << " ns" << std::endl;

    delete[] A;
    delete[] B;
    delete[] C;
    return 0;
}
