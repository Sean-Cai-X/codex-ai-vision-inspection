#!/bin/sh
# Pixel -> FindObject -> persisted SO2 reference -> Audit. No Python dependency.
set -eu
APP=${1:?application required}
MAKER=${2:?native raster fixture generator required}
OUT=${3:?fresh external output directory required}
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
 --roi-x0 0 --roi-y0 0 --roi-x1 320 --roi-y1 240 --max-steps 20000 \
 --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$3.jsonl" > "$3.log" 2>&1
}
run "$OUT/fixtures/base.pgm" tests/fastmatch_harmonic/headless_asset_roundtrip.cxsc "$OUT/reference"
ASSET="$OUT/reference/harmonic_reference.so2"
SHA=$(sha256sum "$ASSET");SHA=${SHA%% *}
count=0
while read -r name angle scale mode status; do
 out="$OUT/$name"
 mkdir -p "$out"
 cp "$ASSET" "$out/harmonic_reference.so2"
 topology="1,1,1,0,1"
 case "$mode" in
  partial) topology="1,1,0,0,1";;
  unverified) topology="0,1,0,0,1";;
 esac
 # Stream the script once: sed -i metadata preservation fails on some NTFS mounts.
 sed -e "s/TRUSTED_ASSET_SHA/$SHA/" \
     -e "s/topology(1,1,1,0,1)/topology($topology)/" \
     -e '/m_audit.expectstatus/,$d' \
     tests/fastmatch_harmonic/headless_asset_reload.cxsc > "$out/run.cxsc"
 printf 'm_audit.expectstatus("%s");\n' "$status" >> "$out/run.cxsc"
 if [ "$status" = AUDIT_POSE_HYPOTHESES ]; then
  printf 'm_audit.expectcount(2);\nm_audit.expectposebounds(%s,%s,1.0,0.02);\n' "$angle" "$scale" >> "$out/run.cxsc"
 else
  printf 'm_audit.expectcount(0);\n' >> "$out/run.cxsc"
 fi
 printf 'm_audit.save(global_harmonic_receipt_path);\n' >> "$out/run.cxsc"
 run "$OUT/fixtures/$name.pgm" "$out/run.cxsc" "$out"
 grep -q "\"status\":\"$status\"" "$out/harmonic_audit_receipt.json"
 count=$((count+1))
done < "$OUT/fixtures/raster.cases"
test "$count" -eq 14
# A relaxed angle bound must NOT relax the independent scale bound.
mkdir -p "$OUT/wrong_scale"
cp "$ASSET" "$OUT/wrong_scale/harmonic_reference.so2"
sed 's/expectposebounds(0,1,1.0,0.02)/expectposebounds(0,1.5,1.0,0.02)/' \
 "$OUT/base/run.cxsc" > "$OUT/wrong_scale/run.cxsc"
if run "$OUT/fixtures/base.pgm" "$OUT/wrong_scale/run.cxsc" "$OUT/wrong_scale"; then
 echo "FAIL: scale tolerance was bypassed";exit 1
fi
grep -q HARMONIC_POSE_ASSERTION_FAILED "$OUT/wrong_scale.log"
mkdir -p "$OUT/wrong_angle"
cp "$ASSET" "$OUT/wrong_angle/harmonic_reference.so2"
sed 's/expectposebounds(0,1,1.0,0.02)/expectposebounds(3,1,1.0,0.02)/' \
 "$OUT/base/run.cxsc" > "$OUT/wrong_angle/run.cxsc"
if run "$OUT/fixtures/base.pgm" "$OUT/wrong_angle/run.cxsc" "$OUT/wrong_angle"; then
 echo "FAIL: angle tolerance was bypassed";exit 1
fi
grep -q HARMONIC_POSE_ASSERTION_FAILED "$OUT/wrong_angle.log"

sha256sum "$APP" "$MAKER" "$ROOT/tests/fastmatch_harmonic/run_raster_replay.sh" \
 tests/fastmatch_harmonic/headless_asset_roundtrip.cxsc "$OUT/fixtures/raster.cases" "$OUT/fixtures/"*.pgm "$ASSET" "$OUT/"*/run.cxsc \
 "$OUT/"*/harmonic_audit_receipt.json > "$OUT/raster_sha256.txt"
printf '%s\n' '{"schema":"cxvision.so2_raster_replay.v1","status":"PASS","raster_cases":14,"rotation_and_scale_separate":true,"angle_tolerance_deg":1,"scale_tolerance_absolute":0.02,"wrong_scale_rejected":true,"wrong_angle_rejected":true,"occlusion_provenance":"fixture_declared_not_auto_detected","production_eligible":false}' > "$OUT/suite_receipt.json"
echo HEADLESS_RASTER_AUDIT_PASS
