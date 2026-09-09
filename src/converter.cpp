#include "slop/converter.hpp"

#include "slop/decoder.hpp"
#include "slop/gltf_exporter.hpp"

namespace slop {

decode::Decoded M3gConverter::decode(const std::string &input_path,
                                     const std::optional<std::string> &pattern_path) const {
    decode::DecodeOptions options;
    options.pattern_path = pattern_path;
    return decode::Decoder{}.decode_file(input_path, options);
}

exp::GltfPaths M3gConverter::export_gltf(const decode::Decoded &decoded, const std::string &output_path,
                                         bool overwrite) const {
    return exp::GltfExporter{}.write(decoded, output_path, overwrite);
}

exp::ExportReport M3gConverter::convert(const std::string &input_path, const std::string &output_path, bool overwrite,
                                        const std::optional<std::string> &pattern_path) const {
    exp::ExportReport report;
    report.decoded = decode(input_path, pattern_path);
    report.paths = export_gltf(report.decoded, output_path, overwrite);
    return report;
}

} // namespace slop
