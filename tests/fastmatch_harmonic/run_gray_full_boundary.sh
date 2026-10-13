set -eu
root=$1
n=0
for case_dir in "$root"/case_*; do
 for dir in "$case_dir"/chunk_*; do
  out/linux/cxvision_partial_headless_candidate --cxscript-headless --image "$case_dir/source.pgm" --script "$dir/run.cxsc" --out "$dir/headless" --unified-log "$dir/headless.jsonl" > "$dir/headless.log" 2>&1
  grep -q cxscript_headless_ok=true "$dir/headless.log"
  n=$((n+1))
 done
done
echo "FULL_BOUNDARY_HEADLESS_RUN_PASS $n"
