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
std::vector<Sample> Read(const fs::path& path) {
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
    if(result.size()!=38)throw std::runtime_error("expected 38 v2 holdout fixtures");
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
int main(int argc,char** argv) {
 try {
    if(argc!=3)throw std::runtime_error("usage: cyclic_factor_sweep V2_RUN_DIRECTORY NEW_OUTPUT_DIRECTORY");
    fs::path input=argv[1],output=argv[2];
    if(fs::exists(output))throw std::runtime_error("refuse existing output directory");
    auto original=Read(input/"holdout.csv");
    torch::set_num_threads(1);torch::NoGradGuard guard;
    PoseBranch model(1,2,12);
    torch::serialize::InputArchive archive;archive.load_from((input/"experimental_weights.pt").string());
    model->load(archive);model->eval();
    std::vector<torch::Tensor> before;
    for(auto& p:model->parameters())before.push_back(p.clone());
    for(auto& p:model->buffers())before.push_back(p.clone());
    fs::create_directories(output);
    std::ofstream protocol(output/"protocol.json");
    protocol<<R"({"schema":"cxvision.factor_sweep.v1","scope":"DEVELOPMENT_DIAGNOSTIC","production_eligible":false,"training":false,"K":12,"channels":2,"angle_grid_per_symmetry":72,"scenarios":["baseline","scale_only","intensity_only","translation_only","x_only","y_only","combined","angle_dense"],"reference_mae_deg":1,"reference_p95_deg":2})";
    protocol.close();
    std::ofstream csv(output/"predictions.csv");
    csv<<"scenario,id,q,target_deg,scale,cx,cy,intensity,prediction_deg,valid,error_deg,concentration\n"<<std::setprecision(17);
    for(std::string scenario:{"baseline","scale_only","intensity_only","translation_only","x_only","y_only","combined"}) {
        auto samples=original;
        for(auto& s:samples) {
            if(scenario!="scale_only"&&scenario!="combined")s.scale=1;
            if(scenario!="intensity_only"&&scenario!="combined")s.intensity=1;
            if(scenario!="translation_only"&&scenario!="x_only"&&scenario!="combined")s.x=0;
            if(scenario!="translation_only"&&scenario!="y_only"&&scenario!="combined")s.y=0;
        }
        Run(model,samples,scenario,csv);
    }
    std::vector<Sample> dense;
    for(int q:{1,2,4})for(int i=0;i<72;++i)
        dense.push_back({"dense_"+std::to_string(q)+"_"+std::to_string(i),q,i*(360.0/q)/72,1,0,0,1});
    for(int i=0;i<2;++i)dense.push_back({"dense_circle_"+std::to_string(i),0,0,1,0,0,1});
    Run(model,dense,"angle_dense",csv);
    size_t index=0;
    for(auto& p:model->parameters())TORCH_CHECK(torch::equal(p,before[index++]),"parameter changed");
    for(auto& p:model->buffers())TORCH_CHECK(torch::equal(p,before[index++]),"buffer changed");
    csv.close();if(!csv||!protocol)throw std::runtime_error("evidence write failure");
    std::cout<<"FROZEN_STATE PASS rows=484 acceptance=NOT_EVALUATED"<<std::endl;
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
