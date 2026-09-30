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
 for name in left right corner interface ring whole arcs noanchor force_close force_complete closed_asset point_edit cleared stale; do
  out="$OUT/m${method}_$name";mkdir -p "$out"
  cp tests/fastmatch_harmonic/headless_image_baseline.cxsc "$out/run.cxsc"
  printf 'HarmonicAudit m_audit;\nm_audit.parameter(%s,"method");\n' "$method" >> "$out/run.cxsc"
  image=multiple;anchor="35,55,15,20";error=""
  case "$name" in
   right) anchor="270,125,15,20";;
   corner) anchor="35,45,15,15";;
   interface) image=interface;anchor="150,100,20,20";;
   ring) image=ring;anchor="210,105,20,30";;
   whole) anchor="35,45,70,90";error=SUBCURVE_WHOLE_CLOSED_CONTOUR;;
   arcs) anchor="35,75,70,10";error=HARMONIC_ANCHOR_AMBIGUOUS_ARCS;;
   noanchor|stale) error=OPEN_SUBCURVE_ANCHOR_REQUIRED;;
   force_close|force_complete) error=OPEN_SUBCURVE_CANNOT_CLOSE;;
   closed_asset) error=OPEN_SUBCURVE_NOT_CLOSED_DESCRIPTOR;;
   point_edit) error=OPEN_SUBCURVE_EDIT_REQUIRES_CLEAR;;
   cleared) error=OPEN_OBSERVATION_NOT_READY;;
  esac
  if [ "$name" != noanchor ]; then printf 'm_audit.anchorrect(%s);\n' "$anchor" >> "$out/run.cxsc";fi
  printf 'm_audit.fromobjectarc(m_object);\nm_audit.expectsubcurve(m_object);\n' >> "$out/run.cxsc"
  case "$name" in
   force_close) printf 'm_audit.topology(1,1,1,0,1);\n' >> "$out/run.cxsc";;
   force_complete) printf 'm_audit.topology(1,0,1,0,1);\n' >> "$out/run.cxsc";;
   closed_asset) printf 'm_audit.saveasset(global_harmonic_asset_path);\n' >> "$out/run.cxsc";;
   point_edit) printf 'm_audit.point(40,55);\n' >> "$out/run.cxsc";;
   cleared) printf 'm_audit.clear();\n' >> "$out/run.cxsc";;
   stale) printf 'm_audit.fromobjectarc(m_object);\n' >> "$out/run.cxsc";;
  esac
  printf 'm_audit.saveopen(global_open_boundary_receipt_path);\nm_audit.run();\nm_audit.expectstatus("OPEN_CONTOUR_LEGACY_FALLBACK");\nm_audit.expectcount(0);\nm_audit.save(global_harmonic_receipt_path);\n' >> "$out/run.cxsc"
  rc=0
  "$APP" --cxscript-headless --image "$OUT/fixtures/$image.pgm" --script "$out/run.cxsc" --out "$out" \
   --roi-x0 0 --roi-y0 0 --roi-x1 320 --roi-y1 240 --max-steps 20000 \
   --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$out.jsonl" > "$out.log" 2>&1 || rc=$?
  if [ -n "$error" ]; then
   test "$rc" -ne 0
   grep -q "$error" "$out.log"
  else
   test "$rc" -eq 0
   grep -q '"closed":false' "$out/open_boundary_observation.json"
   grep -q '"order":"parent_cyclic_forward"' "$out/open_boundary_observation.json"
   grep -q '"assertions_passed":3' "$out/harmonic_audit_receipt.json"
  fi
  test ! -e "$out/harmonic_reference.so2"
  printf '%s %s PASS\n' "$method" "$name" >> "$OUT/case_results.txt"
  count=$((count+1))
 done
done
test "$count" -eq 28
grep -q '"parent_holes":1' "$OUT/m0_ring/open_boundary_observation.json"
sha256sum "$APP" "$MAKER" "$ROOT/tests/fastmatch_harmonic/run_open_boundary_replay.sh" \
 "$OUT/fixtures/"*.pgm "$OUT/"*/run.cxsc "$OUT/"*/open_boundary_observation.json \
 "$OUT/"*/harmonic_audit_receipt.json > "$OUT/open_boundary_sha256.txt"
printf '%s\n' '{"schema":"cxvision.open_boundary_replay.v1","status":"PASS","cases":28,"parent_order_checked":true,"forced_closure_rejected":true,"closed_descriptor_export_rejected":true,"open_curve_matching":false,"production_eligible":false}' > "$OUT/suite_receipt.json"
echo HEADLESS_OPEN_BOUNDARY_PASS
