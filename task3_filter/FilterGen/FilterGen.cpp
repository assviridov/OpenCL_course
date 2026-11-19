// FilterGen.cpp — генератор входных данных для задачи о фильтрации сигнала.
// Использование: FilterGen [N] [M] [K] [seed]
//   N — число каналов   (по умолчанию 8)
//   M — отсчетов в канале (по умолчанию 1000)
//   K — размер окна       (по умолчанию 101; если ввели четное — берется K+1)
// Формат input.txt (как в оригинальной задаче):
//   строка 1: N M K
//   строка 2: K вещественных чисел — коэффициенты первого фильтра {b1}
//   строка 3: K вещественных чисел — коэффициенты второго фильтра {b2}
//   строка 4: N целых чисел 1 или 2 — номер фильтра для каждого канала
//   далее N строк по M вещественных чисел — отсчеты сигнала

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

int main(int argc, char* argv[])
{
    int N = 8, M = 1000, K = 101;
    unsigned seed = (unsigned)std::time(nullptr);
    if (argc > 1) N = std::atoi(argv[1]);
    if (argc > 2) M = std::atoi(argv[2]);
    if (argc > 3) K = std::atoi(argv[3]);
    if (argc > 4) seed = (unsigned)std::atoi(argv[4]);
    if (K % 2 == 0) K++;
    if (N < 1 || M < 1 || K < 1 || K > M) {
        std::cout << "bad parameters (need 1 <= K <= M)" << std::endl;
        return 1;
    }

    std::srand(seed);

    const char* filename = "input.txt";
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "file error " << filename << "!" << std::endl;
        return 1;
    }

    file << N << " " << M << " " << K << "\n";

    // два набора коэффициентов, значения в диапазоне [-1, 1]
    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < K; i++)
            file << (2.0 * std::rand() / RAND_MAX - 1.0) << " ";
        file << "\n";
    }

    // выбор фильтра для каждого канала: 1 или 2
    for (int i = 0; i < N; i++)
        file << (std::rand() % 2 + 1) << " ";
    file << "\n";

    // отсчеты сигнала, значения в диапазоне [-100, 100]
    for (int ch = 0; ch < N; ch++) {
        for (int i = 0; i < M; i++)
            file << (2.0 * std::rand() / RAND_MAX - 1.0) * 100.0 << " ";
        file << "\n";
    }

    file.close();
    std::cout << "Done: N=" << N << " M=" << M << " K=" << K << " -> " << filename << std::endl;
    return 0;
}
