#include "project_processor.hxx"
#include "path_relativizer.hxx"
#include <pugixml.hpp>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace {
    bool strip_cmake_nodes(pugi::xml_node project_node) {
        std::vector<pugi::xml_node> to_remove;

        for (pugi::xml_node group: project_node.children("ItemGroup")) {
            for (pugi::xml_node node: group.children()) {
                std::string_view name = node.name();
                std::string_view inc = node.attribute("Include").value();

                if ((name == "CustomBuild" && PathRelativizer::contains_ci(inc, "CMakeLists.txt")) ||
                    (name == "ProjectReference" && (PathRelativizer::contains_ci(inc, "ZERO_CHECK") ||
                                                    PathRelativizer::contains_ci(inc, "ALL_BUILD") ||
                                                    PathRelativizer::contains_ci(inc, "INSTALL.")))) {
                    to_remove.push_back(node);
                }
            }
        }

        for (pugi::xml_node node: to_remove) {
            pugi::xml_node parent = node.parent();
            parent.remove_child(node);
            if (!parent.first_child()) {
                parent.parent().remove_child(parent);
            }
        }
        return !to_remove.empty();
    }

    constexpr auto is_cmake_meta = [](std::string_view p) noexcept {
        return PathRelativizer::contains_ci(p, "ALL_BUILD") ||
               PathRelativizer::contains_ci(p, "ZERO_CHECK") ||
               PathRelativizer::contains_ci(p, "INSTALL.") ||
               PathRelativizer::contains_ci(p, "PACKAGE.");
    };

    bool process_slnx_solution(pugi::xml_document &doc, const fs::path &sln_dir) {
        pugi::xml_node solution = doc.child("Solution");
        if (!solution) return false;

        std::vector<pugi::xml_node> meta_nodes;
        pugi::xml_node candidate_startup;
        bool need_new_startup = false;

        for (pugi::xml_node proj: solution.children("Project")) {
            std::string_view path = proj.attribute("Path").value();
            if (is_cmake_meta(path)) {
                meta_nodes.push_back(proj);
                if (proj.attribute("DefaultStartup").as_bool()) {
                    need_new_startup = true;
                }
            } else if (!candidate_startup) {
                candidate_startup = proj;
            }
        }

        if (meta_nodes.empty()) return false;

        if (need_new_startup && candidate_startup) {
            candidate_startup.remove_attribute("DefaultStartup");
            candidate_startup.append_attribute("DefaultStartup").set_value("true");
        }

        for (pugi::xml_node proj: solution.children("Project")) {
            std::vector<pugi::xml_node> dead_deps;
            for (pugi::xml_node dep: proj.children("BuildDependency")) {
                if (is_cmake_meta(dep.attribute("Project").value())) {
                    dead_deps.push_back(dep);
                }
            }
            for (auto dep: dead_deps) proj.remove_child(dep);
        }

        for (pugi::xml_node node: meta_nodes) {
            std::string path_val = node.attribute("Path").value();
            solution.remove_child(node);
            std::error_code ec;
            fs::remove(sln_dir / path_val, ec);
            fs::remove(sln_dir / (path_val + ".filters"), ec);

            std::cout << "[Cleaned] " << path_val << " removed from .slnx and disk\n";
        }
        return true;
    }
} // namespace

void process_xml_file(const fs::path &file_path, std::span<const fs::path> roots) {
    pugi::xml_document doc;
    constexpr unsigned parse_flags = pugi::parse_default | pugi::parse_comments | pugi::parse_declaration;

    if (auto res = doc.load_file(file_path.c_str(), parse_flags); !res) {
        std::cerr << "[XML Error] " << file_path << ": " << res.description() << '\n';
        return;
    }

    bool modified = false;

    if (file_path.extension() == ".slnx") {
        modified |= process_slnx_solution(doc, file_path.parent_path());
    } else {
        modified |= strip_cmake_nodes(doc.child("Project"));
    }

    PathRelativizer relativizer(file_path.parent_path(), roots);

    const bool is_vcxproj = (file_path.extension() == ".vcxproj");
    const auto text_mode = is_vcxproj
                               ? PathRelativizer::Mode::MsBuildMacro
                               : PathRelativizer::Mode::Relative;

    struct RelativizeWalker : pugi::xml_tree_walker {
        const PathRelativizer &rel;
        PathRelativizer::Mode text_mode;
        bool &mod;

        RelativizeWalker(const PathRelativizer &r, PathRelativizer::Mode tm, bool &m)
            : rel(r), text_mode(tm), mod(m) {
        }

        bool for_each(pugi::xml_node &node) override {
            if (node.type() == pugi::node_element) {
                for (pugi::xml_attribute attr: node.attributes()) {
                    if (auto updated = rel.process(attr.value(), PathRelativizer::Mode::Relative)) {
                        attr.set_value(updated->c_str());
                        mod = true;
                    }
                }
            } else if (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata) {
                if (auto updated = rel.process(node.value(), text_mode)) {
                    node.set_value(updated->c_str());
                    mod = true;
                }
            }
            return true;
        }
    } walker(relativizer, text_mode, modified);

    doc.traverse(walker);

    if (modified) {
        constexpr unsigned save_flags = pugi::format_default | pugi::format_write_bom;
        if (!doc.save_file(file_path.c_str(), "  ", save_flags, pugi::encoding_utf8)) {
            std::cerr << "[Save Error] Failed to write: " << file_path << '\n';
        } else {
            std::cout << "[XML Updated] " << file_path.filename().string() << '\n';
        }
    }
}

void process_sln_file(const fs::path &file_path, std::span<const fs::path> roots) {
    std::ifstream in(file_path, std::ios::binary | std::ios::ate);
    if (!in) return;

    std::string content(static_cast<std::size_t>(in.tellg()), '\0');
    in.seekg(0);
    in.read(content.data(), static_cast<std::streamsize>(content.size()));
    in.close();

    bool modified = false;

    std::regex meta_regex(R"rx(Project\([^\)]+\)\s*=\s*"(?:ALL_BUILD|ZERO_CHECK|INSTALL)"[\s\S]*?EndProject\r?\n)rx");
    if (std::regex_search(content, meta_regex)) {
        content = std::regex_replace(content, meta_regex, "");
        modified = true;
        std::error_code ec;
        for (const char *name: {"ALL_BUILD", "ZERO_CHECK", "INSTALL"}) {
            fs::remove(file_path.parent_path() / (std::string(name) + ".vcxproj"), ec);
            fs::remove(file_path.parent_path() / (std::string(name) + ".vcxproj.filters"), ec);
        }
        std::cout << "[Cleaned] CMake meta-projects removed from .sln and disk\n";
    }

    PathRelativizer relativizer(file_path.parent_path(), roots);
    if (auto updated = relativizer.process(content, PathRelativizer::Mode::Relative)) {
        content = std::move(*updated);
        modified = true;
    }

    if (modified) {
        std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        std::cout << "[SLN Updated] " << file_path.filename().string() << '\n';
    }
}
