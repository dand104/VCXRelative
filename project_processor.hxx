#ifndef VCXRELATIVE_PROJECT_PROCESSOR_HXX
#define VCXRELATIVE_PROJECT_PROCESSOR_HXX

#include <filesystem>
#include <span>

namespace fs = std::filesystem;

void process_xml_file(const fs::path& file_path, std::span<const fs::path> roots);
void process_sln_file(const fs::path& file_path, std::span<const fs::path> roots);

#endif //VCXRELATIVE_PROJECT_PROCESSOR_HXX
