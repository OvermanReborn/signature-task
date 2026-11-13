#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <miniz.h>

namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
using Offsets = std::vector<std::uint64_t>;

struct Scanner {
    std::string sig = "TMP{VIRUS}";
    int maxDepth = 3;
    std::uint64_t checked = 0, infected = 0, matches = 0, errors = 0;
    std::ofstream report;
    bool reporting = false;

    void error(const std::string& text) {
        ++errors;
        std::cerr << "Ошибка: " << text << '\n';
    }
    static bool zipHeader(const unsigned char* b, std::size_t n) {
        return n >= 4 && b[0] == 'P' && b[1] == 'K' &&
            ((b[2] == 3 && b[3] == 4) ||
             (b[2] == 5 && b[3] == 6) ||
             (b[2] == 7 && b[3] == 8));
    }
    static std::string csvEscape(const std::string& text) {
        std::string s = "\"";
        for (char c : text) { if (c == '"') s += '"'; s += c; }
        return s + '"';
    }
    void found(const std::string& path, const Offsets& offsets) {
        if (offsets.empty()) return;
        ++infected;
        matches += offsets.size();
        std::ostringstream values;
        for (std::size_t i = 0; i < offsets.size(); ++i) {
            if (i) values << ' ';
            values << offsets[i];
        }
        std::cout << path << " | смещения: " << values.str() << '\n';
        if (reporting) report << csvEscape(path) << ';' << csvEscape(values.str()) << '\n';
    }
    Offsets searchBuffer(const Bytes& data) const {
        Offsets offsets;
        auto it = data.begin();
        while (it != data.end()) {
            it = std::search(it, data.end(), sig.begin(), sig.end());
            if (it == data.end()) break;
            offsets.push_back(static_cast<std::uint64_t>(it - data.begin()));
            ++it; // Учитываем перекрывающиеся вхождения.
        }
        return offsets;
    }
    // Сохраняем хвост длиной sig.size()-1 между блоками.
    void scanStream(std::ifstream& in, const std::string& path) {
        constexpr std::size_t blockSize = 65536;
        std::vector<char> block(blockSize);
        std::string buffer;
        std::uint64_t total = 0;
        Offsets offsets;
        while (in) {
            in.read(block.data(), block.size());
            auto n = static_cast<std::size_t>(in.gcount());
            if (!n) break;
            std::size_t old = buffer.size();
            buffer.append(block.data(), n);
            std::size_t pos = 0;
            while ((pos = buffer.find(sig, pos)) != std::string::npos) {
                offsets.push_back(total - old + pos);
                ++pos;
            }
            total += n;
            std::size_t keep = std::min(sig.size() - 1, buffer.size());
            buffer.erase(0, buffer.size() - keep);
        }
        if (in.bad() || (!in.eof() && in.fail())) {
            error("чтение файла " + path);
            return;
        }
        ++checked;
        found(path, offsets);
    }
    void scanZip(const Bytes& bytes, const std::string& path, int depth) {
        mz_zip_archive zip{};
        if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
            error("не удалось открыть ZIP " + path);
            return;
        }
        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; i < count; ++i) {
            mz_zip_archive_file_stat info{};
            if (!mz_zip_reader_file_stat(&zip, i, &info)) {
                error("не удалось получить запись ZIP " + path);
                continue;
            }
            if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
            std::string name = path + "!/" + info.m_filename;
            size_t n = 0;
            void* raw = mz_zip_reader_extract_to_heap(&zip, i, &n, 0);
            if (!raw) {
                error("не удалось распаковать " + name);
                continue;
            }
            const auto* ptr = static_cast<const unsigned char*>(raw);
            Bytes content(ptr, ptr + n);
            mz_free(raw);
            if (zipHeader(content.data(), content.size())) {
                if (depth < maxDepth) scanZip(content, name, depth + 1);
                else std::cerr << "Пропущен ZIP (предел глубины): " << name << '\n';
            } else {
                ++checked;
                found(name, searchBuffer(content));
            }
        }
        mz_zip_reader_end(&zip);
    }
    void scanFile(const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) { error("не удалось открыть " + path.string()); return; }
        unsigned char header[4]{};
        in.read(reinterpret_cast<char*>(header), 4);
        auto n = static_cast<std::size_t>(in.gcount());
        if (in.bad()) { error("чтение файла " + path.string()); return; }
        in.clear();
        in.seekg(0);
        if (!in) { error("перемещение в файле " + path.string()); return; }
        if (zipHeader(header, n)) {
            Bytes bytes((std::istreambuf_iterator<char>(in)), {});
            if (in.bad()) { error("чтение ZIP " + path.string()); return; }
            if (maxDepth == 0) {
                std::cerr << "Пропущен ZIP (предел глубины): " << path.string() << '\n';
            } else {
                scanZip(bytes, path.string(), 1);
            }
        } else scanStream(in, path.string());
    }
    void scanPath(const fs::path& path) {
        std::error_code ec;
        if (fs::is_regular_file(path, ec)) { scanFile(path); return; }
        if (ec) { error(path.string() + ": " + ec.message()); return; }
        if (!fs::is_directory(path, ec)) {
            error("путь не существует или не является файлом/каталогом: " + path.string());
            return;
        }
        fs::recursive_directory_iterator it(path, ec), end;
        if (ec) { error("открытие каталога " + path.string() + ": " + ec.message()); return; }
        while (it != end) {
            std::error_code fileError;
            if (it->is_regular_file(fileError)) scanFile(it->path());
            if (fileError) error(it->path().string() + ": " + fileError.message());
            it.increment(ec);
            if (ec) { error("обход каталога: " + ec.message()); break; }
        }
    }
};

