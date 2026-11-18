// MatrixSum.cpp — CPU-версия (эталон для проверки). Задача 2: разбор в классе.
// Складывает две прямоугольные матрицы M x N и печатает время работы.
// Результат GPU-версии (MatrixSumCL) должен совпасть с этим выводом.
//
// Новое относительно задачи 1 (сложение векторов):
//   - двумерные данные: элемент (row, col) хранится в линейном массиве
//     по формуле i = row * N + col  (row-major, строка за строкой);
//   - матрица ПРЯМОУГОЛЬНАЯ (M != N), чтобы индексацию нельзя было спутать
//     с линейной.

#include <iostream>
#include <cstdlib>
#include <ctime>

const int M = 8;   // строк
const int N = 5;   // столбцов

void sum(const int* A, const int* B, int* C, int m, int n)
{
    for (int i = 0; i < m; i++)          // по строкам
        for (int j = 0; j < n; j++)      // по столбцам
            C[i * n + j] = A[i * n + j] + B[i * n + j];
}

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
    int* A = new int[M * N];
    int* B = new int[M * N];
    int* C = new int[M * N];

    // фиксированный seed: CPU- и GPU-версии считают ОДНИ И ТЕ ЖЕ данные
    std::srand(42);
    for (int i = 0; i < M * N; i++) {
        A[i] = std::rand() % 201 - 100;
        B[i] = std::rand() % 201 - 100;
    }

    // --- Измерение времени ---
    clock_t tStart = clock();

    sum(A, B, C, M, N);

    double tMs = (double)(clock() - tStart) * 1000.0 / CLOCKS_PER_SEC;

    print(A, M, N, "A");
    print(B, M, N, "B");
    print(C, M, N, "C = A+B");
    std::cout << "CPU time: " << tMs << " ms" << std::endl;

    delete[] A;
    delete[] B;
    delete[] C;
    return 0;
}
