"""Verify equal-budget augmentation ablations; these are development results only."""
import csv
import hashlib
import json
import math
from pathlib import Path
import sys
from verify_synthetic_angles import audit as angle_audit
from verify_factor_sweep import audit as factor_audit, require, read_csv


def audit(root, previous):
    root, previous = Path(root), Path(previous)
    modes = ["baseline", "scale", "intensity", "joint"]
    results = {}
    reference = None
    for mode in modes:
        run = root/mode
        p = json.loads((run/"protocol.json").read_text())
        require(p["augmentation"] == mode and p["evaluation_role"] == "DEVELOPMENT", "wrong arm/role")
        contract = {k: p[k] for k in ("seed", "train_seed", "holdout_seed", "holdout_angle_offset",
                    "steps", "lr", "K", "channels", "target", "rho", "batch_size", "overfit", "holdout")}
        if reference is None:
            reference = contract
        require(contract == reference and p["steps"] == 240 and p["batch_size"] == 14, "unequal budget")
        for name in ("train.csv", "holdout.csv"):
            require((run/name).read_bytes() == (root/"baseline"/name).read_bytes(), "different canonical fixtures")
        rows = read_csv(run/"augmentation.csv")
        identities = [r["id"] for r in read_csv(run/"train.csv")]
        require(len(rows) == 240*14, "wrong augmentation row count")
        for index, row in enumerate(rows):
            step, i = divmod(index, 14)
            require(int(row["step"]) == step and row["id"] == identities[i], "schedule order mismatch")
            scale = .9+.1*((step+i)%3) if mode in ("scale", "joint") else 1
            intensity = .8+.1*((step//3+i)%3) if mode in ("intensity", "joint") else 1
            require(abs(float(row["scale"])-scale) < 1e-12 and
                    abs(float(row["intensity"])-intensity) < 1e-12, "augmentation schedule mismatch")
        curve = read_csv(run/"training_curve.csv")
        require(len(curve) == 240 and all(int(r["step"]) == i and math.isfinite(float(r["kl"]))
                                        for i,r in enumerate(curve)), "invalid learning curve")
        angles = angle_audit(run)
        factors = factor_audit(run, root/(mode+"_factors"))
        receipt = json.loads((run/"training_receipt.json").read_text())
        require(receipt["archive_exact"], "checkpoint reload differs")
        results[mode] = {
            "initial_kl": receipt["initial_kl"], "final_kl": receipt["final_kl"],
            "canonical_train": receipt["train"],
            "development_combined": receipt["holdout"],
            "development_reference_pass": receipt["holdout_pass"],
            "small_sample_overfit_pass": receipt["overfit_pass"],
            "factor_summaries": factors["summaries"],
            "angle_audit_consistency": angles["audit_consistency"],
            "checkpoint_sha256": factors["checkpoint_sha256"],
            "augmentation_sha256": hashlib.sha256((run/"augmentation.csv").read_bytes()).hexdigest(),
        }
    initial = results["baseline"]["initial_kl"]
    require(all(r["initial_kl"] == initial for r in results.values()), "initial model differs")
    for name in ("angle_predictions.csv", "training_curve.csv", "train.csv", "holdout.csv"):
        require((root/"baseline"/name).read_bytes() == (previous/name).read_bytes(), "baseline regression: "+name)
    return {
        "schema": "cxvision.augmentation_ablation.v1", "scope": "DEVELOPMENT_ABLATION",
        "audit_consistency": "PASS", "equal_budget": True, "previous_baseline_exact": True,
        "independent_acceptance": "NOT_EVALUATED", "production_eligible": False,
        "arms": results,
    }


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify_augmentation_ablation.py ABLATION_ROOT PREVIOUS_V2_RUN")
    print(json.dumps(audit(sys.argv[1], sys.argv[2]), indent=2))
