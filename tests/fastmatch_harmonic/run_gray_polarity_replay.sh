set -eu
root=$1
n=0
for dir in "$root"/case_*; do
 id=${dir##*/}
 source_case=${id%_r*}
 out/linux/cxvision_partial_headless_candidate --cxscript-headless --image "${root%/*}/gray_phase_study_20261013/$source_case/source.pgm" --script "$dir/run.cxsc" --out "$dir/headless" --unified-log "$dir/headless.jsonl" > "$dir/headless.log" 2>&1
 grep -q cxscript_headless_ok=true "$dir/headless.log"
 n=$((n+1))
done
echo "GRAY_POLARITY_HEADLESS_RUN_PASS $n"
