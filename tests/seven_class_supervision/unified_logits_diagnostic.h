// Read-only R&D diagnostics for the exact production class-max decode path.
namespace unified_logits {
using namespace supervision_fixture;
inline int run(int argc,char** argv) {
    check(argc==8,"usage: --unified-logits binding parent_root weights shape case_root output_base");
    auto binding=mode_budget::json(argv[2]);const auto parent=fs::canonical(argv[3]);
    const auto candidate=fs::canonical(argv[4]),case_root=fs::canonical(argv[6]);
    const std::string shape=argv[5];check(shape=="circle"||shape=="ellipse","LOGITS_SHAPE_INVALID");
    auto pm=mode_budget::json(parent/"model_manifest.json");
    check(mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/pm.at("weights").get<std::string>())==binding.at("checkpoint_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256"),"LOGITS_PARENT_SHA");
    const auto parent_before=mode_budget::inventory(parent),case_before=mode_budget::inventory(case_root);
    TorchModelManifest manifest;std::string reason;
    check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"LOGITS_MANIFEST");
    YoloV8Segment model(7);check(model->load_state_dict_strict(candidate.string()).complete(),"LOGITS_STRICT_LOAD");
    model->eval();torch::NoGradGuard guard;
    J samples=J::array();
    for(int seed=0;seed<5;++seed) {
        const auto path=case_root/(seed==4?"verify":"training")/("sample-"+std::to_string(seed)+".pgm");
        auto image=cv::imread(path.string());check(!image.empty(),"LOGITS_IMAGE_UNREADABLE");
        SegLetterbox lb;auto raw=model->forward(MakeSegInput(image,manifest,lb));
        const auto contour=points(shape,seed);double left=64,right=0,top=64,bottom=0;
        for(const auto& point:contour) {left=std::min(left,point[0].get<double>());right=std::max(right,point[0].get<double>());
            top=std::min(top,point[1].get<double>());bottom=std::max(bottom,point[1].get<double>());}
        const int target=shape=="circle"?1:2;
        auto gt_box=torch::tensor({{
            float((lb.pad_x+left*lb.resized_width/image.cols)/manifest.input_width),
            float((lb.pad_y+top*lb.resized_height/image.rows)/manifest.input_height),
            float((lb.pad_x+right*lb.resized_width/image.cols)/manifest.input_width),
            float((lb.pad_y+bottom*lb.resized_height/image.rows)/manifest.input_height)}});
        auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
        auto owners=unified_experiment::assign(table,gt_box,{target});
        auto all_boxes=table.decoded_boxes.index({0});
        auto ious=unified_experiment::overlap(all_boxes,gt_box.expand({all_boxes.size(0),4}));
        auto centers=table.centers_xy.index({0});
        auto eligible=(centers.select(1,0)>gt_box.index({0,0}))&(centers.select(1,0)<gt_box.index({0,2}))&
            (centers.select(1,1)>gt_box.index({0,1}))&(centers.select(1,1)<gt_box.index({0,3}));
        J positive_by_level=J::array(),eligible_iou=J::array();int64_t offset=0;
        for(const auto& tensor:raw.class_logits){const int64_t count=tensor.size(2)*tensor.size(3);
            positive_by_level.push_back((owners.slice(0,offset,offset+count)>=0).sum().item<int64_t>());
            auto masked=ious.slice(0,offset,offset+count).masked_fill(eligible.slice(0,offset,offset+count).logical_not(),-1);
            auto top=std::get<0>(masked.topk(std::min<int64_t>(10,count)));
            auto valid=top>=0;eligible_iou.push_back({{"max",std::max(0.0,top.max().item<double>())},
                {"top10_mean",(top.clamp_min(0).sum()/valid.sum().clamp_min(1)).item<double>()},
                {"eligible",eligible.slice(0,offset,offset+count).sum().item<int64_t>()}});
            offset+=count;}
        J levels=J::array();
        for(std::size_t level=0;level<raw.class_logits.size();++level) {
            auto probabilities=raw.class_logits[level].sigmoid().squeeze(0).to(torch::kCPU).contiguous();
            const int64_t height=probabilities.size(1),width=probabilities.size(2),classes=probabilities.size(0);
            auto max_pair=probabilities.max(0);auto maxima=std::get<0>(max_pair).contiguous(),ids=std::get<1>(max_pair).contiguous();
            std::vector<float> scores;scores.reserve(height*width);
            std::vector<int64_t> argmax_all(classes,0),above(classes,0),inside(classes,0),outside(classes,0);
            double target_inside=0,target_outside=0,max_inside=0,max_outside=0;
            int64_t inside_count=0,outside_count=0;
            for(int64_t y=0;y<height;++y)for(int64_t x=0;x<width;++x) {
                const auto cls=ids.index({y,x}).item<int64_t>();
                const auto score=maxima.index({y,x}).item<float>();
                scores.push_back(score);++argmax_all.at(cls);if(score>=manifest.confidence_threshold)++above.at(cls);
                const double original_x=(((x+.5)*manifest.input_width/width)-lb.pad_x)*image.cols/lb.resized_width;
                const double original_y=(((y+.5)*manifest.input_height/height)-lb.pad_y)*image.rows/lb.resized_height;
                const bool in=original_x>=left&&original_x<=right&&original_y>=top&&original_y<=bottom;
                if(in) {++inside.at(cls);++inside_count;target_inside+=probabilities.index({target,y,x}).item<double>();max_inside+=score;}
                else {++outside.at(cls);++outside_count;target_outside+=probabilities.index({target,y,x}).item<double>();max_outside+=score;}
            }
            std::sort(scores.begin(),scores.end());
            auto percentile=[&](double f){return scores.at(std::min<std::size_t>(scores.size()-1,static_cast<std::size_t>(f*(scores.size()-1))));};
            levels.push_back({{"level",level},{"grid",{width,height}},{"anchor_count",height*width},
                {"argmax_all",argmax_all},{"argmax_above_threshold",above},
                {"argmax_inside_gt_bbox",inside},{"argmax_outside_gt_bbox",outside},
                {"max_score_p50",percentile(.50)},{"max_score_p95",percentile(.95)},{"max_score_p99",percentile(.99)},
                {"target_probability_inside_mean",target_inside/std::max<int64_t>(1,inside_count)},
                {"target_probability_outside_mean",target_outside/std::max<int64_t>(1,outside_count)},
                {"max_probability_inside_mean",max_inside/std::max<int64_t>(1,inside_count)},
                {"max_probability_outside_mean",max_outside/std::max<int64_t>(1,outside_count)}});
        }
        samples.push_back({{"seed",seed},{"split",seed<3?"Train":seed==3?"Valid":"VERIFY_SYNTHETIC_ORACLE"},
            {"gt_bbox_original",{left,top,right,bottom}},{"positive_by_level",positive_by_level},
            {"eligible_iou_by_level",eligible_iou},{"levels",levels}});
    }
    check(mode_budget::inventory(parent)==parent_before&&mode_budget::inventory(case_root)==case_before,"LOGITS_INPUT_CHANGED");
    auto out=fs::canonical(fresh_root(argv[7]));
    J report={{"schema","visionai.rd.unified_logits_diagnostic.v1"},{"shape",shape},
        {"candidate_sha256",mode_budget::hash(candidate)},{"parent_sha256",binding.at("checkpoint_sha256")},
        {"class_order",manifest.class_names},{"confidence_threshold",manifest.confidence_threshold},
        {"samples",samples},{"synthetic_only",true},{"production_allowed",false}};
    write_new(out/"logit_diagnostic.json",report.dump(2));
    std::cout<<"PASS logits diagnostic: "<<out<<std::endl;return 0;
}
}
