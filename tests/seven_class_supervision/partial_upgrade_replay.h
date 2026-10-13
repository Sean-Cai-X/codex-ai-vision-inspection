#pragma once
namespace partial_upgrade_replay {
using namespace supervision_fixture;
inline int run(int argc,char** argv){
    check(argc==7,"usage: --partial-replay parent baseline-arm candidate-arm rules output-base");
    const auto parent=fs::canonical(argv[2]),baseline=fs::canonical(argv[3]),candidate=fs::canonical(argv[4]);
    const auto rules=mode_budget::json(argv[5]);auto root=fs::canonical(fresh_root(argv[6]));
    auto pm=mode_budget::json(parent/"model_manifest.json");auto old=mode_budget::json(baseline/"result.json"),current=mode_budget::json(candidate/"result.json");
    const auto parentSha=mode_budget::hash(parent/pm.at("weights").get<std::string>());
    check(old.at("parent_sha256")==parentSha&&current.at("parent_sha256")==parentSha,"PARTIAL_PARENT_MISMATCH");
    check(old.at("shape")=="circle"&&current.at("shape")=="circle","PARTIAL_CIRCLE_REQUIRED");
    check(mode_budget::hash(baseline/"weights.pt")==old.at("checkpoint_sha256")&&mode_budget::hash(candidate/"weights.pt")==current.at("checkpoint_sha256"),"PARTIAL_WEIGHT_SHA_MISMATCH");
    auto data=make_case(root/"circle",rules,"circle","none",3);auto project=mode_budget::json(root/"circle/project.json");
    TorchModelManifest manifest;std::string reason;check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"PARTIAL_MANIFEST_INVALID");
    J report={{"schema","visionai.rd.partial_nominal_replay.v1"},{"baseline_sha256",old.at("checkpoint_sha256")},{"candidate_sha256",current.at("checkpoint_sha256")},
        {"source","RND_SYNTHETIC_SAME_GENERATOR"},{"dataset_revision",data.at("dataset_revision_id")},{"independent_accuracy_evidence",false},{"production_allowed",false}};
    for(int arm=0;arm<2;++arm){
        YoloV8Segment model(7);check(model->load_state_dict_strict(((arm?candidate:baseline)/"weights.pt").string()).complete(),"PARTIAL_RELOAD_FAILED");model->eval();torch::NoGradGuard guard;J rows=J::array();
        for(int seed=0;seed<5;++seed){
            const auto path=root/"circle"/(seed==4?"verify":"training")/("sample-"+std::to_string(seed)+".pgm");
            const auto image=cv::imread(path.string());cv::Mat truth;
            if(seed<4)truth=cv::imread((root/"circle/training"/("sample-"+std::to_string(seed)+"-mask.pgm")).string(),cv::IMREAD_GRAYSCALE)==1;
            const auto started=std::chrono::steady_clock::now();SegLetterbox lb;auto raw=model->forward(MakeSegInput(image,manifest,lb));SegNmsDiagnostics nms;
            const auto selected=DecodeCandidates(raw,model->head(),manifest,&nms);std::vector<cxvision::mask_error::Prediction> predictions;
            for(std::size_t i=0;i<selected.size();++i){double q=0,s=0;auto mask=DecodeMask(selected[i],raw.prototypes,manifest,lb,image.size(),q,s);if(LargestContour(mask).size()<3)continue;
                predictions.push_back({"candidate-"+std::to_string(i),selected[i].class_id,selected[i].score,mask>0});}
            rows.push_back({{"seed",seed},{"split",seed<3?"Train":seed==3?"Valid":"VERIFY"},{"image_sha256",mode_budget::hash(path)},
                {"metrics",cxvision::mask_error::AnalyzeSingleGroundTruth(image.size(),1,truth,predictions)},
                {"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()},
                {"nms",{{"above_threshold",nms.above_threshold},{"cap_reached",nms.cap_reached},{"selected",selected.size()},{"returned",predictions.size()}}}});
        }report[arm?"candidate":"baseline"]=rows;
    }
    write_new(root/"nominal_replay.json",report.dump(2));std::cout<<"PARTIAL_REPLAY: "<<root.generic_string()<<std::endl;return 0;
}
}
