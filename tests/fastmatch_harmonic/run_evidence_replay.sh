#!/bin/sh
set -eu
APP=${1:?application required}
MAKER=${2:?native topology fixture maker required}
OUT=${3:?fresh external output required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT" in /*) ;; *) exit 64;; esac
OUT=$(realpath -m -- "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
test ! -e "$OUT"
mkdir -p "$OUT"
cd "$ROOT"
"$MAKER" "$OUT/fixtures"
for name in closed_reference open_interface open_arc edited invalid_order ambiguous_anchor; do
 out="$OUT/$name";mkdir -p "$out"
 script="$name";image=multiple
 case "$name" in
  open_interface) image=interface;;
  open_arc) image=ring;;
  edited|invalid_order|ambiguous_anchor) script=closed_reference;;
 esac
 cp "tests/fastmatch_harmonic/evidence_$script.cxsc" "$out/run.cxsc"
 case "$name" in
  edited) printf '\n// harmonic_default global_harmonic_method 1\n// harmonic_default global_harmonic_sample_count 128\n// harmonic_default global_harmonic_residual_ppm 50000\n' >> "$out/run.cxsc";;
  invalid_order) printf '\n// harmonic_default global_harmonic_max_order 128\n' >> "$out/run.cxsc";;
  ambiguous_anchor) printf '\n// harmonic_default global_harmonic_anchor_y 75\n// harmonic_default global_harmonic_anchor_w 70\n// harmonic_default global_harmonic_anchor_h 10\n' >> "$out/run.cxsc";;
 esac
 rc=0
 "$APP" --cxscript-headless --image "$OUT/fixtures/$image.pgm" --script "$out/run.cxsc" --out "$out" \
  --roi-x0 0 --roi-y0 0 --roi-x1 320 --roi-y1 240 --max-steps 20000 \
  --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$out.jsonl" > "$out.log" 2>&1 || rc=$?
 case "$name" in
  invalid_order) test "$rc" -ne 0;grep -q 'Maximum harmonic order' "$out.log";;
  ambiguous_anchor) test "$rc" -ne 0;grep -q HARMONIC_ANCHOR_AMBIGUOUS_ARCS "$out.log";;
  *) test "$rc" -eq 0; test -s "$out/harmonic_audit_receipt.json";;
 esac
 printf '%s PASS\n' "$name" >> "$OUT/case_results.txt"
done
grep -q '"sample_count":128' "$OUT/edited/harmonic_audit_receipt.json"
grep -q '"method":1' "$OUT/edited/harmonic_audit_receipt.json"
grep -q '"maximum_pose_residual":0.05' "$OUT/edited/harmonic_audit_receipt.json"
grep -q '"closed":false' "$OUT/open_interface/open_boundary_observation.json"
grep -q '"parent_holes":1' "$OUT/open_arc/open_boundary_observation.json"
sha256sum "$APP" "$MAKER" "$OUT/"*/run.cxsc "$OUT/"*/harmonic_audit_receipt.json > "$OUT/evidence_replay_sha256.txt"
echo HEADLESS_HARMONIC_EVIDENCE_PASS
