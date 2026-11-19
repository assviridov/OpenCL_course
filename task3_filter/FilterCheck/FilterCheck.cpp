// FilterCheck.cpp — ПОСЛЕДОВАТЕЛЬНАЯ ПРОВЕРОЧНАЯ ПРОГРАММА (эталон).
// Студентам: пользуйтесь для сверки ответа; читать код не обязательно.
//
// Считает отфильтрованные значения r "в лоб" (двойной цикл) в double
// и печатает среднее по всем N*M отсчетам с точностью до 4-го знака.
// Запуск: FilterCheck [input.txt] [-v]
//   -v — дополнительно напечатать массив r построчно (для отладки части A).

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <chrono>

int main(int argc, char* argv[])
{
    const char* filename = "input.txt";
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-v") verbose = true;
        else filename = argv[i];
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "file error " << filename << "!" << std::endl;
        return 1;
    }

    int N, M, K;
    file >> N >> M >> K;

    double* b[2] = { new double[K], new double[K] };
    for (int f = 0; f < 2; f++)
        for (int i = 0; i < K; i++)
            file >> b[f][i];

    int* sel = new int[N];
    for (int i = 0; i < N; i++) {
        file >> sel[i];
        sel[i]--;                       // 1 или 2 -> индекс 0 или 1
    }

    double* a = new double[(size_t)N * M];
    for (size_t i = 0; i < (size_t)N * M; i++)
        file >> a[i];
    file.close();

    const int half = (K - 1) / 2;       // отступ от края до центра окна

    auto tStart = std::chrono::steady_clock::now();

    double res = 0.0;
    for (int ch = 0; ch < N; ch++) {
        const double* bch = b[sel[ch]];
        const double* arow = a + (size_t)ch * M;
        if (verbose) std::cout << "channel " << ch + 1 << ": ";
        for (int pos = 0; pos < M; pos++) {
            double r = 0.0;
            if (pos >= half && pos < M - half) {        // окно целиком внутри канала
                const double* win = arow + pos - half;  // начало окна
                for (int k = 0; k < K; k++)
                    r += win[k] * bch[k];
            } else {
                r = 0.0;                                // краевые отсчеты
            }
            if (verbose) std::cout << r << " ";
            res += r;
        }
        if (verbose) std::cout << std::endl;
    }

    auto tEnd = std::chrono::steady_clock::now();
    double tNs = std::chrono::duration<double, std::nano>(tEnd - tStart).count();

    std::cout << std::fixed << std::setprecision(4)
              << "result: " << res / ((double)N * M) << std::endl;
    std::cout << "CPU time: " << (long long)tNs << " ns" << std::endl;

    delete[] b[0];
    delete[] b[1];
    delete[] sel;
    delete[] a;
    return 0;
}
