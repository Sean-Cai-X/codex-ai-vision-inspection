#include "../../cximage/CxGrayBoundaryAudit.h"
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
template<class F> void rejects(F f,const std::string& expected){try{f();}catch(const std::exception&e){need(std::string(e.what())==expected,"wrong rejection");return;}throw std::runtime_error("missing rejection");}
int main(int argc,char**argv)try{
 need(argc>=3,"baseline output [verify]");fs::path source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
 if(argc==4){size_t n=0;for(auto row:read(root/"matrix.json")){auto dir=root/row["case_id"].get<std::string>();
  need(read(dir/"native.json")==read(dir/"headless/gray_boundary_receipt_0.json"),"headless mismatch");++n;}
  std::cout<<"FULL_APP_HEADLESS_PASS cases="<<n<<"\n";return 0;}
 need(!fs::exists(root),"fresh output");fs::create_directories(root);J matrix=J::array();size_t total=0;
 for(auto row:read(source/"matrix.json")){
  std::string source_id=row["case_id"];std::string id="full_"+source_id;auto dir=root/id;fs::create_directories(dir);fs::copy_file(source/source_id/"source.pgm",dir/"source.pgm");
  auto img=cv::imread((dir/"source.pgm").string(),0);auto old=read(source/source_id/"aggregate.json");
  CxGrayBoundaryAudit g;g.image((dir/"source.pgm").c_str());g.caseid(id.c_str());J params;
  for(auto p:cxgrayparams::Parameters())params[p.key]=p.initial;
  params["full_boundary_enabled"]=1;params["foreground_polarity"]=1;params["recenter_iterations"]=id.substr(id.size()-1)=="8"?8:0;
  params["anchor_x"]=0;params["anchor_y"]=0;params["anchor_width"]=img.cols;params["anchor_height"]=img.rows;
  std::ostringstream script;script<<"GrayBoundaryAudit g;\ng.caseid(\""<<id<<"\");\ng.image(\""<<(dir/"source.pgm").string()<<"\");\n";
  for(auto p:params.items()){g.parameter(p.value(),p.key().c_str());script<<"g.parameter("<<p.value()<<",\""<<p.key()<<"\");\n";}
  g.run();script<<"g.run();\n";auto receipt=J::parse(g.receipt());auto points=receipt["refinement"]["points"];
  need(points.size()==old["ordered_core_points"].size(),"core size");
  for(size_t i=0;i<points.size();++i){auto a=points[i],b=old["ordered_core_points"][i]["point"];a.erase("source_id");b.erase("source_id");need(a==b,"core point parity");}
  need(receipt["accepted_point_count"]==row["accepted"],"accepted parity");
  need(receipt["full_boundary"]["observed_cycle_complete"]==row["observed_cycle_complete"],"closure parity");
  rejects([&]{g.spans();},"FULL_GRAY_POSE_SPANS_UNSUPPORTED");
  write(dir/"native.json",g.receipt());write(dir/"run.cxsc",script.str());need(cv::imwrite((dir/"overlay.png").string(),g.overlay()),"overlay");
  matrix.push_back({{"case_id",id},{"raw",points.size()},{"accepted",receipt["accepted_point_count"]}});total+=points.size();
  if(matrix.size()==1){
   g.parameter(1,"maximum_chunks");rejects([&]{g.run();},"FULL_GRAY_MAXIMUM_CHUNKS");
   rejects([&]{g.receipt();},"GRAY_SCRIPT_RUN_REQUIRED");
   g.parameter(64,"maximum_chunks");g.parameter(126,"chunk_core_points");rejects([&]{g.run();},"FULL_GRAY_CHUNK_BUDGET");
   g.parameter(100,"chunk_core_points");g.endpointpoint(1,1,"start");g.endpointpoint(2,2,"end");g.endpointpoint(3,3,"via");g.endpointprovenance("guard");
   rejects([&]{g.run();},"FULL_GRAY_ENDPOINT_MODE_CONFLICT");
  }
 }
 cxgeom::polar::BoundaryNormalConfig normal;cxpartialimage::GrayBoundaryConfig gray;
 auto probe=[&](cv::Mat image){cxpartialimage::RefineFullGrayBoundary(image,"guard","v1",128,0,{0,0,image.cols,image.rows},normal,gray);};
 cv::Mat m(100,100,CV_8UC1,cv::Scalar(0));cv::rectangle(m,{10,10,60,60},cv::Scalar(255),-1);cv::rectangle(m,{20,20,10,10},cv::Scalar(0),-1);
 rejects([&]{probe(m);},"FULL_GRAY_SINGLE_COMPONENT_WITHOUT_HOLES_REQUIRED");
 m.setTo(0);cv::rectangle(m,{10,10,20,20},cv::Scalar(255),-1);cv::rectangle(m,{50,50,20,20},cv::Scalar(255),-1);
 rejects([&]{probe(m);},"FULL_GRAY_SINGLE_COMPONENT_WITHOUT_HOLES_REQUIRED");
 m.setTo(0);cv::rectangle(m,{0,10,20,20},cv::Scalar(255),-1);rejects([&]{probe(m);},"FULL_GRAY_BORDER_TOUCH");
 write(root/"matrix.json",matrix.dump(2));std::cout<<"FULL_APP_NATIVE_PASS cases="<<matrix.size()<<" points="<<total<<" guards=7\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
