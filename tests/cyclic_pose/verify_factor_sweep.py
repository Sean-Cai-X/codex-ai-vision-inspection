"""Audit frozen-model factor sweeps independently; output is diagnostic, not acceptance."""
import csv
import hashlib
import json
import math
from pathlib import Path
import sys


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read_csv(path):
    with path.open() as handle:
        return list(csv.DictReader(handle))


def stats(values):
    if not values:
        return {"count": 0, "mae_deg": None, "p95_deg": None, "max_deg": None}
    values = sorted(values)
    return {"count": len(values), "mae_deg": sum(values)/len(values),
            "p95_deg": values[math.ceil(.95*len(values))-1], "max_deg": values[-1]}


def audit(source, directory):
    source, directory = Path(source), Path(directory)
    protocol = json.loads((source/"protocol.json").read_text())
    require(protocol["schema"] == "cxvision.synthetic_pose_protocol.v2", "expected v2 checkpoint provenance")
    raw = read_csv(directory/"predictions.csv")
    require(len(raw) == 484, "unexpected total row count")
    scenarios = ["baseline", "scale_only", "intensity_only", "translation_only",
                 "x_only", "y_only", "combined", "angle_dense"]
    reference = {r["id"]: r for r in read_csv(source/"holdout.csv")}
    previous = {r["id"]: r for r in read_csv(source/"angle_predictions.csv") if r["id"] in reference}
    groups = {name: {} for name in scenarios}
    for r in raw:
        name, ident = r["scenario"], r["id"]
        require(name in groups and ident not in groups[name], "unknown scenario or duplicate ID")
        q, target = int(r["q"]), float(r["target_deg"])
        require(q in (0, 1, 2, 4) and r["valid"] in ("0", "1"), "invalid metadata")
        valid = r["valid"] == "1"
        prediction = float(r["prediction_deg"])
        require(math.isfinite(prediction), "nonfinite prediction")
        error = abs(math.remainder(prediction-target, 360/q)) if q else None
        if q and valid:
            require(abs(error-float(r["error_deg"])) < 1e-6, "incorrect angular error")
        else:
            require(r["error_deg"] == "NA", "invalid angle must not report an error")
        if name != "angle_dense":
            ref = reference[ident]
            require(q == int(ref["q"]) and abs(target-float(ref["angle_deg"])) < 1e-9, "unpaired angle")
            for field, keep, default in [
                ("scale", name in ("scale_only", "combined"), 1),
                ("intensity", name in ("intensity_only", "combined"), 1),
                ("cx", name in ("translation_only", "x_only", "combined"), 0),
                ("cy", name in ("translation_only", "y_only", "combined"), 0),
            ]:
                expected = float(ref[field]) if keep else default
                require(abs(float(r[field])-expected) < 1e-12, "confounded factor: "+name+"/"+field)
            if name == "combined":
                old = previous[ident]
                require(valid == (old["valid"] == "1"), "combined validity changed")
                if q:
                    require(abs(math.remainder(prediction-float(old["prediction_deg"]), 360/q)) < 1e-5,
                            "combined condition does not reproduce v2 result")
        else:
            require(float(r["scale"]) == 1 and float(r["intensity"]) == 1
                    and float(r["cx"]) == 0 and float(r["cy"]) == 0, "confounded angle sweep")
            if q:
                i = int(ident.rsplit("_", 1)[1])
                require(0 <= i < 72 and ident == "dense_"+str(q)+"_"+str(i)
                        and abs(target-i*(360/q)/72) < 1e-9, "invalid dense angle")
        groups[name][ident] = dict(q=q, target=target, prediction=prediction, valid=valid, error=error)
    summaries = {}
    for name, rows in groups.items():
        require(len(rows) == (218 if name == "angle_dense" else 38), "scenario count mismatch")
        if name != "angle_dense":
            require(set(rows) == set(reference), "scenario IDs mismatch")
        by_q = {}
        for q in (1, 2, 4):
            items = [r for r in rows.values() if r["q"] == q]
            require(len(items) == (72 if name == "angle_dense" else 12), "symmetry count mismatch")
            by_q[str(q)] = stats([r["error"] for r in items if r["valid"]])
        result = stats([r["error"] for r in rows.values() if r["q"] and r["valid"]])
        result.update(invalid_count=sum(bool(r["q"]) and not r["valid"] for r in rows.values()),
                      circle_invalid_count=sum(not r["q"] and not r["valid"] for r in rows.values()),
                      by_symmetry=by_q)
        if name != "angle_dense":
            paired = [(r, groups["baseline"][ident]) for ident, r in rows.items()
                      if r["q"] and r["valid"] and groups["baseline"][ident]["valid"]]
            result["paired_prediction_drift"] = stats([
                abs(math.remainder(r["prediction"]-b["prediction"], 360/r["q"])) for r, b in paired])
            result["mean_error_change_deg"] = sum(r["error"]-b["error"] for r,b in paired)/len(paired) if paired else None
        else:
            for label, train_grid in (("train_grid_angles", True), ("off_train_grid_angles", False)):
                items = [r for r in rows.values() if r["q"] and r["valid"] and
                         (abs(math.remainder(r["target"], 90/r["q"])) < 1e-7) == train_grid]
                result[label] = stats([r["error"] for r in items])
        summaries[name] = result
    return {"schema": "cxvision.factor_audit.v1", "scope": "DEVELOPMENT_DIAGNOSTIC",
            "audit_consistency": "PASS", "production_eligible": False,
            "acceptance": "NOT_EVALUATED", "combined_reproduces_v2": True,
            "summaries": summaries,
            "checkpoint_sha256": hashlib.sha256((source/"experimental_weights.pt").read_bytes()).hexdigest(),
            "predictions_sha256": hashlib.sha256((directory/"predictions.csv").read_bytes()).hexdigest()}


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify_factor_sweep.py V2_RUN_DIRECTORY SWEEP_DIRECTORY")
    print(json.dumps(audit(sys.argv[1], sys.argv[2]), indent=2))
