"""Independently audit synthetic pose angles; never trains or changes model/data."""
import csv
import hashlib
import json
import math
from pathlib import Path
import sys


def metrics(rows):
    errors = [r["error"] for r in rows if r["q"] and r["valid"]]
    errors.sort()
    return {
        "mae_deg": sum(errors) / len(errors) if errors else 180,
        "p95_deg": errors[math.ceil(.95 * len(errors)) - 1] if errors else 180,
        "max_deg": max(errors) if errors else 180,
        "angle_count": sum(bool(r["q"]) for r in rows),
        "invalid_count": sum(bool(r["q"]) and not r["valid"] for r in rows),
        "circle_invalid_count": sum(not r["q"] and not r["valid"] for r in rows),
    }


def audit(directory):
    directory = Path(directory)
    protocol = json.loads((directory / "protocol.json").read_text())
    receipt = json.loads((directory / "training_receipt.json").read_text())
    manifests = {}
    for split in ("train", "holdout"):
        rows = list(csv.DictReader((directory / (split + ".csv")).open()))
        assert len(rows) == (14 if split == "train" else 38), "fixture count"
        manifests[split] = {r["id"]: r for r in rows}
        assert len(manifests[split]) == len(rows), "duplicate fixture id"
    for a in manifests["train"].values():
        for b in manifests["holdout"].values():
            q = int(a["q"])
            if q and q == int(b["q"]):
                assert abs(math.remainder(float(a["angle_deg"]) -
                                          float(b["angle_deg"]), 360 / q)) > 1e-6
    predictions = list(csv.DictReader((directory / "angle_predictions.csv").open()))
    assert len(predictions) == 52
    assert len({r["id"] for r in predictions}) == 52
    decoded = {"train": [], "holdout": []}
    oracle = {"train": [], "holdout": []}
    for r in predictions:
        split = "train" if r["id"] in manifests["train"] else "holdout"
        truth = manifests[split][r["id"]]
        q, target = int(truth["q"]), float(truth["angle_deg"])
        assert q == int(r["q"]) and abs(target - float(r["target_deg"])) < 1e-7
        assert r["valid"] in ("0", "1")
        valid, prediction = r["valid"] == "1", float(r["prediction_deg"])
        assert math.isfinite(prediction)
        error = abs(math.remainder(prediction - target, 360 / q)) if q else 0
        if q and valid:
            assert abs(error - float(r["error_deg"])) < 1e-6, "reported error mismatch"
        else:
            assert r["error_deg"] == "NA"
        decoded[split].append(dict(q=q, valid=valid, error=error))
        # Exact target distribution, without any model: exposes finite-bin phase bias.
        if q:
            angles = [2 * math.pi * g / protocol["K"] for g in range(protocol["K"])]
            weights = [math.exp(protocol["kappa"] * math.cos(q * (a - math.radians(target))))
                       for a in angles]
            c = sum(w * math.cos(q * a) for w, a in zip(weights, angles))
            s = sum(w * math.sin(q * a) for w, a in zip(weights, angles))
            ideal = math.degrees(math.atan2(s, c) / q)
            oracle[split].append(dict(q=q, valid=True,
                error=abs(math.remainder(ideal - target, 360 / q))))
    results = {}
    for split in decoded:
        actual = metrics(decoded[split])
        for key, value in actual.items():
            assert abs(value - receipt[split][key]) < 1e-6, "receipt mismatch: " + key
        results[split] = {
            "all": actual,
            "by_symmetry": {str(q): metrics([r for r in decoded[split] if r["q"] == q])
                            for q in (1, 2, 4)},
            "ideal_target_decode": {str(q): metrics([r for r in oracle[split] if r["q"] == q])
                                    for q in (1, 2, 4)},
        }
    tm, hm = results["train"]["all"], results["holdout"]["all"]
    overfit = (receipt["final_kl"] <= receipt["initial_kl"] * protocol["overfit"]["loss_ratio_max"]
               and tm["mae_deg"] <= protocol["overfit"]["mae_deg_max"]
               and tm["max_deg"] <= protocol["overfit"]["max_deg_max"]
               and tm["invalid_count"] == 0 and tm["circle_invalid_count"] == 2)
    holdout = (hm["mae_deg"] <= protocol["holdout"]["mae_deg_max"]
               and hm["p95_deg"] <= protocol["holdout"]["p95_deg_max"]
               and hm["invalid_count"] == 0 and hm["circle_invalid_count"] == 2)
    assert overfit == receipt["overfit_pass"] and holdout == receipt["holdout_pass"]
    return {
        "schema": "cxvision.independent_angle_audit.v1",
        "audit_consistency": "PASS", "overfit_pass": overfit, "holdout_pass": holdout,
        "production_eligible": False, "metrics": results,
        "artifact_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted(directory.iterdir()) if p.is_file()},
    }


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_synthetic_angles.py RUN_DIRECTORY")
    print(json.dumps(audit(sys.argv[1]), indent=2))
