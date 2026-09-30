#!/bin/sh
set -eu
APP=${1:?application required}
MAKER=${2:?native fixture maker required}
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
 for name in open_unverified open_declared ring_fallback ring_override multi_default multi_select0 multi_select1 multi_cross multi_stale index_oob invalid_topology; do
  out="$OUT/m${method}_$name";mkdir -p "$out"
  cp tests/fastmatch_harmonic/headless_image_baseline.cxsc "$out/run.cxsc"
  printf 'HarmonicAudit m_audit;\nm_audit.parameter(%s,"method");\n' "$method" >> "$out/run.cxsc"
  image=multiple;error="";status=AUDIT_POSE_HYPOTHESES;poses=2
  case "$name" in
   open_unverified) image=interface;status=UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK;poses=0;;
   open_declared) image=interface;status=OPEN_CONTOUR_LEGACY_FALLBACK;poses=0;;
   ring_fallback) image=ring;status=UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK;poses=0;;
   ring_override) image=ring;error=HARMONIC_TOPOLOGY_CONTRADICTS_MEASUREMENT;;
   multi_default|multi_stale) error=HARMONIC_AMBIGUOUS_SOURCE_SELECTION;;
   multi_cross) status=POSE_RESIDUAL_REJECTED;poses=0;;
   index_oob) error=HARMONIC_MISSING_MEASUREMENT;;
   invalid_topology) image=interface;error=HARMONIC_INVALID_TOPOLOGY;;
  esac
  for side in 0 1; do
   printf 'm_audit.select(%s);\n' "$side" >> "$out/run.cxsc"
   case "$name" in
    multi_select0) printf 'm_audit.sourceindex(0);\n' >> "$out/run.cxsc";;
    multi_select1) printf 'm_audit.sourceindex(1);\n' >> "$out/run.cxsc";;
    multi_cross) printf 'm_audit.sourceindex(%s);\n' "$side" >> "$out/run.cxsc";;
    multi_stale) if [ "$side" -eq 0 ]; then printf 'm_audit.sourceindex(1);\n' >> "$out/run.cxsc";fi;;
    index_oob) printf 'm_audit.sourceindex(99);\n' >> "$out/run.cxsc";;
   esac
   printf 'm_audit.fromobject(m_object);\n' >> "$out/run.cxsc"
   case "$name" in
    open_unverified) ;;
    open_declared) printf 'm_audit.topology(1,0,0,0,1);\n' >> "$out/run.cxsc";;
    ring_fallback) printf 'm_audit.topology(1,1,1,1,1);\n' >> "$out/run.cxsc";;
    invalid_topology) printf 'm_audit.topology(2,1,1,0,1);\n' >> "$out/run.cxsc";;
    *) printf 'm_audit.topology(1,1,1,0,1);\n' >> "$out/run.cxsc";;
   esac
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
test "$count" -eq 22
# Confirm selection and holes came from measured image evidence.
grep -q '"measured_holes":1' "$OUT/m0_ring_fallback/harmonic_audit_receipt.json"
grep -q '"measured_source_count":2' "$OUT/m0_multi_select1/harmonic_audit_receipt.json"
grep -q '"selected_source_index":1' "$OUT/m0_multi_select1/harmonic_audit_receipt.json"
sha256sum "$APP" "$MAKER" "$ROOT/tests/fastmatch_harmonic/run_topology_replay.sh" \
 "$OUT/fixtures/"*.pgm "$OUT/"*/run.cxsc "$OUT/"*/harmonic_audit_receipt.json > "$OUT/topology_sha256.txt"
printf '%s\n' '{"schema":"cxvision.so2_topology_replay.v1","status":"PASS","cases":22,"measured_holes_preserved":true,"explicit_one_shot_selection":true,"open_boundary_matching_supported":false,"production_eligible":false}' > "$OUT/suite_receipt.json"
echo HEADLESS_TOPOLOGY_GUARDS_PASS
