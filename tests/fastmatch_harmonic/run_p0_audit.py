"""Reproducible P0 report. Writes generated artifacts outside the checkout."""
import argparse
import hashlib
import json
import math
import platform
import subprocess
import time
from pathlib import Path
from so2_prototype import Config, descriptor, distance, match
from test_so2_prototype import shape, transform, angular_error

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--case-root", type=Path, required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[2]
    out=args.out.resolve()
    if out == repo or repo in out.parents:
        parser.error("--out must be outside the source checkout")
    if not args.case_root.is_dir():
        parser.error("--case-root must be an existing case directory")
    out.mkdir(parents=True,exist_ok=True)
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()
    sources=["cximage/FastMatch.cpp","cximage/Grid.cpp",
             "cximage/CxFastMatchShapeModel.cpp","cximage/FindSegmentationTypes.h"]
    inventory={
        "schema":"cxvision.fastmatch_semantics_audit.v1",
        "source_commit":revision,
        "source_sha256":{p:sha(repo/p) for p in sources},
        "observation_scope":"source audit; no live FastMatch model instance queried",
        "grid_levels":[],
        "route_direction":"fine_to_coarse",
        "duplicate_semantics":"offline exact IMAGEFASTMODEL equality",
        "m_imagefastmatchlist_semantics":"observation grid feature vector",
        "points_shape_ordering":"directional scan concatenation; closure not guaranteed",
        "reference_closed_flag":"dense_points.size() >= 3; independent topology validation required",
        "reference_asset_role":"model_screening",
        "measurement_evidence":False,
        "legacy_behavior_changed":False}
    for level in [72,36,12,6,3]:
        directory=repo/"model"/f"{level}x{level}"
        inventory["grid_levels"].append({
            "grid_width":level,"grid_height":level,"harmonic_order":None,
            "runtime_model_count":None,
            "runtime_count_status":"NOT_OBSERVED",
            "legacy_directory_exists":directory.exists(),
            "legacy_pat_file_count":len(list(directory.glob("*.pat"))) if directory.exists() else None,
            "ordered_closed_status":"REQUIRES_PER_MODEL_VALIDATION"})
    write(out/"fastmatch_model_inventory.json",inventory)
    cases=[]
    for p in sorted(args.case_root.rglob("case_manifest.json")):
        manifest=json.loads(p.read_text(encoding="utf-8-sig"))
        label=p.parent/manifest.get("typed_label","")
        topology=manifest.get("topology","unknown")
        cases.append({
            "case_id":manifest.get("internal_case_id"),
            "case_name":manifest.get("review_item"),
            "case_manifest_sha256":sha(p),
            "topology":topology,
            "label_present":label.is_file(),
            "label_sha256":sha(label) if label.is_file() else None,
            "label_status":manifest.get("binding_status"),
            "source":manifest.get("tool"),
            "independent_of_formfit_seed":None,
            "seed_independence_status":"NOT_VERIFIED_FROM_MANIFEST",
            "admitted_as_ground_truth":False,
            "harmonic_status":("OPEN_CONTOUR_LEGACY_FALLBACK" if topology=="open"
                               else "UNREVIEWED_CLOSED_PROPOSAL"),
            "automatic_closure":False})
    write(out/"observation_contour_sources.json",{
        "schema":"cxvision.harmonic_observation_sources.v1",
        "cases":cases,
        "runtime_sources":[
            {"source":"FindSegmentationRegion.contour","eligible_after_validation":True,
             "requires":["instance ID","closed topology","hole/component metadata","source provenance"]},
            {"source":"FindObjectMeasurementSnapshot.outer_boundary","eligible_after_validation":True,
             "warning":"use pixel coordinates; normalized_boundary scales X and Y independently"},
            {"source":"BuildFastMatchObservedShapeModel","eligible_for_global_seed":False,
             "reason":"already depends on FastMatchTransform seed"}]})
    names=["asymmetric","rectangle","square","ellipse","triangle","circle"]
    config=Config()
    runs=[]
    for method in ["dft","efd"]:
        references={name:descriptor(shape(name),config,method=method,closed=True) for name in names}
        rows=[]
        for name in names:
            for angle in range(0,360,15):
                for scale in [0.6,1.0,1.7]:
                    points=transform(shape(name),angle,scale)
                    points=points[13:]+points[:13]
                    started=time.perf_counter()
                    observed=descriptor(points,config,method=method,closed=True)
                    ranking=sorted((distance(reference,observed),key)
                                   for key,reference in references.items())
                    result=match(references[name],observed)
                    elapsed=(time.perf_counter()-started)*1000
                    errors=[angular_error(h["angle_deg"],angle)
                            for h in result["pose_hypotheses"]]
                    rows.append({"case":name,"angle_deg":angle,"scale":scale,
                                 "correct_candidate_rank":next(i+1 for i,(_,k) in enumerate(ranking) if k==name),
                                 "invariant_distance":distance(references[name],observed),
                                 "pose_error_deg":min(errors) if errors else None,
                                 "status":result["status"],"elapsed_ms":elapsed})
        pose_errors=sorted(row["pose_error_deg"] for row in rows if row["pose_error_deg"] is not None)
        times=sorted(row["elapsed_ms"] for row in rows)
        runs.append({"method":method,"sample_count":config.sample_count,"max_order":config.max_order,
                     "scenario_count":len(rows),"reference_count":len(names),
                     "pose_observed_count":len(pose_errors),
                     "recall_at_1":sum(row["correct_candidate_rank"]==1 for row in rows)/len(rows),
                     "max_invariant_distance":max(row["invariant_distance"] for row in rows),
                     "pose_mae_deg":sum(pose_errors)/len(pose_errors),
                     "pose_p95_deg":pose_errors[math.ceil(.95*len(pose_errors))-1],
                     "orientation_unobservable_count":sum(row["status"]=="ORIENTATION_UNOBSERVABLE" for row in rows),
                     "prototype_p50_ms":times[len(times)//2],
                     "prototype_p95_ms":times[math.ceil(.95*len(times))-1],
                     "rows":rows})
    numerical_pass=all(r["recall_at_1"]==1 and r["pose_observed_count"]==360 and r["pose_mae_deg"]<.5 and
                       r["max_invariant_distance"]<.01 and
                       r["orientation_unobservable_count"]==72 for r in runs)
    summary={
        "schema":"cxvision.so2_p0_report.v1",
        "status":"P0_NUMERICAL_PASS" if numerical_pass else "P0_NUMERICAL_FAIL",
        "source_commit":revision,
        "prototype_sha256":sha(Path(__file__).with_name("so2_prototype.py")),
        "runner_sha256":sha(Path(__file__)),
        "specification_sha256":sha(out/"Next_20260920.source.txt") if (out/"Next_20260920.source.txt").is_file() else None,
        "runtime":{"python":platform.python_version(),"platform":platform.platform()},
        "dataset_kind":"procedurally generated controlled contours; not business-image accuracy",
        "measurement_evidence":False,"production_eligible":False,
        "reference_and_observation":"separate descriptors",
        "topology_policy":"open/partial/hole/multicomponent explicitly rejected in P0",
        "runtime_modes_implemented":[],
        "legacy_behavior_changed":False,
        "industrial_metrics_not_measured":[
            "image blur/low contrast segmentation reliability","occlusion recall",
            "actual FastMatch candidate Recall@K","FormFit residual and convergence",
            "end-to-end runtime benefit","wrong-edge rate"],
        "runs":runs}
    write(out/"so2_p0_report.json",summary)
    print(json.dumps({**{k:v for k,v in summary.items() if k!="runs"},
                      "runs":[{k:v for k,v in r.items() if k!="rows"} for r in runs]},
                     indent=2))
    return 0 if numerical_pass else 1

if __name__=="__main__":
    raise SystemExit(main())
