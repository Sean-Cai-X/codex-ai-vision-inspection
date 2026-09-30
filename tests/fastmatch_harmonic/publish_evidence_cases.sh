#!/bin/sh
# Publish only local audit assets. Never add the output directory to git.
set -eu
REPLAY=${1:?successful run_evidence_replay output required}
DEST=${2:?new external Evidence case directory required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$DEST" in /*) ;; *) exit 64;; esac
DEST=$(realpath -m -- "$DEST")
case "$DEST/" in "$ROOT/"*) exit 64;; esac
test ! -e "$DEST"
test "$(wc -l < "$REPLAY/case_results.txt")" -eq 6
mkdir -p "$DEST"
for name in closed_reference open_interface open_arc; do
 image=multiple;title="Closed Reference";geometry=polygon;facts=harmonic_audit_receipt.json
 case "$name" in
  open_interface) image=interface;title="Open Interface";geometry=line;facts=open_boundary_observation.json;;
  open_arc) image=ring;title="Open Arc";geometry=arc;facts=open_boundary_observation.json;;
 esac
 out="$DEST/$name";mkdir -p "$out"
 cp "$REPLAY/fixtures/$image.pgm" "$out/source_image.pgm"
 for file in run.cxsc evidence_overlay.png result_summary.json harmonic_audit_receipt.json; do
  cp "$REPLAY/$name/$file" "$out/$file"
 done
 if [ "$facts" != harmonic_audit_receipt.json ]; then cp "$REPLAY/$name/$facts" "$out/$facts";fi
 printf '{"schema":"cxvision.harmonic_fixture_label.v1","geometry_type":"%s","annotation_status":"PROPOSED_CONTROLLED_FIXTURE","human_accepted":false,"training_eligible":false,"annotations":[]}\n' "$geometry" > "$out/typed_label.json"
 printf '{"schema":"cxvision.evidence_case.v1","run_id":"harmonic_audit_v1","internal_case_id":"harmonic_audit_%s","review_item":"Harmonic Audit - %s","tool":"CxFastMatchHarmonicAudit","display_group":"CxFastMatchHarmonicAudit / Audit Cases","display_category":"To Verify","case_role":"audit_only_fixture","geometry_type":"%s","binding_status":"REFERENCE_ONLY","parameter_summary":"Audit-only; Key Parameter Controls; controlled fixture, not business approval","source_image":"source_image.pgm","typed_label":"typed_label.json","geometry_facts_ref":"%s","evidence_overlay":"evidence_overlay.png","result_summary":"result_summary.json","script_snapshot":"run.cxsc","required_assets":["source_image.pgm","typed_label.json","%s","evidence_overlay.png","result_summary.json","run.cxsc","harmonic_audit_receipt.json"]}\n' "$name" "$title" "$geometry" "$facts" "$facts" > "$out/case_manifest.json"
 (cd "$out" && sha256sum source_image.pgm typed_label.json "$facts" evidence_overlay.png result_summary.json run.cxsc case_manifest.json > asset_sha256.txt)
done
printf 'HARMONIC_EVIDENCE_PUBLISHED %s\n' "$DEST"
