#ifndef VCXRELATIVE_PATH_RELATIVIZER_HXX
#define VCXRELATIVE_PATH_RELATIVIZER_HXX

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

class PathRelativizer {
public:
    enum class Mode {
        Relative,
        MsBuildMacro
    };

    PathRelativizer(const fs::path& file_dir, std::span<const fs::path> roots);

    [[nodiscard]] std::optional<std::string> process(std::string_view input, Mode mode) const;

    [[nodiscard]] static bool contains_ci(std::string_view haystack, std::string_view needle) noexcept;

private:
    struct Rule {
        std::string target;
        std::string rel_replacement;
        std::string macro_replacement;
        bool is_current_dir{false};
    };

    std::vector<Rule> rules_;
    std::size_t min_target_len_ = ~std::size_t{0};
};

#endif //VCXRELATIVE_PATH_RELATIVIZER_HXX
