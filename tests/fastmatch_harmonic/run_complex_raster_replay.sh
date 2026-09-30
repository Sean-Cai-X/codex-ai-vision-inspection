#!/bin/sh
set -eu
APP=${1:?application required}
MAKER=${2:?native complex fixture maker required}
OUT=${3:?fresh external output required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT" in /*) ;; *) exit 64;; esac
OUT=$(realpath -m -- "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
test ! -e "$OUT"
mkdir -p "$OUT"
cd "$ROOT"
"$MAKER" "$OUT/fixtures"
run() {
 "$APP" --cxscript-headless --image "$1" --script "$2" --out "$3" \
 --roi-x0 0 --roi-y0 0 --roi-x1 400 --roi-y1 400 --max-steps 20000 \
 --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$3.jsonl" > "$3.log" 2>&1
}
failures=0
count=0
for method in 0 1; do
 for family in bevel notched wavy; do
  ref="$OUT/m${method}_${family}_reference"
  mkdir -p "$ref"
  sed -e "s/expectcount(2)/expectcount(1)/" \
      -e "s@// Closure.*@// Topology is declared from the controlled fixture, not automatically verified.@" \
      -e "/HarmonicAudit m_audit;/a m_audit.parameter($method,\"method\");" \
      tests/fastmatch_harmonic/headless_asset_roundtrip.cxsc > "$ref/run.cxsc"
  run "$OUT/fixtures/${family}_base.pgm" "$ref/run.cxsc" "$ref"
 done
 while read -r family name angle scale mode status; do
  ref="$OUT/m${method}_${family}_reference/harmonic_reference.so2"
  sha=$(sha256sum "$ref");sha=${sha%% *}
  out="$OUT/m${method}_$name"
  mkdir -p "$out"
  cp "$ref" "$out/harmonic_reference.so2"
  topology="1,1,1,0,1"
  if [ "$mode" = partial ]; then topology="1,1,0,0,1";fi
  sed -e "s/TRUSTED_ASSET_SHA/$sha/" \
      -e "/HarmonicAudit m_audit;/a m_audit.parameter($method,\"method\");" \
      -e "s/topology(1,1,1,0,1)/topology($topology)/" \
      -e '/m_audit.expectstatus/,$d' \
      tests/fastmatch_harmonic/headless_asset_reload.cxsc > "$out/run.cxsc"
  # Save raw outcome before assertions, so a failed quality gate remains analyzable.
  printf 'm_audit.save(global_harmonic_receipt_path);\nm_audit.expectstatus("%s");\n' "$status" >> "$out/run.cxsc"
  if [ "$status" = AUDIT_POSE_HYPOTHESES ]; then
   printf 'm_audit.expectcount(1);\nm_audit.expectposebounds(%s,%s,1.0,0.02);\n' "$angle" "$scale" >> "$out/run.cxsc"
  else
   printf 'm_audit.expectcount(0);\n' >> "$out/run.cxsc"
  fi
  printf 'm_audit.save(global_harmonic_receipt_path);\n' >> "$out/run.cxsc"
  if run "$OUT/fixtures/$name.pgm" "$out/run.cxsc" "$out"; then
   printf '%s %s PASS\n' "$method" "$name" >> "$OUT/case_results.txt"
  else
   printf '%s %s FAIL\n' "$method" "$name" >> "$OUT/case_results.txt"
   failures=$((failures+1))
  fi
  count=$((count+1))
 done < "$OUT/fixtures/complex.cases"
done
test "$count" -eq 48
sha256sum "$APP" "$MAKER" "$ROOT/tests/fastmatch_harmonic/run_complex_raster_replay.sh" \
 tests/fastmatch_harmonic/headless_asset_roundtrip.cxsc "$OUT/fixtures/complex.cases" \
 "$OUT/fixtures/"*.pgm "$OUT/"*/run.cxsc "$OUT/"*/harmonic_reference.so2 \
 "$OUT/"*/harmonic_audit_receipt.json > "$OUT/complex_sha256.txt"
status=PASS
if [ "$failures" -ne 0 ]; then status=FAIL;fi
printf '{"schema":"cxvision.so2_complex_raster_replay.v1","status":"%s","cases":48,"failures":%s,"methods":["DFT","EFD"],"angle_tolerance_deg":1,"scale_tolerance_absolute":0.02,"production_eligible":false}\n' "$status" "$failures" > "$OUT/suite_receipt.json"
test "$failures" -eq 0
echo HEADLESS_COMPLEX_RASTER_PASS
