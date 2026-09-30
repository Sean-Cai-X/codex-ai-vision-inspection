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
count=0
for method in 0 1; do
 for name in left right fullleft components arcs interior background sparse invalidarea conflict_index conflict_anchor stale holeonly open_unverified invalidpoints; do
  out="$OUT/m${method}_$name";mkdir -p "$out"
  cp tests/fastmatch_harmonic/headless_image_baseline.cxsc "$out/run.cxsc"
  printf 'HarmonicAudit m_audit;\nm_audit.parameter(%s,"method");\n' "$method" >> "$out/run.cxsc"
  image=multiple;error="";status=AUDIT_POSE_HYPOTHESES;poses=2
  case "$name" in
   components) error=HARMONIC_ANCHOR_AMBIGUOUS_COMPONENTS;;
   arcs) error=HARMONIC_ANCHOR_AMBIGUOUS_ARCS;;
   interior|background) error=HARMONIC_ANCHOR_NO_BOUNDARY;;
   sparse) error=HARMONIC_ANCHOR_INSUFFICIENT_SUPPORT;;
   invalidarea) error=HARMONIC_INVALID_ANCHOR;;
   conflict_index|conflict_anchor) error=HARMONIC_SELECTION_CONFLICT;;
   stale) error=HARMONIC_AMBIGUOUS_SOURCE_SELECTION;;
   holeonly) image=ring;error=HARMONIC_ANCHOR_NO_BOUNDARY;;
   open_unverified) image=interface;status=UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK;poses=0;;
   invalidpoints) error=HARMONIC_INVALID_ANCHOR_POINTS;;
  esac
  for side in 0 1; do
   printf 'm_audit.select(%s);\n' "$side" >> "$out/run.cxsc"
   anchor="35,55,15,20"
   case "$name" in
    right) anchor="270,125,15,20";;
    fullleft) anchor="35,45,70,90";;
    components) anchor="0,0,320,240";;
    arcs) anchor="35,75,70,10";;
    interior) anchor="50,70,10,10";;
    background) anchor="120,20,10,10";;
    sparse) printf 'm_audit.anchorpoints(4096);\n' >> "$out/run.cxsc";;
    invalidarea) anchor="35,55,0,20";;
    conflict_index) printf 'm_audit.sourceindex(0);\n' >> "$out/run.cxsc";;
    holeonly) anchor="180,115,10,10";;
    open_unverified) anchor="150,100,20,20";;
    invalidpoints) printf 'm_audit.anchorpoints(0);\n' >> "$out/run.cxsc";;
   esac
   if [ "$name" != stale ] || [ "$side" -eq 0 ]; then
    printf 'm_audit.anchorrect(%s);\n' "$anchor" >> "$out/run.cxsc"
   fi
   if [ "$name" = conflict_anchor ]; then printf 'm_audit.sourceindex(0);\n' >> "$out/run.cxsc";fi
   printf 'm_audit.fromobject(m_object);\n' >> "$out/run.cxsc"
   if [ "$name" != open_unverified ]; then printf 'm_audit.topology(1,1,1,0,1);\n' >> "$out/run.cxsc";fi
  done
  printf 'm_audit.run();\nm_audit.save(global_harmonic_receipt_path);\nm_audit.expectstatus("%s");\nm_audit.expectcount(%s);\n' "$status" "$poses" >> "$out/run.cxsc"
  if [ "$poses" -eq 2 ]; then printf 'm_audit.expectposebounds(0,1,0.01,0.01);\n' >> "$out/run.cxsc";fi
  printf 'm_audit.save(global_harmonic_receipt_path);\n' >> "$out/run.cxsc"
  rc=0
  "$APP" --cxscript-headless --image "$OUT/fixtures/$image.pgm" --script "$out/run.cxsc" --out "$out" \
   --roi-x0 0 --roi-y0 0 --roi-x1 320 --roi-y1 240 --max-steps 20000 \
   --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$out.jsonl" > "$out.log" 2>&1 || rc=$?
  if [ -n "$error" ]; then
   test "$rc" -ne 0
   grep -q "$error" "$out.log"
  else
   test "$rc" -eq 0
   grep -q "\"status\":\"$status\"" "$out/harmonic_audit_receipt.json"
  fi
  printf '%s %s PASS\n' "$method" "$name" >> "$OUT/case_results.txt"
  count=$((count+1))
 done
done
test "$count" -eq 30
grep -q '"selected_source_index":0' "$OUT/m0_left/harmonic_audit_receipt.json"
grep -q '"selected_source_index":1' "$OUT/m0_right/harmonic_audit_receipt.json"
grep -q '"rect_half_open":\[35,55,15,20\]' "$OUT/m0_left/harmonic_audit_receipt.json"
grep -q '"basis":"outer_boundary_points"' "$OUT/m0_left/harmonic_audit_receipt.json"
sha256sum "$APP" "$MAKER" "$ROOT/tests/fastmatch_harmonic/run_anchor_replay.sh" \
 "$OUT/fixtures/"*.pgm "$OUT/"*/run.cxsc "$OUT/"*/harmonic_audit_receipt.json > "$OUT/anchor_sha256.txt"
printf '%s\n' '{"schema":"cxvision.so2_anchor_replay.v1","status":"PASS","cases":30,"selection_basis":"measured_outer_boundary_points","one_shot":true,"subcurve_extraction":false,"production_eligible":false}' > "$OUT/suite_receipt.json"
echo HEADLESS_ANCHOR_SELECTION_PASS
