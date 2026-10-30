#include "torch_cyclic_pose_branch.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
using namespace cxvision::cyclic;
namespace fs=std::filesystem;
constexpr double pi=3.14159265358979323846;
struct Sample {std::string id;int q;double angle,scale,x,y,intensity;};
std::vector<Sample> Read(const fs::path& path,size_t expected=38) {
    std::ifstream in(path);std::string line;
    if(!std::getline(in,line)||line!="id,q,angle_deg,scale,cx,cy,intensity")
        throw std::runtime_error("unexpected holdout manifest");
    std::vector<Sample> result;
    while(std::getline(in,line)) {
        std::stringstream s(line);std::vector<std::string> f;std::string value;
        while(std::getline(s,value,','))f.push_back(value);
        if(f.size()!=7)throw std::runtime_error("invalid fixture row");
        result.push_back({f[0],std::stoi(f[1]),std::stod(f[2]),std::stod(f[3]),
                          std::stod(f[4]),std::stod(f[5]),std::stod(f[6])});
    }
    if(result.size()!=expected)throw std::runtime_error("unexpected fixture count");
    return result;
}
std::pair<torch::Tensor,torch::Tensor> Render(const Sample& s) {
    auto t=torch::arange(32,torch::kFloat32)-15.5;
    auto x=t.view({1,32}),y=-t.view({32,1});
    double a=s.angle*pi/180;
    auto u=(std::cos(a)*(x-s.x)+std::sin(a)*(y-s.y))/s.scale;
    auto v=(-std::sin(a)*(x-s.x)+std::cos(a)*(y-s.y))/s.scale;
    torch::Tensor shape;
    if(s.q==1)shape=torch::exp(-((u+3).square()/16+v.square()/9))+
        .8*torch::exp(-((u-4).square()/6+(v-2).square()/4));
    else if(s.q==2)shape=torch::exp(-(u.square()/49+v.square()/9));
    else if(s.q==4)shape=torch::sigmoid(3*(5-u.abs()))*torch::sigmoid(3*(5-v.abs()));
    else if(s.q==0)shape=torch::sigmoid(3*(5-torch::sqrt(u.square()+v.square())));
    else throw std::runtime_error("invalid q");
    shape=shape.clamp(0,1);
    return {shape*s.intensity,shape.gt(.2).to(torch::kFloat32)};
}
void Run(PoseBranch& model,const std::vector<Sample>& samples,const std::string& scenario,
         std::ostream& csv) {
    std::vector<torch::Tensor> images,masks;
    std::vector<int64_t> qs;std::vector<std::vector<std::string>> ids;
    for(const auto& s:samples) {
        auto r=Render(s);images.push_back(r.first.unsqueeze(0));masks.push_back(r.second.unsqueeze(0));
        qs.push_back(s.q);ids.push_back({s.id});
    }
    auto q=torch::tensor(qs,torch::kInt64).unsqueeze(1);
    InstanceMasks im{torch::stack(masks),ids,MaskSource::GroundTruth};
    auto out=model->forward(torch::stack(images),im,q,q>0);
    for(size_t i=0;i<samples.size();++i) {
        const auto& s=samples[i];bool valid=out.angles.angle_valid[i][0].item<bool>();
        double pred=out.angles.angle_rad[i][0].item<double>()*180/pi;
        csv<<scenario<<','<<s.id<<','<<s.q<<','<<s.angle<<','<<s.scale<<','<<s.x<<','<<s.y
           <<','<<s.intensity<<','<<pred<<','<<valid<<',';
        if(valid&&s.q)csv<<std::abs(std::remainder(pred-s.angle,360.0/s.q));else csv<<"NA";
        csv<<','<<out.angles.concentration[i][0].item<double>()<<'\n';
    }
    std::cout<<"SCENARIO "<<scenario<<" rows="<<samples.size()<<std::endl;
}
// Separate diagnostic translation unit preserves the preregistered evaluator sources.
// Rendering/inference helpers intentionally match the locked evaluator; audit verifies
// that factorial mask15 reproduces every q1 locked prediction.
int main(int argc,char** argv) {
 try {
    if(argc!=4)throw std::runtime_error("usage: cyclic_q1_tail_probe MODEL_RUN LOCKED_MANIFEST NEW_OUTPUT");
    fs::path input=argv[1],output=argv[3];
    if(fs::exists(output))throw std::runtime_error("refuse existing output directory");
    auto all=Read(argv[2],372);
    std::vector<Sample> original;
    for(auto s:all)if(s.q==1)original.push_back(s);
    if(original.size()!=120)throw std::runtime_error("expected 120 q1 fixtures");
    torch::set_num_threads(1);torch::NoGradGuard guard;
    PoseBranch model(1,2,12);
    torch::serialize::InputArchive archive;archive.load_from((input/"experimental_weights.pt").string());
    model->load(archive);model->eval();
    std::vector<torch::Tensor> before;
    for(auto& p:model->parameters())before.push_back(p.clone());
    for(auto& p:model->buffers())before.push_back(p.clone());
    fs::create_directories(output);
    std::ofstream protocol(output/"protocol.json");
    protocol<<R"({"schema":"cxvision.q1_tail_probe.v1","scope":"DEVELOPMENT_DIAGNOSTIC","training":false,"production_eligible":false,"rows":2400,"factor_bits":{"scale":1,"intensity":2,"x":4,"y":8},"angle_probes":["snap_clean","snap_observed","minus5","plus5"],"tail_definition":"original_locked_error_gt_2deg"})";
    protocol.close();
    std::ofstream csv(output/"predictions.csv");
    csv<<"scenario,id,q,target_deg,scale,cx,cy,intensity,prediction_deg,valid,error_deg,concentration\n"<<std::setprecision(17);
    for(int mask=0;mask<16;++mask) {
        auto samples=original;
        for(auto& s:samples) {
            if(!(mask&1))s.scale=1;
            if(!(mask&2))s.intensity=1;
            if(!(mask&4))s.x=0;
            if(!(mask&8))s.y=0;
        }
        Run(model,samples,"factor_"+std::to_string(mask),csv);
    }
    for(std::string scenario:{"snap_clean","snap_observed","minus5","plus5"}) {
        auto samples=original;
        for(auto& s:samples) {
            if(scenario=="snap_clean"||scenario=="snap_observed")
                s.angle=std::fmod(90*std::floor(s.angle/90+.5),360.0);
            else s.angle=std::fmod(s.angle+(scenario=="minus5"?355:5),360.0);
            if(scenario=="snap_clean") {s.scale=1;s.intensity=1;s.x=0;s.y=0;}
        }
        Run(model,samples,scenario,csv);
    }
    size_t index=0;
    for(auto& p:model->parameters())TORCH_CHECK(torch::equal(p,before[index++]),"parameter changed");
    for(auto& p:model->buffers())TORCH_CHECK(torch::equal(p,before[index++]),"buffer changed");
    csv.close();if(!csv||!protocol)throw std::runtime_error("evidence write failure");
    std::cout<<"FROZEN_STATE PASS rows=2400 acceptance=NOT_EVALUATED"<<std::endl;
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
