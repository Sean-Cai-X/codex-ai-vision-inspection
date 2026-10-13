#include "../../cximage/CxGrayBoundaryAudit.h"
#include "CxGeoBoundaryChunkPlan.h"
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
int main(int argc,char**argv)try{
 need(argc>=3,"source matrix / output [verify]");fs::path source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
 if(argc==4){int n=0;for(auto row:read(root/"matrix.json"))for(auto chunk:row["chunks"]){
  auto dir=root/row["case_id"].get<std::string>()/chunk.get<std::string>();
  need(read(dir/"native.json")==read(dir/"headless/gray_boundary_receipt_0.json"),"headless parity");++n;
 }std::cout<<"FULL_BOUNDARY_HEADLESS_PARITY_PASS chunks="<<n<<"\n";return 0;}
 need(!fs::exists(root),"fresh output");fs::create_directories(root);J matrix=J::array();size_t total=0,coreTotal=0,acceptedTotal=0;
 for(auto row:read(source/"matrix.json")){
  std::string id=row["case"];int iterations=row["recenter_iterations"];std::string name=id+"_r"+std::to_string(iterations);auto dir=root/name;fs::create_directories(dir);
  need(fs::copy_file(source/id/"source.pgm",dir/"source.pgm"),"source copy");auto image=dir/"source.pgm";
  cv::Mat gray=cv::imread(image.string(),0),mask;need(!gray.empty(),"image");cv::threshold(gray,mask,128,255,cv::THRESH_BINARY_INV);
  std::vector<std::vector<cv::Point>> curves;cv::findContours(mask,curves,cv::RETR_TREE,cv::CHAIN_APPROX_NONE);need(curves.size()==1,"single closed contour required");
  auto parent=curves[0];auto plan=cxgeom::PlanClosedBoundaryChunks(parent.size());std::vector<int>ownership(parent.size());std::vector<bool> accepted(parent.size());
  J scores=read(source/id/("recenter"+std::to_string(iterations)+".json"));J ordered=J::array();
  std::vector<J> points(parent.size());J chunkNames=J::array();
  for(size_t ci=0;ci<plan.size();++ci){auto c=plan[ci];auto chunkName="chunk_"+std::to_string(ci);auto out=dir/chunkName;fs::create_directories(out);
   CxGrayBoundaryAudit g;auto caseid=name+"_"+chunkName;g.caseid(caseid.c_str());g.image(image.c_str());
   J params;for(auto p:cxgrayparams::Parameters())params[p.key]=p.initial;
   params["foreground_polarity"]=1;params["recenter_iterations"]=iterations;params["anchor_x"]=0;params["anchor_y"]=0;params["anchor_width"]=gray.cols;params["anchor_height"]=gray.rows;
   std::ostringstream s;s<<"GrayBoundaryAudit g;\ng.caseid(\""<<caseid<<"\");\ng.image(\""<<image.string()<<"\");\n";
   for(auto p:params.items()){g.parameter(p.value(),p.key().c_str());s<<"g.parameter("<<p.value()<<",\""<<p.key()<<"\");\n";}
   for(auto sel:std::vector<std::pair<const char*,size_t>>{{"start",0},{"end",c.parent_indices.size()-1},{"via",c.parent_indices.size()/2}})
    for(int axis:{0,1}){auto p=parent[c.parent_indices[sel.second]];double v=axis?p.y:p.x;std::string key=std::string(sel.first)+(axis?"_y":"_x");g.endpointparameter(v,key.c_str());s<<"g.endpointparameter("<<v<<",\""<<key<<"\");\n";}
   g.endpointparameter(1e-6,"maximum_snap_px");g.endpointprovenance("image_extracted_ordered_chunk_not_pose_landmarks");
   s<<"g.endpointparameter(1e-6,\"maximum_snap_px\");\ng.endpointprovenance(\"image_extracted_ordered_chunk_not_pose_landmarks\");\ng.run();\n";g.run();
   auto receipt=J::parse(g.receipt());auto actual=receipt["refinement"]["points"];
   need(actual.size()==c.parent_indices.size(),"selected count");
   need(receipt["binding"]["gray_sha256"]==scores["chunks"][ci]["gray_sha256"],"gray SHA");
   for(size_t k=c.halo;k<c.halo+c.core_count;++k){size_t ix=c.parent_indices[k];need(++ownership[ix]==1,"duplicate core");
    auto a=actual[k],b=scores["chunks"][ci]["points"][k];a.erase("source_id");b.erase("source_id");need(a==b,"same point refinement");
    accepted[ix]=actual[k]["accepted"];points[ix]={{"parent_index",ix},{"chunk_index",ci},{"local_index",k},{"source_id",actual[k]["source_id"]},{"point",actual[k]}};
   }
   write(out/"native.json",receipt.dump(2));write(out/"run.cxsc",s.str());
   write(out/"mapping.json",J({{"parent_indices",c.parent_indices},{"core_begin",c.core_begin},{"core_count",c.core_count},{"halo",c.halo},{"wraps_parent_origin",c.wraps_parent_origin},{"count_halo_in_scores",false}}).dump(2));
   chunkNames.push_back(chunkName);++total;
  }
  for(auto n:ownership)need(n==1,"uncovered raw point");
  auto runs=cxgeom::OrderedAcceptedBoundaryRuns(accepted);J spans=J::array(),rejected=J::array();
  for(auto run:runs.runs)spans.push_back({{"parent_indices",run},{"closed",false},{"synthetic_points",0}});
  for(size_t i=0;i<points.size();++i){ordered.push_back(points[i]);if(!accepted[i])rejected.push_back({{"parent_index",i},{"status",points[i]["point"]["status"]}});}
  need(rejected.size()==row["rejected"].get<size_t>(),"full rejection baseline");
  cv::Mat overlay;cv::cvtColor(gray,overlay,cv::COLOR_GRAY2BGR);auto pt=[](const J&p){return cv::Point(cvRound(p[0].get<double>()*16),cvRound(p[1].get<double>()*16));};
  for(size_t i=0;i<points.size();++i){size_t j=(i+1)%points.size();cv::line(overlay,pt(points[i]["point"]["raw"]),pt(points[j]["point"]["raw"]),{255,255,0},1,cv::LINE_AA,4);
   if(!accepted[i])cv::circle(overlay,pt(points[i]["point"]["raw"]),16,{0,0,255},1,cv::LINE_AA,4);}
  for(auto run:runs.runs)for(size_t k=1;k<run.size();++k)cv::line(overlay,pt(points[run[k-1]]["point"]["refined"]["point"]),pt(points[run[k]]["point"]["refined"]["point"]),{0,220,0},1,cv::LINE_AA,4);
  need(cv::imwrite((dir/"overlay.png").string(),overlay),"overlay");
  J summary={{"case_id",name},{"raw_count",parent.size()},{"accepted",parent.size()-rejected.size()},{"rejected",rejected.size()},{"chunks",chunkNames},{"halo_scored",false},{"pose_evaluated",false},{"production_eligible",false},{"observed_cycle_complete",runs.observed_cycle_complete},{"seam_supported",runs.seam_supported}};
  write(dir/"aggregate.json",J({{"schema","ordered_boundary_batch.v1"},{"summary",summary},{"ordered_core_points",ordered},{"accepted_runs",spans},{"rejected_indices",rejected}}).dump(2));matrix.push_back(summary);coreTotal+=parent.size();acceptedTotal+=parent.size()-rejected.size();
 }
 write(root/"matrix.json",matrix.dump(2));std::cout<<"FULL_BOUNDARY_NATIVE_PASS cases="<<matrix.size()<<" chunks="<<total<<" raw_core="<<coreTotal<<" accepted="<<acceptedTotal<<" rejected="<<coreTotal-acceptedTotal<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
