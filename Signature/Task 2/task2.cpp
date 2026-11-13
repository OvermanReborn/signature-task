#include <filesystem>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " ПУТЬ_К_КАТАЛОГУ\n";
        return 1;
    }

    fs::path directory = argv[1];
    std::error_code ec;

    if (!fs::is_directory(directory, ec)) {
        std::cerr << "Ошибка: каталог не найден или недоступен\n";
        return 1;
    }

    std::uintmax_t totalSize = 0;
    std::size_t fileCount = 0;

    fs::recursive_directory_iterator it(directory, ec);
    fs::recursive_directory_iterator end;

    if (ec) {
        std::cerr << "Ошибка открытия каталога: " << ec.message() << '\n';
        return 1;
    }

    while (it != end) {
        const auto& entry = *it;

        if (entry.is_regular_file(ec) && !ec) {
            auto size = entry.file_size(ec);

            if (!ec) {
                std::cout << entry.path().string()
                          << " — " << size << " байт\n";
                ++fileCount;
                totalSize += size;
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

    std::cout << "\nВсего файлов: " << fileCount << '\n';
    std::cout << "Общий размер: " << totalSize << " байт\n";

    return 0;
}