void help() {
    std::cout << "Использование: sigscan <путь> [--sig СТРОКА] [--depth N] [--report файл.csv]\n"
                 "По умолчанию: --sig 'TMP{VIRUS}' --depth 3\n"
                 "Коды выхода: 0 — не найдено, 1 — найдено, 2 — ошибка.\n";
}
int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        help(); return 0;
    }
    if (argc < 2 || argv[1][0] == '-') { help(); return 2; }
    Scanner scanner;
    std::string reportPath;
    for (int i = 2; i < argc; ++i) {
        std::string option = argv[i];
        if ((option == "--sig" || option == "--depth" || option == "--report") && i + 1 < argc) {
            std::string value = argv[++i];
            if (option == "--sig") {
                if (value.empty()) { std::cerr << "Пустая сигнатура недопустима\n"; return 2; }
                scanner.sig = value;
            } else if (option == "--report") {
                if (value.empty()) { std::cerr << "Пустое имя отчёта\n"; return 2; }
                reportPath = value;
            } else {
                try {
                    std::size_t used = 0;
                    int depth = std::stoi(value, &used);
                    if (used != value.size() || depth < 0 || depth > 100) throw std::invalid_argument("depth");
                    scanner.maxDepth = depth;
                } catch (...) { std::cerr << "Неверная глубина: " << value << '\n'; return 2; }
            }
        } else { std::cerr << "Неизвестный параметр или нет значения: " << option << '\n'; help(); return 2; }
    }
    // Отчёт будет создан заранее только если он не находится внутри сканируемого пути.
    if (!reportPath.empty()) {
        std::error_code ec;
        fs::path input = fs::weakly_canonical(argv[1], ec);
        if (ec) { std::cerr << "Ошибка пути\n"; return 2; }
        fs::path output = fs::weakly_canonical(reportPath, ec);
        if (ec) { std::cerr << "Ошибка пути отчёта\n"; return 2; }
        auto rel = output.lexically_relative(input);
        if (output == input || (!rel.empty() && *rel.begin() != "..")) {
            std::cerr << "Отчёт должен быть вне сканируемого файла или каталога\n"; return 2;
        }
        scanner.report.open(reportPath);
        if (!scanner.report) { std::cerr << "Не удалось создать отчёт\n"; return 2; }
        scanner.reporting = true;
        scanner.report << "путь;смещения\n";
    }
    auto start = std::chrono::steady_clock::now();
    try { scanner.scanPath(argv[1]); }
    catch (const std::exception& e) { scanner.error(e.what()); }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (scanner.reporting) {
        scanner.report.close();
        if (!scanner.report) scanner.error("запись CSV-отчёта");
    }
    std::cout << "\nПроверено файлов: " << scanner.checked
              << "\nФайлов с сигнатурой: " << scanner.infected
              << "\nВсего вхождений: " << scanner.matches
              << "\nОшибок: " << scanner.errors
              << "\nВремя: " << elapsed << " мс\n";
    return scanner.errors ? 2 : (scanner.infected ? 1 : 0);
}
