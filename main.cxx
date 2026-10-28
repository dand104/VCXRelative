#include "path_relativizer.hxx"
#include "project_processor.hxx"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: vcxrelative <build_dir> [source_dir...]\n";
        return 1;
    }

    try {
        std::vector<fs::path> roots;
        for (int i = 1; i < argc; ++i) {
            roots.push_back(fs::weakly_canonical(argv[i]));
        }

        const fs::path build_dir = roots.front();

        std::ranges::sort(roots, std::ranges::greater{}, [](const fs::path& p) {
            return p.native().size();
        });

        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(build_dir)) {
            if (!entry.is_regular_file()) continue;
            if (!fs::exists(entry.path())) continue;

            const fs::path& path = entry.path();
            const auto ext = path.extension().string();

            if (ext == ".vcxproj" || ext == ".slnx" ||
                (ext == ".filters" && PathRelativizer::contains_ci(path.filename().string(), ".vcxproj.filters"))) {
                process_xml_file(path, roots);
                } else if (ext == ".sln") {
                    process_sln_file(path, roots);
                }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal Error] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}