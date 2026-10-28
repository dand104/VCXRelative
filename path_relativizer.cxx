#include "path_relativizer.hxx"
#include <algorithm>
#include <cctype>
#include <ranges>

namespace {

std::string to_utf8(const fs::path& p) {
    const auto u8 = p.u8string();
    std::string str(reinterpret_cast<const char*>(u8.data()), u8.size());
    while (str.size() > 1 && (str.back() == '\\' || str.back() == '/')) {
        str.pop_back();
    }
    return str;
}

constexpr bool is_slash(char c) noexcept {
    return c == '\\' || c == '/';
}

constexpr bool path_char_eq(char a, char b) noexcept {
    if (is_slash(a) && is_slash(b)) return true;
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

constexpr bool is_path_boundary(char c) noexcept {
    return is_slash(c) || c == ';' || c == '"' || c == '\'';
}

} // namespace

PathRelativizer::PathRelativizer(const fs::path& file_dir, std::span<const fs::path> roots) {
    rules_.reserve(roots.size());
    for (const auto& root : roots) {
        std::string target = to_utf8(root);
        if (target.empty()) continue;

        fs::path rel = fs::proximate(root, file_dir).make_preferred();
        if (!rel.is_relative()) {
            continue;
        }
        std::string rel_str = to_utf8(rel);
        bool is_dot = (rel_str == ".");
        std::string macro_str = is_dot ? "$(ProjectDir)" : "$(ProjectDir)" + rel_str;

        min_target_len_ = std::min(min_target_len_, target.size());
        rules_.emplace_back(std::move(target), std::move(rel_str), std::move(macro_str), is_dot);
    }
}

bool PathRelativizer::contains_ci(std::string_view haystack, std::string_view needle) noexcept {
    return !std::ranges::search(haystack, needle, path_char_eq).empty();
}

std::optional<std::string> PathRelativizer::process(std::string_view input, Mode mode) const {
    if (rules_.empty() || input.size() < min_target_len_) {
        return std::nullopt;
    }

    std::optional<std::string> result;

    for (const auto& rule : rules_) {
        std::string_view view = result ? std::string_view(*result) : input;
        if (view.size() < rule.target.size()) continue;

        const std::string& repl = (mode == Mode::MsBuildMacro) ? rule.macro_replacement : rule.rel_replacement;
        std::string buffer;
        std::size_t pos = 0;
        bool replaced = false;

        while (pos < view.size()) {
            auto sub = view.substr(pos);
            auto match = std::ranges::search(sub, rule.target, path_char_eq);
            if (match.empty()) break;

            const std::size_t match_pos = pos + std::distance(sub.begin(), match.begin());
            std::size_t after_pos = match_pos + rule.target.size();

            if (after_pos == view.size() || is_path_boundary(view[after_pos])) {
                buffer.append(view.substr(pos, match_pos - pos));
                buffer.append(repl);
                replaced = true;

                if (mode == Mode::MsBuildMacro && rule.is_current_dir &&
                    after_pos < view.size() && is_slash(view[after_pos])) {
                    ++after_pos;
                }
                pos = after_pos;
            } else {
                buffer.append(view.substr(pos, (match_pos - pos) + 1));
                pos = match_pos + 1;
            }
        }

        if (replaced) {
            buffer.append(view.substr(pos));
            result = std::move(buffer);
        }
    }

    return result;
}