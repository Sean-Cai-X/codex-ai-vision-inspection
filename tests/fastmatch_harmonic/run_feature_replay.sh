#!/bin/sh
# Real native application/cxscript execution; native C++ receipt verification.
set -eu
APP=${1:?application required}
MAKER=${2:?native fixture generator required}
CHECK=${3:?native receipt checker required}
OUT=${4:?fresh external output required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT" in /*) ;; *) exit 64;; esac
OUT=$(realpath -m -- "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
test ! -e "$OUT"
mkdir -p "$OUT"
cd "$ROOT"
"$MAKER" "$OUT/fixtures"
for name in off dft efd open stale toggle invalid; do
 mkdir -p "$OUT/$name"
 source=closed_reference;image=multiple
 if [ "$name" = open ];then source=open_interface;image=interface;fi
 cp "tests/fastmatch_harmonic/evidence_$source.cxsc" "$OUT/$name/run.cxsc"
 if [ "$name" != off ];then
  printf '\n// harmonic_default global_harmonic_debug_mode 1\n// harmonic_default global_harmonic_save_features 1\n' >> "$OUT/$name/run.cxsc"
 fi
 case "$name" in
  efd) printf '\n// harmonic_default global_harmonic_method 1\n' >> "$OUT/$name/run.cxsc";;
  stale) sed -i '/m_audit.save(global_harmonic_receipt_path);/i m_audit.topology(0,1,1,0,1);\nm_audit.run();' "$OUT/$name/run.cxsc";;
  toggle) sed -i '/m_audit.save(global_harmonic_receipt_path);/i m_audit.parameter(0,"debug_mode");\nm_audit.parameter(0,"save_intermediate_features");\nm_audit.run();' "$OUT/$name/run.cxsc";;
  invalid) printf '\n// harmonic_default global_harmonic_debug_mode 0\n' >> "$OUT/$name/run.cxsc";;
 esac
 rc=0
 "$APP" --cxscript-headless --image "$OUT/fixtures/$image.pgm" --script "$OUT/$name/run.cxsc" --out "$OUT/$name" --roi-x1 320 --roi-y1 240 --max-steps 20000 --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1 || rc=$?
 if [ "$name" = invalid ];then
  test "$rc" -ne 0
  grep -q 'requires debug mode' "$OUT/$name.log"
  test ! -e "$OUT/$name/harmonic_audit_receipt.json"
 else test "$rc" -eq 0;fi
 printf '%s PASS\n' "$name" >> "$OUT/cases.txt"
done
"$CHECK" "$OUT"
sha256sum "$APP" "$CHECK" "$OUT/"*/run.cxsc "$OUT/"*/harmonic_audit_receipt.json > "$OUT/sha256.txt"
echo HARMONIC_FEATURE_REPLAY_PASS
