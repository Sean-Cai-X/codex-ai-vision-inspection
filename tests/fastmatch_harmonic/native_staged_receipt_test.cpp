// Reuse the native asymmetric ordered-point fixture; image is only a headless carrier.
#define main legacy_pose_receipt_main
#include "native_pose_receipt_test.cpp"
#undef main
void generate_staged(const char* path) {
 generate(path);
 std::ifstream in(path);std::string script((std::istreambuf_iterator<char>(in)),{});
 in.close();auto pos=script.find("a.run();");need(pos!=std::string::npos);
 script.resize(pos);
 script+=R"(a.run();
a.parameter(1,"staged_search");
a.parameter(1,"staged_capture_response");
a.run();
a.expectstatus("STAGED_RESPONSE_REQUIRES_DEBUG");
a.expectcount(0);
a.parameter(1,"debug_mode");
a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.parameter(1,"method");
a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.parameter(10,"staged_maximum_evaluations");
a.run();
a.expectstatus("STAGED_SEARCH_BUDGET_EXHAUSTED");
a.expectcount(0);
a.parameter(65536,"staged_maximum_evaluations");
a.parameter(32,"staged_fine_order");
a.run();
a.expectcount(0);
a.parameter(12,"staged_fine_order");
a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.parameter(0,"staged_search");
a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.save(global_harmonic_receipt_path);
)";
 std::ofstream out(path);out<<script;out.close();need(bool(out));
}
int main(int argc,char** argv)try {
 need(argc==3);std::string mode=argv[1];
 if(mode=="generate"){generate_staged(argv[2]);return 0;}
 need(mode=="check");std::ifstream in(argv[2]);J root;in>>root;
 need(root.at("production_eligible")==false&&root.at("measurement_evidence")==false);
 const auto& runs=root.at("runs");need(runs.size()==8);
 for(const auto& r:runs)need(r.at("debug").at("save_intermediate_features")==false);
 for(int i:{0,7}){const auto& s=runs[i].at("staged_search");
  need(s.at("status")=="DISABLED"&&s.at("executed").is_null());
  need(s.at("coarse_response").empty()&&s.at("fine_response").empty());
  need(runs[i].at("poses").size()==1);}
 for(int i:{1,5}){const auto& s=runs[i].at("staged_search");
  need(s.at("status")=="NOT_RUN"&&s.at("executed").is_null());
  need(s.at("evaluations")==0&&s.at("coarse_response").empty());
  need(runs[i].at("poses").empty());}
 for(int i:{2,3,6}){const auto& r=runs[i];const auto& s=r.at("staged_search");
  need(r.at("status")=="AUDIT_STAGED_POSE_HYPOTHESES"&&r.at("poses").size()==1);
  need(s.at("status")=="COMPLETED"&&s.at("search_complete")==true);
  need(s.at("continuous_global_optimum_certified")==false);
  need(s.at("requested")==s.at("executed"));
  need(s.at("coarse_response").size()==64&&s.at("fine_response").size()==256);
  need(s.at("evaluations").get<int>()<=65536);
  const auto& p=r.at("poses")[0];double x=p.at("translation_x"),y=p.at("translation_y");
  need(std::hypot(x-17.25,y+8.5)<.3);}
 const auto& b=runs[4].at("staged_search");
 need(b.at("status")=="BUDGET_EXHAUSTED"&&b.at("search_complete")==false);
 need(b.at("evaluations").get<int>()<=10&&runs[4].at("poses").empty());
 need(b.at("requested").at("maximum_evaluations")==10);
 need(runs[1].at("status")=="STAGED_RESPONSE_REQUIRES_DEBUG");
 std::cout<<"STAGED_HEADLESS_RECEIPT_PASS disabled/debug/DFT/EFD/budget/invalid/recover"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
