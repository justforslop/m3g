#include <slop/convert.hpp>
#include <slop/decode.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char *USAGE =
    "Usage: slop <input.m3g> <output.{gltf|glb}> [--pattern <image.{png|jpg|jpeg}>] "
    "[--png-level <0-9>] [--overwrite] [--verbose]";

struct CommandLine {
    std::string input_path;
    std::string output_path;
    bool overwrite = false;
    bool verbose = false;
    std::optional<std::string> pattern_path;
    int png_compression_level = 8;
};

std::optional<CommandLine> parse_command(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::nullopt;
    }
    for (const auto &arg : args) {
        if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        }
    }

    CommandLine command;
    std::vector<std::string> positional;
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string &arg = args[index];
        if (arg == "--overwrite") {
            command.overwrite = true;
        } else if (arg == "--verbose") {
            command.verbose = true;
        } else if (arg == "--pattern") {
            if (index + 1 >= args.size() || args[index + 1].rfind("--", 0) == 0) {
                throw std::invalid_argument("Missing value for --pattern option.");
            }
            command.pattern_path = args[++index];
        } else if (arg == "--png-level" || arg == "--compress-level") {
            if (index + 1 >= args.size() || args[index + 1].rfind("--", 0) == 0) {
                throw std::invalid_argument("Missing value for --png-level option.");
            }
            const std::string &value = args[++index];
            try {
                command.png_compression_level = std::stoi(value);
            } catch (const std::exception &) {
                throw std::invalid_argument("Invalid --png-level value: " + value);
            }
            if (command.png_compression_level < 0 || command.png_compression_level > 9) {
                throw std::invalid_argument("--png-level must be between 0 and 9.");
            }
        } else if (arg.rfind("--", 0) == 0) {
            throw std::invalid_argument("Unknown option: " + arg);
        } else {
            positional.push_back(arg);
        }
    }
    if (positional.size() != 2) {
        throw std::invalid_argument("Expected exactly two positional arguments.");
    }
    command.input_path = positional[0];
    command.output_path = positional[1];
    return command;
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    std::optional<CommandLine> command;
    try {
        command = parse_command(args);
    } catch (const std::invalid_argument &error) {
        std::cerr << error.what() << '\n' << USAGE << '\n';
        return 1;
    }

    if (!command) {
        std::cout << USAGE << '\n';
        return 0;
    }

    try {
        slop::M3gConverter converter;
        auto report = converter.convert(command->input_path, command->output_path, command->overwrite,
                                        command->pattern_path, command->png_compression_level);
        const auto &decoded = report.decoded;
        if (command->verbose) {
            std::cout << "Converted " << decoded.source_path << " -> " << report.paths.gltf_path << '\n';
            std::cout << "Nodes=" << decoded.node_count() << ", meshes=" << decoded.mesh_count()
                      << ", materials=" << decoded.material_count() << ", textures=" << decoded.texture_count()
                      << ", images=" << decoded.image_count() << ", cameras=" << decoded.camera_count()
                      << ", animations=" << decoded.animation_count() << '\n';
            if (!decoded.warnings().empty()) {
                std::cout << "Warnings:\n";
                for (const auto &warning : decoded.warnings()) {
                    std::cout << " - " << warning.message << '\n';
                }
            }
        } else {
            std::cout << "Wrote " << report.paths.gltf_path << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << "slop failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
