"""Pre-register and audit one frozen-checkpoint synthetic validation; never trains."""
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import random
import sys
from verify_factor_sweep import read_csv, require, stats

SEED = 2026093001
FIELDS = ["id", "q", "angle_deg", "scale", "cx", "cy", "intensity"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixtures():
    rng = random.Random(SEED)
    rows = []
    for q in (1, 2, 4, 0):
        period = 360/q if q else 360
        previous = ([i*period/4 for i in range(4)] +
                    [(i+.5)*period/12 for i in range(12)] +
                    [(i+.25)*period/12 for i in range(12)] +
                    [i*period/72 for i in range(72)]) if q else []
        for i in range(120 if q else 12):
            while True:
                angle = (i+rng.uniform(.1, .9))*period/120 if q else 0
                if not q or all(abs(math.remainder(angle-a, period)) > 1e-4 for a in previous):
                    break
            rows.append(dict(id="locked_"+str(q)+"_"+str(i), q=q, angle_deg=angle,
                             scale=rng.uniform(.91, 1.09), cx=rng.uniform(-.7, .7),
                             cy=rng.uniform(-.7, .7), intensity=rng.uniform(.82, .98)))
    return rows


def manifest_text():
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(buffer, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    writer.writerows(fixtures())
    return buffer.getvalue()


def source_hashes():
    repo = Path(__file__).resolve().parents[2]
    files = sorted((repo/"libtorch_module").glob("torch_cyclic_*"))
    files += [repo/"tests/cyclic_pose/factor_sweep.cpp", Path(__file__).resolve(),
              repo/"tests/cyclic_pose/verify_factor_sweep.py"]
    return {str(p.relative_to(repo)): sha(p) for p in files if p.is_file()}


def prepare(source, locked):
    source, locked = Path(source), Path(locked)
    require(not locked.exists(), "refuse existing registration")
    receipt = json.loads((source/"training_receipt.json").read_text())
    require(receipt.get("augmentation") == "joint" and receipt["archive_exact"], "expected frozen joint candidate")
    expected = json.loads((source.parent/"step_receipt.json").read_text())["candidate_checkpoint_sha256"]
    require(sha(source/"experimental_weights.pt") == expected, "candidate hash mismatch")
    locked.mkdir(parents=True)
    (locked/"manifest.csv").write_text(manifest_text())
    protocol = {
        "schema": "cxvision.locked_pose_protocol.v1", "seed": SEED,
        "checkpoint_sha256": expected, "source_sha256": source_hashes(),
        "manifest_sha256": sha(locked/"manifest.csv"),
        "rows": 372, "oriented_per_q": 120, "circle_count": 12,
        "scale_range": [.91, 1.09], "intensity_range": [.82, .98],
        "position_range_px": [-.7, .7], "previous_angle_exclusion_deg": 1e-4,
        "gates": {"mae_deg_max": 1, "p95_deg_max": 2, "invalid_max": 0,
                  "apply_to": ["pooled", "q1", "q2", "q4"]},
        "scope": "CONTROLLED_SYNTHETIC_LOCKED_VALIDATION", "production_eligible": False,
        "unknown_shapes": False, "gt_masks": True, "known_symmetry": True,
    }
    (locked/"protocol.json").write_text(json.dumps(protocol, indent=2))
    return protocol


def verify_lock(source, locked):
    source, locked = Path(source), Path(locked)
    p = json.loads((locked/"protocol.json").read_text())
    require(p["schema"] == "cxvision.locked_pose_protocol.v1" and p["seed"] == SEED, "protocol mismatch")
    require(p["gates"] == {"mae_deg_max": 1, "p95_deg_max": 2, "invalid_max": 0,
                           "apply_to": ["pooled", "q1", "q2", "q4"]}, "modified gates")
    require(sha(source/"experimental_weights.pt") == p["checkpoint_sha256"], "checkpoint changed")
    require(source_hashes() == p["source_sha256"], "evaluator or model source changed")
    require(sha(locked/"manifest.csv") == p["manifest_sha256"], "manifest changed")
    require((locked/"manifest.csv").read_text() == manifest_text(), "fixture generation mismatch")
    return p


def audit(source, locked, output):
    p = verify_lock(source, locked)
    rows = read_csv(Path(output)/"predictions.csv")
    truth = {r["id"]: r for r in read_csv(Path(locked)/"manifest.csv")}
    require(len(rows) == 372 and len({r["id"] for r in rows}) == 372, "missing or duplicate predictions")
    decoded = []
    for row in rows:
        target = truth[row["id"]]
        require(row["scenario"] == "locked_holdout" and int(row["q"]) == int(target["q"]), "identity mismatch")
        for actual, expected in [("target_deg", "angle_deg"), ("scale", "scale"),
                                 ("cx", "cx"), ("cy", "cy"), ("intensity", "intensity")]:
            require(abs(float(row[actual])-float(target[expected])) < 1e-10, "fixture mismatch")
        require(row["valid"] in ("0", "1"), "invalid validity")
        q, valid = int(row["q"]), row["valid"] == "1"
        pred = float(row["prediction_deg"])
        require(math.isfinite(pred), "nonfinite angle")
        error = abs(math.remainder(pred-float(target["angle_deg"]), 360/q)) if q else None
        if q and valid:
            require(abs(error-float(row["error_deg"])) < 1e-6, "wrong angular error")
        else:
            require(row["error_deg"] == "NA", "invalid angle error must be NA")
        decoded.append(dict(q=q, valid=valid, error=error))
    metrics = {}
    for label, q in [("pooled", None), ("q1", 1), ("q2", 2), ("q4", 4)]:
        subset = [r for r in decoded if r["q"] and (q is None or r["q"] == q)]
        require(len(subset) == (360 if q is None else 120), "wrong q count")
        invalid = sum(not r["valid"] for r in subset)
        m = stats([r["error"] for r in subset if r["valid"]])
        m["invalid_count"] = invalid
        m["pass"] = (invalid == 0 and m["mae_deg"] is not None and
                     m["mae_deg"] <= 1 and m["p95_deg"] <= 2)
        metrics[label] = m
    circles = [r for r in decoded if not r["q"]]
    circle_ok = len(circles) == 12 and all(not r["valid"] for r in circles)
    passed = circle_ok and all(m["pass"] for m in metrics.values())
    return {"schema": "cxvision.locked_pose_result.v1", "audit_consistency": "PASS",
            "scope": p["scope"], "controlled_synthetic_pass": passed,
            "production_eligible": False, "metrics": metrics, "circle_gate_pass": circle_ok,
            "checkpoint_sha256": p["checkpoint_sha256"], "manifest_sha256": p["manifest_sha256"],
            "predictions_sha256": sha(Path(output)/"predictions.csv")}


if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: locked_pose_validation.py prepare|verify SOURCE LOCKED or audit SOURCE LOCKED OUTPUT")
    mode = sys.argv[1]
    if mode == "prepare" and len(sys.argv) == 4:
        result = prepare(*sys.argv[2:])
    elif mode == "verify" and len(sys.argv) == 4:
        result = verify_lock(*sys.argv[2:])
    elif mode == "audit" and len(sys.argv) == 5:
        result = audit(*sys.argv[2:])
    else:
        raise SystemExit("invalid mode/arguments")
    print(json.dumps(result, indent=2))
    if mode == "audit" and not result["controlled_synthetic_pass"]:
        raise SystemExit(2)
