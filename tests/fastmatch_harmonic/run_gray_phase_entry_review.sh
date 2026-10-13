set -eu
root=$1
n=0
for dir in "$root"/case_*; do
 id=${dir##*/}
 source_case=${id%_r*}
 result=0
 out/linux/cxvision_partial_headless_candidate --cxscript-headless --image "${root%/*}/gray_phase_study_20261013/$source_case/source.pgm" --script "$dir/run.cxsc" --out "$dir/headless" --unified-log "$dir/headless.jsonl" > "$dir/headless.log" 2>&1 || result=$?
 if [ "$result" -eq 0 ]; then echo "UNEXPECTED_SUCCESS $id"; exit 1; fi
 if ! grep -q MASK_BOUNDARY_NO_ANCHORED_CONTOUR "$dir/headless.log"; then echo "UNEXPECTED_FAILURE $id $result"; exit 1; fi
 n=$((n+1))
done
echo "EXPECTED_REJECTION_HEADLESS_PASS $n"
