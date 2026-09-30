"""C++ vs independent Python oracle; generated contours only, no business assets."""
import argparse
import dataclasses
import hashlib
import json
import subprocess
from pathlib import Path
from so2_prototype import Config, descriptor, match
from test_so2_prototype import shape, transform, angular_error

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--binary",type=Path,required=True)
    parser.add_argument("--report",type=Path)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[2]
    if args.report and (args.report.resolve()==repo or repo in args.report.resolve().parents):
        parser.error("report must remain outside checkout")
    binary=args.binary.resolve()
    maximum_coefficient_error=0.0
    maximum_pose_error=0.0
    count=0
    config=Config()
    def invoke(a,b,method):
        lines=[" ".join(str(int(v)) if isinstance(v,bool) else str(v)
                         for v in [0 if method=="dft" else 1,*dataclasses.astuple(config)])]
        for points in (a,b):
            lines.append("1 1 1 0 1 "+str(len(points)))
            lines.extend(f"{x:.17g} {y:.17g}" for x,y in points)
        result=subprocess.run([str(binary)],input="\n".join(lines)+"\n",
                              capture_output=True,text=True,timeout=30)
        if result.returncode:
            raise AssertionError((method,name,angle,scale,result.stdout+result.stderr))
        return json.loads(result.stdout)
    for method in ("dft","efd"):
        for name in ("asymmetric","rectangle","square","ellipse","triangle","circle"):
            points=shape(name)
            reference=descriptor(points,config,method=method,closed=True)
            for angle in range(0,360,15):
                for scale in (.6,1,1.7):
                    observed=transform(points,angle,scale)
                    observed=observed[13:]+observed[:13]
                    # Exercise winding normalization as well as cyclic start.
                    if angle%30==0:observed.reverse()
                    target=descriptor(observed,config,method=method,closed=True)
                    expected=match(reference,target)
                    actual=invoke(points,observed,method)
                    for key,want in (("reference",reference),("observation",target)):
                        got=actual[key]
                        error=max(abs(complex(*v)-w) for v,w in zip(got["coefficients"],want["coefficients"]))
                        maximum_coefficient_error=max(maximum_coefficient_error,error)
                        assert len(got["coefficients"])==len(want["coefficients"])
                        assert error<1e-9,(method,name,angle,error)
                        assert abs(got["scale"]-want["scale"])<1e-8
                        assert abs(got["perimeter"]-want["perimeter"])<1e-8
                        assert abs(complex(*got["centroid"])-want["centroid"])<1e-8
                    assert actual["status"]==expected["status"],(method,name,angle,actual,expected)
                    assert actual["symmetry_order"]==expected["symmetry_order"]
                    assert abs(actual["distance"]-expected["invariant_distance"])<1e-9
                    assert len(actual["poses"])==len(expected["pose_hypotheses"])
                    for p in actual["poses"]:
                        q=min(expected["pose_hypotheses"],key=lambda q:angular_error(p["angle_deg"],q["angle_deg"]))
                        error=angular_error(p["angle_deg"],q["angle_deg"])
                        maximum_pose_error=max(maximum_pose_error,error)
                        assert error<.001,(method,name,angle,error)
                        assert abs(p["scale"]-q["scale"])<1e-8
                        assert abs(p["correlation"]-q["correlation"])<1e-10
                        assert abs(p["residual"]-q["residual"])<1e-6
                    count+=1
        points=shape("asymmetric")
        for observed in ([(-x,y) for x,y in points],shape("ellipse")):
            assert invoke(points,observed,method)["status"]=="POSE_RESIDUAL_REJECTED"
            count+=1
    report={"schema":"cxvision.so2_native_parity.v1","status":"PASS",
            "scenario_count":count,"max_coefficient_error":maximum_coefficient_error,
            "max_pose_difference_deg":maximum_pose_error,
            "binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
            "source_sha256":{p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in
                 ["cxgeom/include/CxGeoSO2Harmonic.h","cxgeom/src/CxGeoSO2Harmonic.cpp",
                  "tests/fastmatch_harmonic/so2_prototype.py","tests/fastmatch_harmonic/run_native_parity.py"]},
            "runtime_integrated":False,"production_eligible":False}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))
if __name__=="__main__":
    main()
