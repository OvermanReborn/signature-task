#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Ищем все вхождения в файле, прочитанном целиком.
bool scanFile(const fs::path& path, const std::string& signature) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        std::cerr << "Не удалось открыть: " << path.string() << '\n';
        return false;
    }

    std::vector<char> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (file.bad()) {
        std::cerr << "Ошибка чтения: " << path.string() << '\n';
        return false;
    }

    std::vector<std::size_t> offsets;
    auto position = data.begin();

    while (position != data.end()) {
        position = std::search(
            position, data.end(),
            signature.begin(), signature.end()
        );

        if (position == data.end()) {
            break;
        }

        offsets.push_back(position - data.begin());
        ++position;
    }

    if (!offsets.empty()) {
        std::cout << path.string() << "\n  Смещения: ";
        for (auto offset : offsets) {
            std::cout << offset << " ";
        }
        std::cout << "\n  Вхождений: " << offsets.size() << "\n\n";
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " ПУТЬ_К_КАТАЛОГУ\n";
        return 1;
    }

    fs::path directory = argv[1];
    const std::string signature = "TMP{VIRUS}";
    std::error_code ec;

    if (!fs::is_directory(directory, ec)) {
        std::cerr << "Каталог не найден или недоступен\n";
        return 1;
    }

    fs::recursive_directory_iterator it(directory, ec);
    fs::recursive_directory_iterator end;

    if (ec) {
        std::cerr << "Ошибка открытия каталога: " << ec.message() << '\n';
        return 1;
    }

    while (it != end) {
        if (it->is_regular_file(ec)) {
            if (!scanFile(it->path(), signature)) {
                return 1;
            }
        }

        if (ec) {
            std::cerr << "Ошибка: " << ec.message() << '\n';
            return 1;
        }

        it.increment(ec);
        if (ec) {
            std::cerr << "Ошибка обхода: " << ec.message() << '\n';
            return 1;
        }
    }

    std::cout << "Поиск завершён.\n";
    return 0;
}
