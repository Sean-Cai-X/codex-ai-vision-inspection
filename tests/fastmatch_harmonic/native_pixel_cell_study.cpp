#include "CxGeoPixelCellBoundary.h"
#include "CxGeoSO2ReferenceAsset.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
using J=nlohmann::json;using Z=std::complex<double>;namespace fs=std::filesystem;using namespace cxgeom::so2;
void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
struct Input {Contour contour;J provenance;};
Input extract(fs::path path,bool cell){
 cv::Mat im=cv::imread(path.string(),0),mask;need(!im.empty(),"image");cv::threshold(im,mask,128,255,cv::THRESH_BINARY_INV);
 Input in;in.contour.closed=true;in.contour.topology_verified=true;J points=J::array();
 if(cell){
  std::vector<uint8_t> bytes(mask.datastart,mask.dataend);auto r=cxgeom::TraceSinglePixelCellBoundary(bytes,mask.cols,mask.rows);
  in.contour.points=r.points;
  for(size_t i=0;i<r.points.size();++i)points.push_back({{"x",r.points[i].real()},{"y",r.points[i].imag()},{"pixel_x",r.edges[i].x},{"pixel_y",r.edges[i].y},{"cell_side",r.edges[i].side}});
 }else{
  std::vector<std::vector<cv::Point>> curves;std::vector<cv::Vec4i> hierarchy;cv::findContours(mask.clone(),curves,hierarchy,cv::RETR_TREE,cv::CHAIN_APPROX_NONE);
  need(curves.size()==1,"single source");
  for(auto p:curves[0]){in.contour.points.emplace_back(p.x,p.y);points.push_back({{"x",p.x},{"y",p.y}});}
 }
 std::ifstream f(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});
 in.provenance={{"image_sha256",Sha256Bytes(bytes)},{"image_path",path.string()},{"threshold",128},{"polarity","dark_foreground_le_128"},
 {"representation",cell?"unit_pixel_cell_union_boundary":"foreground_boundary_pixel_centers"},{"physical_subpixel_estimate",false},{"points",points}};
 return in;
}
int main(int argc,char**argv)try{
 need(argc==3,"original replay / fresh output");fs::path original=argv[1],root=argv[2];need(!fs::exists(root),"fresh output");fs::create_directories(root);
 // Exact area/perimeter and rejection tests independent of image fixtures.
 std::vector<uint8_t> mask(100,0);for(int y=3;y<6;++y)for(int x=3;x<5;++x)mask[y*10+x]=1;
 auto rectangle=cxgeom::TraceSinglePixelCellBoundary(mask,10,10);double area=0,perimeter=0;
 for(size_t i=0;i<rectangle.points.size();++i){auto a=rectangle.points[i],b=rectangle.points[(i+1)%rectangle.points.size()];area+=std::imag(std::conj(a)*b)/2;perimeter+=std::abs(b-a);}
 need(std::abs(area)==6&&perimeter==10,"cell geometry");
 auto rejects=[&](std::vector<uint8_t> m){bool thrown=false;try{cxgeom::TraceSinglePixelCellBoundary(m,10,10);}catch(const std::invalid_argument&){thrown=true;}need(thrown,"negative");};
 rejects(std::vector<uint8_t>(100));auto two=mask;two[77]=1;rejects(two);auto diagonal=std::vector<uint8_t>(100);diagonal[33]=diagonal[44]=1;rejects(diagonal);
 auto hole=std::vector<uint8_t>(100);for(int y=2;y<7;++y)for(int x=2;x<7;++x)hole[y*10+x]=1;hole[44]=0;rejects(hole);auto border=mask;border[0]=1;rejects(border);
 J matrix=J::array();
 for(bool cell:{false,true})for(int method:{0,1}){
  auto ref=extract(original/"fixtures/notched_base.pgm",cell);auto m=method?Method::Efd:Method::Dft;
  auto reference=Build(ref.contour,Config{},m);std::string basis=cell?"cell":"center",prefix=basis+"_m"+std::to_string(method);
  auto bytes=EncodeReference({reference,"notched_binary:"+basis});write(root/(prefix+".so2"),bytes);write(root/(prefix+"_reference.json"),ref.provenance.dump(2));
  std::ifstream cases(original/"fixtures/complex.cases");std::string family,name,mode,status;double angle,scale;
  while(cases>>family>>name>>angle>>scale>>mode>>status){if(family!="notched")continue;
   auto observation=extract(original/"fixtures"/(name+".pgm"),cell);if(mode=="partial")observation.contour.complete=false;
   Result r;try{r=Match(reference,Build(observation.contour,Config{},m));}catch(const std::invalid_argument& e){r.status=e.what();}
   auto d=root/(prefix+"_"+name);fs::create_directories(d);write(d/"observation.json",observation.provenance.dump(2));
   J row={{"basis",basis},{"method",method},{"case",name},{"status",r.status},{"poses",J::array()},{"production_eligible",false}};
   for(auto p:r.poses){Z c(199.5,199.5),truth=std::polar(scale,angle*std::acos(-1.)/180),tt=c-truth*c;
    row["poses"].push_back({{"angle_deg",p.angle_deg},{"scale",p.scale},{"translation_x",p.translation_x},{"translation_y",p.translation_y},
    {"angle_error_deg",std::abs(std::remainder(p.angle_deg-angle,360))},{"scale_error",std::abs(p.scale-scale)},{"translation_error_px",std::abs(Z(p.translation_x,p.translation_y)-tt)},
    {"center_error_px",std::abs(std::polar(p.scale,p.angle_deg*std::acos(-1.)/180)*c+Z(p.translation_x,p.translation_y)-c)}});}
   if(mode=="mirror"||mode=="partial")need(r.poses.empty(),"negative accepted");else need(r.succeeded,"positive rejected");
   write(d/"native.json",row.dump(2));matrix.push_back(row);
   std::ostringstream s;s<<std::setprecision(17)<<"Image carrier; FindObject obj; carrier.copyFromMat(global_matInput); obj.setrect(0,0,400,400); obj.setsearchtype(0); obj.setdistance(1); obj.setbrow(2); obj.setroithre(128); obj.setminmax(1,99999999); obj.measurecc(carrier);\n";
   s<<"// Observation points below are native image-derived cell/center coordinates, not analytic truth.\nHarmonicAudit a; a.parameter("<<method<<",\"method\"); a.select(0); a.trustedsha(\""<<ReferenceSha256(bytes)<<"\"); a.loadasset(\""<<fs::absolute(root/(prefix+".so2")).string()<<"\"); a.select(1);\n";
   for(auto p:observation.contour.points)s<<"a.point("<<p.real()<<","<<p.imag()<<");\n";
   s<<"a.topology(1,1,"<<(mode=="partial"?0:1)<<",0,1); a.run(); a.save(global_harmonic_receipt_path);\n";write(d/"run.cxsc",s.str());
  }
 }
 write(root/"matrix.json",matrix.dump(2));std::cout<<"CELL_REPRESENTATION_PASS cases="<<matrix.size()<<" negatives=empty,multiple,diagonal,hole,border\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
