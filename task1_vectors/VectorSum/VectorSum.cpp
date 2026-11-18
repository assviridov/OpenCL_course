// VectorSum.cpp — CPU-версия (эталон для проверки)
// Считает поэлементную сумму двух векторов и печатает время работы.
// Результат работы GPU-версии (VectorSumCL) должен совпасть с этим выводом.

#include <iostream>
#include <fstream>
#include <ctime>

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
    clock_t tStart = clock();

    sum(A, B, C, N);

    double tMs = (double)(clock() - tStart) * 1000.0 / CLOCKS_PER_SEC;

    for (int i = 0; i < 10 && i < N; i++)
        std::cout << C[i] << " ";
    std::cout << (N > 10 ? "..." : "") << std::endl;

    std::cout << "CPU time: " << tMs << " ms" << std::endl;

    delete[] A;
    delete[] B;
    delete[] C;
    return 0;
}
