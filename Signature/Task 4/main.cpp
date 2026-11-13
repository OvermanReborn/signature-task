#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <miniz.h>

using Bytes = std::vector<unsigned char>;

const std::string signature = "TMP{VIRUS}";

// Проверяем первые байты: похож ли файл на ZIP.
bool isZip(const Bytes& data) {
    return data.size() >= 4 &&
           data[0] == 'P' && data[1] == 'K' &&
           ((data[2] == 3 && data[3] == 4) ||
            (data[2] == 5 && data[3] == 6) ||
            (data[2] == 7 && data[3] == 8));
}

// Открываем ZIP из памяти и проверяем его содержимое.
void scanZip(const Bytes& data, const std::string& path, int depth) {
    mz_zip_archive zip{};

    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        std::cerr << "Ошибка открытия ZIP: " << path << '\n';
        return;
    }

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
        mz_zip_archive_file_stat info{};

        if (!mz_zip_reader_file_stat(&zip, i, &info)) {
            std::cerr << "Ошибка чтения записи " << i << '\n';
            continue;
        }

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            continue;
        }

        std::string name = path + "!/" + info.m_filename;

        std::cout << "Файл: " << name
                  << " | исходный: " << info.m_uncomp_size
                  << " | сжатый: " << info.m_comp_size
                  << " | метод: " << info.m_method << '\n';

        if (info.m_uncomp_size == 0) {
            continue;
        }

        size_t size = 0;
        void* raw = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);

        if (!raw) {
            std::cerr << "Ошибка распаковки: " << name << '\n';
            continue;
        }

        auto bytes = static_cast<unsigned char*>(raw);
        Bytes content(bytes, bytes + size);
        mz_free(raw);

        if (isZip(content)) {
            if (depth < 3) {
                scanZip(content, name, depth + 1);
            } else {
                std::cout << "Достигнут предел вложенности: "
                          << name << '\n';
            }
        } else {
            auto position = content.begin();

            while (true) {
                position = std::search(
                    position, content.end(),
                    signature.begin(), signature.end()
                );

                if (position == content.end()) {
                    break;
                }

                std::cout << "  СИГНАТУРА: смещение "
                          << (position - content.begin()) << '\n';

                ++position;
            }
        }
    }

    mz_zip_reader_end(&zip);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0]
                  << " ПУТЬ_К_ZIP\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);

    if (!file) {
        std::cerr << "Не удалось открыть файл\n";
        return 1;
    }

    Bytes data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (file.bad() || !isZip(data)) {
        std::cerr << "Ошибка чтения или это не ZIP\n";
        return 1;
    }

    scanZip(data, argv[1], 0);
    return 0;
}
