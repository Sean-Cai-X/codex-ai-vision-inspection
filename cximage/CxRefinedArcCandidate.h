#pragma once
#include "CxRefinedArcUiParameters.h"

// Paired candidates own two frozen images, not the viewer's currently selected
// single image. Discovery publishes evidence_binding.json only after this
// bundle has been written and checked by the caller.
namespace cxpaircandidate {
using J = nlohmann::json;
inline std::string Bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("PAIR_CANDIDATE_SOURCE_UNREADABLE");
  std::string bytes((std::istreambuf_iterator<char>(input)), {});
  if (input.bad() || bytes.empty())
    throw std::runtime_error("PAIR_CANDIDATE_SOURCE_EMPTY_OR_READ_FAILED");
  return bytes;
}
inline void Write(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output) throw std::runtime_error("PAIR_CANDIDATE_WRITE_FAILED");
}
inline J Freeze(const std::string& script, const std::filesystem::path& directory) {
  auto config = cxpairui::Read(script); // refuse token/executable disagreement
  std::string bytes[2], graySha[2];
  std::filesystem::path sources[2];
  for (int i : {0, 1}) {
    sources[i] = cxpairui::ImagePath(config.at("sides").at(i));
    bytes[i] = Bytes(sources[i]);
    graySha[i] = cxpairui::ImageSha(config.at("sides").at(i));
    if (Bytes(sources[i]) != bytes[i])
      throw std::runtime_error("PAIR_CANDIDATE_SOURCE_CHANGED");
    const auto declared = config.at("sides").at(i).at("landmark").value("image_sha256", std::string());
    if (!declared.empty() && declared != graySha[i])
      throw std::runtime_error("PAIR_CANDIDATE_LANDMARK_SHA_MISMATCH");
  }
  const auto dir = std::filesystem::absolute(directory).lexically_normal();
  std::filesystem::create_directories(dir.parent_path());
  if (!std::filesystem::create_directory(dir))
    throw std::runtime_error("PAIR_CANDIDATE_ALREADY_EXISTS");
  J inputs = J::array();
  for (int i : {0, 1}) {
    const std::string role = i == 0 ? "reference" : "observation";
    const auto image = dir / (role + sources[i].extension().string());
    Write(image, bytes[i]);
    config["sides"][i]["image"] = image.generic_string();
    if (Bytes(image) != bytes[i] || cxpairui::ImageSha(config["sides"][i]) != graySha[i])
      throw std::runtime_error("PAIR_CANDIDATE_COPY_MISMATCH");
    inputs.push_back({{"role", role}, {"source_image_path", sources[i].generic_string()},
      {"image_path", image.generic_string()}, {"case_id", config["sides"][i]["case_id"]},
      {"dataset_revision", config["sides"][i]["revision"]},
      {"file_sha256", cxgeom::so2::Sha256Bytes(bytes[i])}, {"gray_sha256", graySha[i]}});
  }
  const auto frozen = cxpairui::Build(config);
  if (cxpairui::Read(frozen) != config)
    throw std::runtime_error("PAIR_CANDIDATE_ROUNDTRIP_FAILED");
  Write(dir / "script_snapshot.cxsc", frozen);
  Write(dir / "source_script.cxsc", script);
  Write(dir / "pair_parameters.json", config.dump(2));
  J binding{{"schema", "refined_arc_candidate_inputs.v1"}, {"inputs", inputs},
    {"source_script_sha256", cxgeom::so2::Sha256Bytes(script)},
    {"script_sha256", cxgeom::so2::Sha256Bytes(frozen)},
    {"production_allowed", false}, {"result_status", "NOT_RUN"},
    {"review_status", "pending_human_review"}};
  Write(dir / "pair_input_binding.json", binding.dump(2));
  return binding;
}
inline void Verify(const std::filesystem::path& dir) {
  const auto script = Bytes(dir / "script_snapshot.cxsc");
  const auto config = cxpairui::Read(script);
  const auto binding = J::parse(Bytes(dir / "pair_input_binding.json"));
  if (binding.at("schema") != "refined_arc_candidate_inputs.v1" ||
      binding.at("script_sha256") != cxgeom::so2::Sha256Bytes(script) ||
      J::parse(Bytes(dir / "pair_parameters.json")) != config ||
      binding.at("inputs").size() != 2 || binding.at("production_allowed") != false)
    throw std::runtime_error("PAIR_CANDIDATE_BINDING_MISMATCH");
  for (int i : {0, 1}) {
    const auto& input = binding.at("inputs").at(i);
    const auto& side = config.at("sides").at(i);
    if (input.at("role") != (i == 0 ? "reference" : "observation") ||
        input.at("image_path") != side.at("image") ||
        input.at("case_id") != side.at("case_id") ||
        input.at("dataset_revision") != side.at("revision") ||
        input.at("file_sha256") != cxgeom::so2::Sha256Bytes(Bytes(cxpairui::ImagePath(side))) ||
        input.at("gray_sha256") != cxpairui::ImageSha(side))
      throw std::runtime_error("PAIR_CANDIDATE_INPUT_MISMATCH");
  }
}
}
