#pragma once
#include "CxGeometryImagePyramid.h"
#include "CxGrayBoundaryManualRun.h"
#include <opencv2/imgcodecs.hpp>

// Independent, explicit source-file diagnostic. Never replaces Image View,
// FindLine's sampling policy, or the selected case's pose result.
inline void DrawGeometryPyramidControls(ManualTestContext& c) {
 using namespace cximage::geometry;
 struct State {
  std::string binding,summary="NOT RUN",receipt;
  ImagePyramidConfig config;
  int preview_method=1,preview_level=0;
  double preview_factor=2;
  int roi[4]={0,0,0,0};
 };
 static State s;
 const std::string path=c.image_file_path;
 std::error_code ec;
 const auto modified=std::filesystem::last_write_time(path,ec);
 const auto binding=c.active_case_id+"\n"+path+"\n"+c.editor_text+"\n"+
  (ec?"UNAVAILABLE":std::to_string(modified.time_since_epoch().count()));
 if(s.binding!=binding){s=State{};s.binding=binding;}
 if(!ImGui::CollapsingHeader("Image Pyramid / Source Mapping Diagnostic"))return;
 ImGui::TextWrapped("Source file: %s",path.c_str());
 ImGui::TextWrapped("Independent diagnostic, not the case pose pipeline. Original-image remeasurement remains required. Enlargement is display only.");
 ImGui::BeginDisabled(c.has_pending_execution_snapshot);
 bool changed=false;
 if(ImGui::BeginTable("pyramid_parameters",2,ImGuiTableFlags_SizingStretchProp)){
  ImGui::TableSetupColumn("Parameter",ImGuiTableColumnFlags_WidthStretch,.55f);
  ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,.45f);
  auto row=[](const char* label){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextWrapped("%s",label);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);};
  row("Downsample method");changed|=ImGui::Combo("##down",&s.config.downsample_method,"Gaussian pyrDown\0Area\0Linear\0Cubic\0");
  row("Maximum levels (1..12)");changed|=ImGui::InputInt("##levels",&s.config.maximum_levels);
  row("Minimum dimension (px, >=2)");changed|=ImGui::InputInt("##mindim",&s.config.minimum_dimension);
  row("Scale step (1.1..4)");changed|=ImGui::InputDouble("##step",&s.config.scale_step);
  row("Extra Gaussian sigma (0..4 level px)");changed|=ImGui::InputDouble("##sigma",&s.config.prefilter_sigma);
  row("ROI x, y, width, height (original px)");changed|=ImGui::InputInt4("##roi",s.roi);
  row("Preview level (0-based)");changed|=ImGui::InputInt("##previewlevel",&s.preview_level);
  row("Display enlargement (1..4)");changed|=ImGui::InputDouble("##factor",&s.preview_factor);
  row("Display interpolation");changed|=ImGui::Combo("##up",&s.preview_method,"Nearest\0Linear\0Cubic\0Lanczos4\0");
  ImGui::EndTable();
 }
 ImGui::TextWrapped("ROI 0,0,0,0 = full image. Gaussian pyrDown requires step 2 and extra sigma 0. Linear/Cubic without prefilter may alias; compare against the original.");
 if(ImGui::Button("Reset Pyramid Defaults")){const auto key=s.binding;s=State{};s.binding=key;changed=true;}
 if(changed){s.receipt.clear();s.summary="STALE: parameters changed; run again.";}
 ImGui::BeginDisabled(path.empty()||ec.operator bool()||c.active_case_id.empty());
 if(ImGui::Button("Generate Pyramid Evidence Preview")){
  s.receipt.clear();s.summary="FAILED";
  try{
   const auto source=cv::imread(path,cv::IMREAD_UNCHANGED);
   if(source.empty()||source.depth()!=CV_8U||source.total()>16000000)throw std::runtime_error("Source must be 8-bit, at most 16 million pixels.");
   cv::Mat gray;
   if(source.channels()==1)gray=source;
   else if(source.channels()==3)cv::cvtColor(source,gray,cv::COLOR_BGR2GRAY);
   else if(source.channels()==4)cv::cvtColor(source,gray,cv::COLOR_BGRA2GRAY);
   else throw std::runtime_error("Unsupported source channels.");
   cv::Rect roi(s.roi[0],s.roi[1],s.roi[2],s.roi[3]);
   if(roi==cv::Rect())roi={0,0,gray.cols,gray.rows};
   const auto sha=cxgraymanual::ImageSha(gray);
   const auto pyramid=BuildGeometryImagePyramid(gray,roi,sha,s.config);
   if(s.preview_level<0||s.preview_level>=int(pyramid.levels.size()))throw std::runtime_error("Preview level unavailable; reduce level index.");
   const auto preview=EnlargePyramidPreview(pyramid.levels[s.preview_level],s.preview_factor,s.preview_method);
   const auto root=ResolveCxVisionRunPath("cxscript_runs/geometry_pyramid_manual");
   std::filesystem::create_directories(root);
   const auto dir=root/("run_"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
   if(!std::filesystem::create_directory(dir))throw std::runtime_error("Run directory already exists.");
   nlohmann::json rows=nlohmann::json::array();
   for(const auto& level:pyramid.levels){
    const auto file="level_"+std::to_string(level.index)+".png";
    if(!cv::imwrite((dir/file).string(),level.gray))throw std::runtime_error("Level image write failed.");
    rows.push_back({{"level",level.index},{"width",level.gray.cols},{"height",level.gray.rows},
     {"scale_x",level.original_units_per_pixel},{"scale_y",level.ScaleY()},
     {"offset_x",level.original_offset.x},{"offset_y",level.original_offset.y},{"file",file},
     {"gray_sha256",cxgraymanual::ImageSha(level.gray)}});
   }
   if(!cv::imwrite((dir/"display_only.png").string(),preview))throw std::runtime_error("Preview image write failed.");
   nlohmann::json receipt={{"schema","geometry_pyramid_preview.v1"},{"case_id",c.active_case_id},
    {"source_gray_sha256",sha},{"script_sha256",cxgeom::so2::Sha256Bytes(c.editor_text)},
    {"source_kind","selected_source_file_not_live_view"},{"roi",{roi.x,roi.y,roi.width,roi.height}},
    {"method",pyramid.filter},{"scale_step",s.config.scale_step},{"sigma_level_px",s.config.prefilter_sigma},
    {"maximum_levels",s.config.maximum_levels},{"minimum_dimension",s.config.minimum_dimension},
    {"preview_level",s.preview_level},{"preview_factor",s.preview_factor},{"preview_method",s.preview_method},
    {"levels",rows},{"production_eligible",false},{"pose_executed",false},{"human_review","PENDING"}};
   cxgraymanual::Write(dir/"receipt.json",receipt.dump(2));
   s.receipt=receipt.dump(2);s.summary="PREVIEW GENERATED: "+dir.string();
  }catch(const std::exception& e){s.summary=std::string("FAILED: ")+e.what();}
 }
 ImGui::EndDisabled();ImGui::EndDisabled();
 ImGui::TextWrapped("%s",s.summary.c_str());
 if(!s.receipt.empty()&&ImGui::TreeNode("Executed pyramid parameters and mapping")){
  ImGui::TextWrapped("%s",s.receipt.c_str());ImGui::TreePop();
 }
}
