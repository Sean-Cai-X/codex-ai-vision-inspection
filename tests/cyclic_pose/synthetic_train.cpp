#include "torch_cyclic_pose_branch.h"
#include "moment_target.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
using namespace cxvision::cyclic;
namespace fs=std::filesystem;
namespace {
constexpr double pi=3.14159265358979323846;
struct Sample {std::string id;int q;double angle,scale,cx,cy,intensity;};
struct Data {torch::Tensor image,q,theta,requested;InstanceMasks masks;std::vector<Sample> samples;};
Data Generate(bool holdout,const fs::path& dir) {
    std::mt19937 rng(holdout?48127:31415);
    auto jitter=[&](){return (double(rng())/double(rng.max())-0.5)*1.5;};
    std::vector<torch::Tensor> images,masks;
    std::vector<int64_t> orders;std::vector<float> angles;
    std::vector<std::vector<std::string>> ids;
    std::vector<Sample> samples;
    auto t=torch::arange(32,torch::kFloat32)-15.5;
    auto x=t.view({1,32}),y=-t.view({32,1});
    std::ofstream manifest(dir/(holdout?"holdout.csv":"train.csv"));
    manifest<<"id,q,angle_deg,scale,cx,cy,intensity\n"<<std::setprecision(17);
    for(int q:{1,2,4,0}) {
        int count=q==0?2:(holdout?12:4);
        for(int i=0;i<count;++i) {
            double period=q?360.0/q:360.0;
            double angle=q?(holdout?(i+0.25)*period/12:i*period/4):0;
            // Holdout angle sets are disjoint modulo the declared symmetry.
            double scale=holdout?(i%2?1.08:0.92):1.0;
            double cx=jitter(),cy=jitter(),intensity=holdout?0.85:1.0;
            double a=angle*pi/180;
            auto u=(std::cos(a)*(x-cx)+std::sin(a)*(y-cy))/scale;
            auto v=(-std::sin(a)*(x-cx)+std::cos(a)*(y-cy))/scale;
            torch::Tensor shape;
            if(q==1)shape=torch::exp(-((u+3).square()/16+v.square()/9))+
                0.8*torch::exp(-((u-4).square()/6+(v-2).square()/4));
            else if(q==2)shape=torch::exp(-(u.square()/49+v.square()/9));
            else if(q==4)shape=torch::sigmoid(3*(5-u.abs()))*torch::sigmoid(3*(5-v.abs()));
            else shape=torch::sigmoid(3*(5-torch::sqrt(u.square()+v.square())));
            shape=shape.clamp(0,1);
            auto mask=shape.gt(0.2).to(torch::kFloat32);
            auto image=shape*intensity;
            std::string id=(holdout?"holdout_":"train_")+std::to_string(q)+"_"+std::to_string(i);
            images.push_back(image.unsqueeze(0));masks.push_back(mask.unsqueeze(0));
            orders.push_back(q);angles.push_back(angle*pi/180);ids.push_back({id});
            samples.push_back({id,q,angle,scale,cx,cy,intensity});
            manifest<<id<<','<<q<<','<<angle<<','<<scale<<','<<cx<<','<<cy<<','<<intensity<<'\n';
            std::ofstream pgm(dir/(id+".pgm"),std::ios::binary);
            pgm<<"P5\n32 32\n255\n";
            auto bytes=(image*255).round().to(torch::kUInt8).contiguous();
            pgm.write(reinterpret_cast<const char*>(bytes.data_ptr<uint8_t>()),1024);
            if(!pgm)throw std::runtime_error("fixture write failed");
        }
    }
    if(!manifest)throw std::runtime_error("manifest write failed");
    auto q=torch::tensor(orders,torch::kInt64).unsqueeze(1);
    Data d{torch::stack(images),q,torch::tensor(angles).unsqueeze(1),q>0,
           {torch::stack(masks),ids,MaskSource::GroundTruth},samples};
    torch::save(d.image,(dir/(holdout?"holdout_images.pt":"train_images.pt")).string());
    torch::save(d.masks.values,(dir/(holdout?"holdout_masks.pt":"train_masks.pt")).string());
    return d;
}
torch::Tensor Loss(const InstancePoseOutput& out,const Data& d) {
    auto target=experiment::MomentTarget(d.theta,d.q,out.logits.size(-1));
    auto kl=(target*(target.clamp_min(1e-20).log()-torch::log_softmax(out.logits,-1))).sum(-1);
    auto valid=out.mask_valid&d.requested;
    if(valid.sum().item<int>()!=12)throw std::runtime_error("unexpected training supervision count");
    return kl.masked_select(valid).mean();
}
struct Metrics {double mean=0,p95=0,maximum=0;int count=0,invalid=0,circle_invalid=0;};
Metrics Evaluate(const InstancePoseOutput& out,const Data& d,std::ostream& csv) {
    std::vector<double> errors;Metrics m;
    auto values=out.angles.angle_rad.cpu(),valid=out.angles.angle_valid.cpu();
    for(size_t i=0;i<d.samples.size();++i) {
        const auto& s=d.samples[i];bool ok=valid[i][0].item<bool>();
        double angle=values[i][0].item<double>()*180/pi;
        double error=s.q?std::abs(std::remainder(angle-s.angle,360.0/s.q)):0;
        if(s.q){++m.count;if(!ok)++m.invalid;else errors.push_back(error);}
        else if(!ok)++m.circle_invalid;
        csv<<s.id<<','<<s.q<<','<<s.angle<<','<<angle<<','<<(ok?1:0)<<',';
        if(s.q&&ok)csv<<error;else csv<<"NA";
        csv<<','<<out.angles.concentration[i][0].item<double>()<<'\n';
    }
    if(errors.empty())m.mean=m.p95=m.maximum=180;
    else {
        std::sort(errors.begin(),errors.end());
        for(auto e:errors)m.mean+=e;
        m.mean/=errors.size();m.maximum=errors.back();
        m.p95=errors[static_cast<size_t>(std::ceil(0.95*errors.size()))-1];
    }
    return m;
}
void JsonMetrics(std::ostream& s,const Metrics& m) {
    s<<"{\"mae_deg\":"<<m.mean<<",\"p95_deg\":"<<m.p95<<",\"max_deg\":"<<m.maximum
      <<",\"angle_count\":"<<m.count<<",\"invalid_count\":"<<m.invalid
      <<",\"circle_invalid_count\":"<<m.circle_invalid<<"}";
}
}
int main(int argc,char** argv){
 try{
    if(argc!=2)throw std::runtime_error("usage: cyclic_synthetic_train NEW_EXTERNAL_DIRECTORY");
    fs::path dir=argv[1];
    if(fs::exists(dir))throw std::runtime_error("refuse existing output directory");
    fs::create_directories(dir);
    torch::set_num_threads(1);torch::manual_seed(918);
    // Fixed BEFORE training; no holdout-based selection or early stopping.
    std::ofstream protocol(dir/"protocol.json");
    protocol<<R"({"schema":"cxvision.synthetic_pose_protocol.v2","seed":918,"train_seed":31415,"holdout_seed":48127,"holdout_angle_offset":0.25,"steps":240,"lr":0.01,"K":12,"channels":2,"target":"moment_cosine","rho":0.4,"overfit":{"loss_ratio_max":0.35,"mae_deg_max":2,"max_deg_max":5},"holdout":{"mae_deg_max":1,"p95_deg_max":2},"production_eligible":false})";
    protocol.close();
    auto train=Generate(false,dir);
    PoseBranch model(1,2,12);
    model->eval();
    double initial;
    {torch::NoGradGuard guard;initial=Loss(model->forward(train.image,train.masks,train.q,train.requested),train).item<double>();}
    torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(0.01));
    std::ofstream curve(dir/"training_curve.csv");curve<<"step,kl\n";
    model->train();
    for(int step=0;step<240;++step) {
        optimizer.zero_grad();
        auto out=model->forward(train.image,train.masks,train.q,train.requested);
        auto loss=Loss(out,train);
        if(!torch::isfinite(loss).item<bool>())throw std::runtime_error("nonfinite loss");
        loss.backward();
        for(const auto& p:model->parameters())
            if(p.grad().defined()&&!torch::isfinite(p.grad()).all().item<bool>())
                throw std::runtime_error("nonfinite gradient");
        optimizer.step();
        curve<<step<<','<<std::setprecision(10)<<loss.item<double>()<<'\n';
        if(step%20==0)std::cout<<"STEP "<<step<<" kl="<<loss.item<double>()<<std::endl;
    }
    curve.close();model->eval();
    torch::NoGradGuard guard;
    auto final_train=model->forward(train.image,train.masks,train.q,train.requested);
    double final_loss=Loss(final_train,train).item<double>();
    // Freeze final weights BEFORE constructing or evaluating holdout.
    torch::serialize::OutputArchive archive;model->save(archive);archive.save_to((dir/"experimental_weights.pt").string());
    auto holdout=Generate(true,dir);
    for(const auto& a:train.samples)for(const auto& b:holdout.samples)
        if(a.q&&a.q==b.q&&std::abs(std::remainder(a.angle-b.angle,360.0/a.q))<1e-6)
            throw std::runtime_error("angle split leakage");
    auto final_holdout=model->forward(holdout.image,holdout.masks,holdout.q,holdout.requested);
    std::ofstream csv(dir/"angle_predictions.csv");
    csv<<"id,q,target_deg,prediction_deg,valid,error_deg,concentration\n"<<std::setprecision(10);
    auto tm=Evaluate(final_train,train,csv),hm=Evaluate(final_holdout,holdout,csv);csv.close();
    PoseBranch reload(1,2,12);torch::serialize::InputArchive input;input.load_from((dir/"experimental_weights.pt").string());
    reload->load(input);reload->eval();
    bool roundtrip=torch::equal(final_holdout.logits,
        reload->forward(holdout.image,holdout.masks,holdout.q,holdout.requested).logits);
    bool overfit=final_loss<=initial*0.35&&tm.invalid==0&&tm.mean<=2&&tm.maximum<=5&&tm.circle_invalid==2;
    bool generalization=hm.invalid==0&&hm.mean<=1&&hm.p95<=2&&hm.circle_invalid==2;
    std::ofstream report(dir/"training_receipt.json");
    report<<std::setprecision(12)<<"{\"schema\":\"cxvision.synthetic_pose_training.v2\",\"production_eligible\":false,"
        <<"\"initial_kl\":"<<initial<<",\"final_kl\":"<<final_loss
        <<",\"overfit_pass\":"<<(overfit?"true":"false")
        <<",\"holdout_pass\":"<<(generalization?"true":"false")
        <<",\"archive_exact\":"<<(roundtrip?"true":"false")<<",\"train\":";
    JsonMetrics(report,tm);report<<",\"holdout\":";JsonMetrics(report,hm);report<<"}\n";report.close();
    if(!report||!csv||!curve||!protocol)throw std::runtime_error("evidence write failed");
    std::cout<<"FINAL initial_kl="<<initial<<" final_kl="<<final_loss
        <<" train_mae="<<tm.mean<<" holdout_mae="<<hm.mean<<" holdout_p95="<<hm.p95
        <<" overfit="<<overfit<<" holdout="<<generalization<<" archive="<<roundtrip<<std::endl;
    return roundtrip?(overfit&&generalization?0:2):1;
 }catch(const std::exception& e){std::cerr<<"ERROR "<<e.what()<<std::endl;return 1;}
}
