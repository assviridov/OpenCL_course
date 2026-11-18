// VectorGen.cpp — генератор входных данных
// Использование: VectorGen [N]   (по умолчанию N = 25)
// Формат vector.txt:
//   строка 1: N
//   далее N строк: A[i] B[i]   (целые числа от -100 до 100)

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

int main(int argc, char* argv[])
{
    int N = 250000;
    if (argc > 1)
        N = std::atoi(argv[1]);
    if (N < 1) {
        std::cout << "bad N" << std::endl;
        return 1;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const char* filename = "vector.txt";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "file error " << filename << "!" << std::endl;
        return 1;
    }

    file << N << "\n";
    for (int i = 0; i < N; i++)
        file << (std::rand() % 201 - 100) << " " << (std::rand() % 201 - 100) << "\n";

    file.close();
    std::cout << "Done: " << N << " pairs -> " << filename << std::endl;
    return 0;
}
