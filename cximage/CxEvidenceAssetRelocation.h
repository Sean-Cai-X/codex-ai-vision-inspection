#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <set>
#include "../libtorchsegmentation/src/utils/json.hpp"

namespace cximage { namespace evidence {
// Explicit migration metadata, not a filename search or execution approval.
struct RelocatedAsset {
  std::string original_path, case_id, sha256, mapping_id;
  std::filesystem::path resolved_path;
};

inline std::string ReadRelocationBytes(const std::filesystem::path& path) {
  constexpr std::size_t limit=64*1024*1024;
  std::ifstream input(path,std::ios::binary);
  if(!input)throw std::runtime_error("RELOCATION_READ_FAILED");
  std::string bytes;char buffer[8192];
  while(input) {
    input.read(buffer,sizeof buffer);
    const auto count=static_cast<std::size_t>(input.gcount());
    if(count>limit-bytes.size())throw std::runtime_error("RELOCATION_SIZE_LIMIT");
    bytes.append(buffer,count);
  }
  if(input.bad())throw std::runtime_error("RELOCATION_READ_FAILED");
  return bytes;
}

inline RelocatedAsset ResolveRelocatedAsset(
    const nlohmann::json& manifest, const std::filesystem::path& asset_root,
    const std::string& original, const std::string& case_id,
    const std::function<std::string(const std::string&)>& digest) {
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  require(!original.empty()&&!case_id.empty(),"RELOCATION_IDENTITY_MISSING");
  require(manifest.at("schema")=="cxvision.evidence_asset_relocations.v1",
          "RELOCATION_SCHEMA");
  const auto& entries=manifest.at("entries");
  require(entries.is_array()&&entries.size()<=4096,"RELOCATION_ENTRY_LIMIT");
  const nlohmann::json* match=nullptr;
  for(const auto& entry:entries) {
    if(entry.at("original_path").get<std::string>()!=original ||
       entry.at("case_id").get<std::string>()!=case_id)continue;
    require(match==nullptr,"RELOCATION_AMBIGUOUS");
    match=&entry;
  }
  require(match!=nullptr,"RELOCATION_NOT_REGISTERED");
  const std::string relative=match->at("relative_path").get<std::string>();
  const std::filesystem::path rel(relative);
  require(!relative.empty()&&!rel.is_absolute()&&!rel.has_root_name()&&
          relative.find(':')==std::string::npos&&
          relative.find('\\')==std::string::npos,"RELOCATION_UNSAFE_PATH");
  for(const auto& part:rel)require(part!=".."&&part!=".","RELOCATION_UNSAFE_PATH");
  const auto root=std::filesystem::canonical(asset_root);
  auto candidate=root;
  for(const auto& part:rel) {
    candidate/=part;
    require(!std::filesystem::is_symlink(candidate),"RELOCATION_SYMLINK");
  }
  const auto resolved=std::filesystem::canonical(candidate);
  const auto within=resolved.lexically_relative(root);
  require(!within.empty()&&!within.is_absolute(),"RELOCATION_OUTSIDE_ROOT");
  for(const auto& part:within)require(part!="..","RELOCATION_OUTSIDE_ROOT");
  require(std::filesystem::is_regular_file(resolved),"RELOCATION_NOT_FILE");
  require(std::filesystem::file_size(resolved)<=64*1024*1024,"RELOCATION_SIZE_LIMIT");
  const auto expected=match->at("sha256").get<std::string>();
  require(expected.size()==64&&expected.find_first_not_of("0123456789abcdef")==std::string::npos,
          "RELOCATION_INVALID_SHA");
  const auto mapping=match->at("mapping_id").get<std::string>();
  require(!mapping.empty(),"RELOCATION_PROVENANCE_MISSING");
  const auto bytes=ReadRelocationBytes(resolved);
  require(digest(bytes)==expected,"RELOCATION_SHA_MISMATCH");
  return {original,case_id,expected,mapping,resolved};
}

// Validates declared assets only. Never rewrites a script or authorizes replay.
inline nlohmann::json ValidateRelocatedCaseAssets(
    const nlohmann::json& manifest,const nlohmann::json& registry,
    const std::filesystem::path& asset_root,const std::string& case_id,
    const std::function<std::string(const std::string&)>& digest) {
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  const auto& binding=manifest.at("case_binding");
  require(binding.at("case_id")==case_id,"RELOCATION_CASE_MISMATCH");
  require(registry.at("schema")=="cxvision.evidence_case_roots.v1"&&
          registry.at("roots").is_array(),"RELOCATION_ROOT_REGISTRY_INVALID");
  const auto registered=binding.at("registered_root").get<std::string>();
  const std::filesystem::path relRoot(registered);
  require(!registered.empty()&&!relRoot.is_absolute()&&!relRoot.has_root_name()&&
          registered.find(':')==std::string::npos&&registered.find('\\')==std::string::npos,
          "RELOCATION_UNSAFE_ROOT");
  for(const auto& part:relRoot)require(part!=".."&&part!=".","RELOCATION_UNSAFE_ROOT");
  int roots=0;for(const auto& root:registry.at("roots"))if(root==registered)++roots;
  require(roots==1,"RELOCATION_ROOT_NOT_UNIQUE_OR_REGISTERED");
  const auto root=std::filesystem::canonical(asset_root);
  const auto allowed=std::filesystem::canonical(root/relRoot);
  const auto within=allowed.lexically_relative(root);
  for(const auto& part:within)require(part!="..","RELOCATION_OUTSIDE_ROOT");
  auto resolve=[&](const std::string& original){
    auto a=ResolveRelocatedAsset(manifest,root,original,case_id,digest);
    const auto relative=a.resolved_path.lexically_relative(allowed);
    require(!relative.empty()&&!relative.is_absolute(),"RELOCATION_OUTSIDE_REGISTERED_ROOT");
    for(const auto& part:relative)require(part!="..","RELOCATION_OUTSIDE_REGISTERED_ROOT");
    return a;
  };
  const auto receipt=resolve(binding.at("origin_receipt").get<std::string>());
  const auto caseManifest=resolve(binding.at("case_manifest").get<std::string>());
  const auto script=resolve(binding.at("original_script").get<std::string>());
  const auto receiptBytes=ReadRelocationBytes(receipt.resolved_path);
  const auto caseBytes=ReadRelocationBytes(caseManifest.resolved_path);
  require(digest(receiptBytes)==receipt.sha256&&digest(caseBytes)==caseManifest.sha256,
          "RELOCATION_SHA_MISMATCH");
  const auto origin=nlohmann::json::parse(receiptBytes);
  const auto spec=nlohmann::json::parse(caseBytes);
  require(origin.at("script")==script.original_path,"RELOCATION_RECEIPT_SCRIPT_MISMATCH");
  require(spec.at("schema")=="cxvision.evidence_case.v1"&&
          spec.at("internal_case_id")==case_id,"RELOCATION_CASE_MANIFEST_MISMATCH");
  const auto directory=caseManifest.resolved_path.parent_path();
  require(script.resolved_path==(directory/spec.at("script_snapshot").get<std::string>()).lexically_normal(),
          "RELOCATION_SCRIPT_BINDING_MISMATCH");
  const auto image=resolve(origin.at("image").get<std::string>());
  require(image.resolved_path==(directory/spec.at("source_image").get<std::string>()).lexically_normal(),
          "RELOCATION_IMAGE_BINDING_MISMATCH");
  const auto& required=spec.at("required_assets");
  require(required.is_array()&&!required.empty()&&required.size()<=256,"RELOCATION_REQUIRED_ASSETS_INVALID");
  std::set<std::string> names; nlohmann::json assets=nlohmann::json::array();
  for(const auto& item:required) {
    const auto name=item.get<std::string>();
    require(!name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\:")==std::string::npos,
            "RELOCATION_REQUIRED_ASSET_UNSAFE");
    require(names.insert(name).second,"RELOCATION_REQUIRED_ASSET_DUPLICATE");
    const auto relative=(directory/name).lexically_relative(root).generic_string();
    const nlohmann::json* match=nullptr;
    for(const auto& entry:manifest.at("entries")) {
      if(entry.at("case_id")==case_id&&entry.at("relative_path")==relative) {
        require(match==nullptr,"RELOCATION_DEPENDENCY_AMBIGUOUS");match=&entry;
      }
    }
    require(match!=nullptr,"RELOCATION_DEPENDENCY_MISSING");
    const auto a=resolve(match->at("original_path").get<std::string>());
    assets.push_back({{"original_path",a.original_path},{"resolved_path",a.resolved_path.generic_string()},
                      {"sha256",a.sha256},{"mapping_id",a.mapping_id}});
  }
  require(names.count(spec.at("script_snapshot").get<std::string>())&&
          names.count(spec.at("source_image").get<std::string>()),"RELOCATION_CORE_ASSET_UNDECLARED");
  return {{"status","DECLARED_ASSETS_VERIFIED"},{"case_id",case_id},{"assets",assets},
          {"origin_receipt_sha256",receipt.sha256},{"case_manifest_sha256",caseManifest.sha256},
          {"registered_root",registered},{"script_dependencies_semantically_verified",false},
          {"replay_allowed",false},{"production_eligible",false}};
}
}}
