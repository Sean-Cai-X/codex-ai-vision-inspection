#pragma once
namespace candidate_audit {
using namespace supervision_fixture;
inline double mask_iou(const cv::Mat& a,const cv::Mat& b){
    return double(cv::countNonZero(a&b))/std::max(1,cv::countNonZero(a|b));
}
inline J explain_probes(const J& all,const J& probes,int target){
    std::map<int64_t,int> classes;for(const auto& r:all)classes[r.at("anchor").get<int64_t>()]=r.at("class_id").get<int>();
    J evidence=J::array();std::set<std::string> causes;
    for(const auto& p:probes){
        if(!p.at("mask_valid_contour").get<bool>()||p.at("mask_iou").get<double>()<.5)continue;
        const bool correct=p.at("class_id").get<int>()==target;std::string cause;
        if(correct&&p.at("decision")=="unexamined_detection_cap")cause="QUALIFIED_TARGET_UNEXAMINED_AT_CAP";
        else if(correct&&p.at("decision")=="suppressed_box_nms"){
            auto parent=p.at("suppressor_anchor").get<int64_t>();check(classes.count(parent)==1,"PROVENANCE_SUPPRESSOR_MISSING");
            cause=classes.at(parent)==target?"QUALIFIED_TARGET_SUPPRESSED_BY_SAME_CLASS":"QUALIFIED_TARGET_SUPPRESSED_BY_WRONG_CLASS";
        }else if(!correct&&p.at("decision")=="selected")cause="SELECTED_GEOMETRY_MATCH_WRONG_CLASS";
        if(!cause.empty()){causes.insert(cause);evidence.push_back({{"cause",cause},{"anchor",p.at("anchor")},
            {"mask_iou",p.at("mask_iou")},{"suppressor_anchor",p.at("suppressor_anchor")}});}
    }
    return {{"observed_mechanisms",causes},{"positive_evidence",evidence},{"scope","bounded_above_threshold_mask_probes"},
        {"absence_proves_no_candidate",false},{"matching_mask_iou",.5}};
}
inline void same_candidates(const std::vector<SegCandidate>& a,const std::vector<SegCandidate>& b){
    check(a.size()==b.size(),"PROVENANCE_SELECTION_COUNT_CHANGED");
    for(std::size_t i=0;i<a.size();++i)check(a[i].source_anchor==b[i].source_anchor&&a[i].class_id==b[i].class_id&&a[i].score==b[i].score&&
        a[i].x0==b[i].x0&&a[i].y0==b[i].y0&&a[i].x1==b[i].x1&&a[i].y1==b[i].y1&&
        ((!a[i].coefficients.defined()&&!b[i].coefficients.defined())||torch::equal(a[i].coefficients,b[i].coefficients)),"PROVENANCE_SELECTION_CHANGED");
}
inline void self_test(){
    TorchModelManifest manifest;manifest.max_detections=2;manifest.iou_threshold=.45;manifest.class_agnostic_nms=true;
    auto c=[](int64_t id,int cls,float score,float x){SegCandidate r;r.source_anchor=id;r.class_id=cls;r.score=score;r.x0=x;r.x1=x+10;r.y0=0;r.y1=10;return r;};
    std::vector<SegCandidate> input={c(40,1,.9f,0),c(12,2,.8f,0),c(70,1,.7f,30),c(13,2,.6f,60)};
    SegNmsDiagnostics a,b;SegCandidateAudit audit;auto plain=SelectSegCandidates(input,manifest,a);
    auto traced=SelectSegCandidates(input,manifest,b,&audit);same_candidates(plain,traced);
    check(a.examined==b.examined&&a.suppressed==b.suppressed&&b.examined==3&&b.suppressed==1,"PROVENANCE_COUNTS");
    check(audit.ranked[1].decision=="suppressed_box_nms"&&audit.ranked[1].suppressor_anchor==40&&audit.ranked[1].suppressor_box_iou==1&&
        audit.ranked[2].decision=="selected"&&audit.ranked[3].decision=="unexamined_detection_cap","PROVENANCE_STAGES");
    manifest.class_agnostic_nms=false;auto aware=SelectSegCandidates(input,manifest,b,&audit);
    check(aware.size()==2&&aware[1].source_anchor==12&&audit.ranked[1].decision=="selected","PROVENANCE_CLASS_AWARE");
    SelectSegCandidates({},manifest,b,&audit);check(audit.ranked.empty(),"PROVENANCE_STALE_TRACE");
    // Different boxes can enclose the same small mask: box-NMS is not mask-NMS.
    auto one=c(1,1,.9f,0),two=c(2,1,.8f,5);manifest.class_agnostic_nms=true;
    auto retained=SelectSegCandidates({one,two},manifest,b,&audit);
    cv::Mat shared=cv::Mat::zeros(10,15,CV_8UC1);shared(cv::Rect(5,2,5,5)).setTo(255);
    check(retained.size()==2&&BoxIou(one,two)<.45&&mask_iou(shared,shared)==1,"PROVENANCE_MASK_BOX_DIVERGENCE");
    auto explained=explain_probes(J::array({{{"anchor",40},{"class_id",3}}}),J::array({
        {{"anchor",12},{"class_id",1},{"mask_valid_contour",true},{"mask_iou",.9},{"decision","suppressed_box_nms"},{"suppressor_anchor",40}},
        {{"anchor",13},{"class_id",1},{"mask_valid_contour",true},{"mask_iou",.8},{"decision","unexamined_detection_cap"},{"suppressor_anchor",-1}},
        {{"anchor",14},{"class_id",1},{"mask_valid_contour",false},{"mask_iou",.8},{"decision","unexamined_detection_cap"},{"suppressor_anchor",-1}}}),1);
    check(explained.at("positive_evidence").size()==2&&explained.at("observed_mechanisms").size()==2,"PROVENANCE_EXPLANATION_SCOPE");
    // Decoder source identity must match the flattened training table, including filtered anchors.
    manifest.num_classes=7;manifest.mask_channels=32;manifest.input_width=32;manifest.input_height=32;manifest.confidence_threshold=.25;
    manifest.max_detections=20;YoloV8Segment model(7);YoloV8SegRawOutput raw;
    for(int n:{4,2,1}){raw.class_logits.push_back(torch::full({1,7,n,n},-20.0));raw.box_logits.push_back(torch::zeros({1,64,n,n}));raw.mask_coefficients.push_back(torch::ones({1,32,n,n}));}
    raw.class_logits[0].index_put_({0,1,2,3},3.0);raw.class_logits[1].index_put_({0,2,1,0},2.0);raw.class_logits[2].index_put_({0,3,0,0},1.0);
    auto decoded=DecodeCandidates(raw,model->head(),manifest,&b,&audit);same_candidates(decoded,DecodeCandidates(raw,model->head(),manifest));
    check(audit.ranked.size()==3&&audit.ranked[0].candidate.source_anchor==11&&audit.ranked[1].candidate.source_anchor==18&&
        audit.ranked[2].candidate.source_anchor==20&&audit.ranked[0].candidate.source_row==2&&audit.ranked[0].candidate.source_column==3,"PROVENANCE_GRID_ID");
}
inline J inspect(YoloV8Segment& model,const TorchModelManifest& manifest,const cv::Mat& image,const cv::Mat& truth,int target){
    model->eval();torch::NoGradGuard guard;SegLetterbox lb;auto raw=model->forward(MakeSegInput(image,manifest,lb));
    SegNmsDiagnostics stats;SegCandidateAudit audit;auto selected=DecodeCandidates(raw,model->head(),manifest,&stats,&audit);
    same_candidates(selected,DecodeCandidates(raw,model->head(),manifest));
    auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
    auto probabilities=table.class_logits[0].sigmoid().to(torch::kCPU).contiguous();
    cv::Rect bounds=cv::boundingRect(truth);SegCandidate gt;gt.x0=float(lb.pad_x+bounds.x*double(lb.resized_width)/image.cols);
    gt.y0=float(lb.pad_y+bounds.y*double(lb.resized_height)/image.rows);gt.x1=float(lb.pad_x+(bounds.x+bounds.width)*double(lb.resized_width)/image.cols);
    gt.y1=float(lb.pad_y+(bounds.y+bounds.height)*double(lb.resized_height)/image.rows);
    // Oracle box is the raster-mask envelope, not a substituted training annotation.
    J stages=J::object(),all=J::array();std::vector<std::size_t> probes;
    std::vector<double> box_overlaps;std::size_t geometry_eligible=0;
    for(std::size_t i=0;i<audit.ranked.size();++i){const auto& r=audit.ranked[i];const auto& c=r.candidate;
        double overlap=BoxIou(c,gt);box_overlaps.push_back(overlap);if(overlap>=.5)++geometry_eligible;
        const auto key=r.decision+"/"+manifest.class_names[c.class_id];stages[key]=stages.value(key,0)+1;
        all.push_back({{"rank",i},{"anchor",c.source_anchor},{"level",c.source_level},{"row",c.source_row},{"column",c.source_column},
            {"class_id",c.class_id},{"score",c.score},{"decision",r.decision},{"suppressor_anchor",r.suppressor_anchor},
            {"suppressor_box_iou",r.suppressor_box_iou},{"raster_gt_box_iou",overlap}});
        if(r.decision=="selected")probes.push_back(i);
    }
    // Explicit mask-probe bound. Include every selected result first, then best box-IoU
    // candidates irrespective of their predicted class or NMS outcome. No GT use in inference.
    const std::size_t mask_budget=128;check(probes.size()<=mask_budget,"PROVENANCE_SELECTED_OVER_BUDGET");
    std::vector<std::size_t> order;for(std::size_t i=0;i<audit.ranked.size();++i)if(audit.ranked[i].decision!="selected")order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){return box_overlaps[a]>box_overlaps[b];});
    for(auto i:order){if(probes.size()==mask_budget)break;probes.push_back(i);}
    std::map<int64_t,cv::Mat> masks;J details=J::array();double best_pre_target=0,best_pre_any=0;
    std::string best_pre_target_stage="none";int64_t best_pre_target_anchor=-1;std::size_t geometric_probed=0;
    for(auto i:probes){const auto& r=audit.ranked[i];const auto& c=r.candidate;double q=0,s=0;
        auto mask=DecodeMask(c,raw.prototypes,manifest,lb,image.size(),q,s);bool valid=LargestContour(mask).size()>=3;
        masks[c.source_anchor]=mask;double iou=mask_iou(mask,truth);if(box_overlaps[i]>=.5)++geometric_probed;
        if(valid){best_pre_any=std::max(best_pre_any,iou);if(c.class_id==target&&iou>best_pre_target){best_pre_target=iou;best_pre_target_stage=r.decision;best_pre_target_anchor=c.source_anchor;}}
        J scores=J::array();for(int cls=0;cls<manifest.num_classes;++cls)scores.push_back(probabilities[c.source_anchor][cls].item<float>());
        auto d=all[i];d["class_probabilities"]=scores;d["target_probability"]=scores[target];d["winner_minus_target"]=c.score-scores[target].get<float>();
        d["mask_iou"]=iou;d["mask_valid_contour"]=valid;d["box_network_xyxy"]={c.x0,c.y0,c.x1,c.y1};
        d["box_original_xyxy_unclipped"]={(c.x0-lb.pad_x)*image.cols/lb.resized_width,(c.y0-lb.pad_y)*image.rows/lb.resized_height,
            (c.x1-lb.pad_x)*image.cols/lb.resized_width,(c.y1-lb.pad_y)*image.rows/lb.resized_height};details.push_back(d);
    }
    std::vector<coverage_budget::Prediction> predictions;J pairs=J::array();
    for(std::size_t i=0;i<selected.size();++i){const auto& c=selected[i];const auto& mask=masks.at(c.source_anchor);
        if(LargestContour(mask).size()>=3)predictions.push_back({"candidate-"+std::to_string(i),c.class_id,c.score,mask>0});
        for(std::size_t j=0;j<i;++j){const auto& other=masks.at(selected[j].source_anchor);auto overlap=mask_iou(mask,other);
            if(overlap>=.5&&LargestContour(mask).size()>=3&&LargestContour(other).size()>=3)pairs.push_back({
            {"anchor_a",selected[j].source_anchor},{"anchor_b",c.source_anchor},{"mask_iou",overlap},{"box_iou",BoxIou(selected[j],c)},
            {"class_a",selected[j].class_id},{"class_b",c.class_id}});}
    }
    auto metrics=cxvision::mask_error::AnalyzeSingleGroundTruth(image.size(),target,truth,predictions);
    check(pairs.size()==metrics.at("duplicate_candidate_pairs").get<std::size_t>(),"PROVENANCE_DUPLICATE_COUNT_MISMATCH");
    return {{"metrics",metrics},{"all_above_threshold_candidates",all},{"mask_probes",details},{"returned_overlap_pairs",pairs},
        {"explanation",explain_probes(all,details,target)},
        {"stage_class_counts",stages},{"anchors",stats.anchors},{"above_threshold",stats.above_threshold},{"examined",stats.examined},
        {"suppressed",stats.suppressed},{"unexamined",stats.above_threshold-stats.examined},{"cap_reached",stats.cap_reached},
        {"mask_probe_limit",mask_budget},{"mask_probe_count",probes.size()},{"all_above_threshold_masks_checked",probes.size()==audit.ranked.size()},
        {"box_iou_ge_05_count",geometry_eligible},{"box_iou_ge_05_probed",geometric_probed},{"best_probed_pre_nms_target_iou",best_pre_target},
        {"best_probed_pre_nms_any_iou",best_pre_any},{"best_probed_target_anchor",best_pre_target_anchor},{"best_probed_target_stage",best_pre_target_stage},
        {"below_threshold_masks_checked",false},{"selection_with_without_audit_equal",true}};
}
inline int run(int argc,char** argv){
    check(argc==8||argc==9,"usage: --candidate-audit binding parent_root covered_v5_root candidate_root transfer_root output_base [candidate_arm]");
    const std::string candidate_arm=argc==9?argv[8]:"unified_v11_geometry_assignment_experimental";
    check(candidate_arm=="unified_v11_geometry_assignment_experimental"||candidate_arm=="unified_v12_foreground_competition_experimental"||candidate_arm=="unified_v13_sigmoid_background_experimental","CANDIDATE_ARM_INVALID");
    auto binding=mode_budget::json(argv[2]);auto parent=fs::canonical(argv[3]),v5=fs::canonical(argv[4]),geo=fs::canonical(argv[5]),transfer=fs::canonical(argv[6]);
    auto pm=mode_budget::json(parent/"model_manifest.json");check(mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/pm.at("weights").get<std::string>())==binding.at("checkpoint_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256")&&pm.at("classes")==binding.at("geometry_contract"),"CANDIDATE_PARENT_BINDING");
    auto before_v5=mode_budget::inventory(v5),before_geo=mode_budget::inventory(geo),before_transfer=mode_budget::inventory(transfer);
    TorchModelManifest manifest;std::string reason;check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"CANDIDATE_MANIFEST");
    auto root=fs::canonical(fresh_root(argv[7]));std::cout<<"Candidate audit: "<<root<<std::endl;J summary=J::array();
    const auto start=std::chrono::steady_clock::now();
    for(const std::string shape:{"circle","ellipse"})for(bool geometry:{false,true}){
        const std::string arm=geometry?candidate_arm:"unified_v5_hard_negative_experimental";auto source=geometry?geo:v5;
        auto dir=source/(shape+"-"+arm+"-coverage-v1");auto receipt=mode_budget::json(dir/"result.json");auto weight_sha=mode_budget::hash(dir/"weights.pt");
        check(weight_sha==receipt.at("checkpoint_sha256")&&receipt.at("parent_sha256")==binding.at("checkpoint_sha256"),"CANDIDATE_RECEIPT");
        YoloV8Segment model(7);check(model->load_state_dict_strict((dir/"weights.pt").string()).complete(),"CANDIDATE_STRICT_LOAD");
        for(const auto& previous:receipt.at("observations")){
            check(std::chrono::steady_clock::now()-start<std::chrono::minutes(15),"CANDIDATE_AUDIT_BUDGET");
            const std::string id=previous.at("sample_id");bool valid=id=="nominal_valid_seed3";
            auto image_path=valid?source/shape/"training/sample-3.pgm":transfer/shape/(id+".pgm");
            auto gt_path=valid?source/(shape+"-augmentation/valid-oracle.png"):transfer/shape/(id+"-gt.pgm");
            check(mode_budget::hash(image_path)==previous.at("image_sha256")&&mode_budget::hash(gt_path)==previous.at("annotation_sha256"),"CANDIDATE_INPUT_SHA");
            auto image=cv::imread(image_path.string()),truth=cv::imread(gt_path.string(),cv::IMREAD_GRAYSCALE);check(!image.empty()&&!truth.empty(),"CANDIDATE_INPUT");
            auto detail=inspect(model,manifest,image,truth,shape=="circle"?1:2);
            check(detail.at("metrics")==previous.at("metrics"),"CANDIDATE_BASELINE_NOT_REPRODUCED");
            detail["shape"]=shape;detail["arm"]=arm;detail["sample_id"]=id;detail["split"]=previous.at("split");
            detail["image_sha256"]=previous.at("image_sha256");detail["annotation_sha256"]=previous.at("annotation_sha256");detail["checkpoint_sha256"]=weight_sha;
            auto name=shape+"-"+arm+"-"+id+".json";write_new(root/name,detail.dump(2));
            detail.erase("all_above_threshold_candidates");detail.erase("mask_probes");detail["detail_file"]=name;detail["detail_sha256"]=mode_budget::hash(root/name);summary.push_back(detail);
            std::cout<<shape<<' '<<arm<<' '<<id<<" post="<<detail.at("metrics").at("best_target_iou")<<" probed_pre="<<detail.at("best_probed_pre_nms_target_iou")<<std::endl;
        }
    }
    check(mode_budget::inventory(v5)==before_v5&&mode_budget::inventory(geo)==before_geo&&mode_budget::inventory(transfer)==before_transfer,"CANDIDATE_SOURCE_CHANGED");
    write_new(root/"candidate_audit.json",J({{"schema","visionai.rd.candidate_provenance_audit.v1"},{"observations",summary},
        {"covered_v5_comparison_sha256",mode_budget::hash(v5/"comparison.json")},{"candidate_comparison_sha256",mode_budget::hash(geo/"comparison.json")},{"candidate_arm",candidate_arm},
        {"parent_manifest_sha256",binding.at("parent_manifest_sha256")},{"class_order",manifest.class_names},
        {"probe_executable_sha256",mode_budget::hash(fs::canonical(argv[0]))},
        {"confidence",manifest.confidence_threshold},{"nms_iou",manifest.iou_threshold},{"mask_threshold",manifest.mask_threshold},
        {"max_detections",manifest.max_detections},{"class_agnostic_nms",manifest.class_agnostic_nms},
        {"execution_status","PASS"},{"optimizer_updates",0},{"independent_accuracy_evidence",false},{"model_quality_accepted",false},{"production_allowed",false}}).dump(2));
    std::cout<<"PASS candidate audit: "<<root<<std::endl;return 0;
}
}
