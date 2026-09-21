# HD Reference CxScript Cases

This folder contains CxScript front-door cases that mirror the local HD example projects under `D:/HD/projects` and the image folders under `D:/Codex-WorkDir/Sean_WorkDir/images`.

Current status: `reference_assets_only_model_unverified`.

These scripts are for chain setup:

1. Catalog visibility.
2. Evidence case selection.
3. Torch Runtime / Evidence panel observation.
4. Training Image Set rail population.
5. Headless smoke path preparation.

They do not prove model semantic quality and must not be promoted as `CONTRACT_PASS`.

| Script | HD project | Image set | Current gate |
|---|---|---|---|
| `hd_fruit_classification_reference_direct.cxsc` | `Example_Classification_Fruit.dltp` | `images/fruit/*` | classification adapter pending |
| `hd_juice_bottle_anomaly_reference_direct.cxsc` | `Example_AnomalyDet_JuiceBottle.dltp` + `best_model.hdl` | `images/juice_bottle/*` | anomaly runtime adapter pending |
| `hd_dongle_ocr_reference_direct.cxsc` | `Example_Deep_OCR_Dongle.dltp` | `images/dongle` | OCR label/runtime adapter pending |
| `hd_pill_semantic_segmentation_reference_direct.cxsc` | `Example_SemanticSegm_Pills.dltp` | `images/pill/*/*` | mask label binding pending |
| `hd_pill_bag_instance_segmentation_reference_direct.cxsc` | `Example_InstanceSegm_PillBags.dltp` | `images/pill_bag` | instance mask binding pending |
| `hd_pill_bag_detection_reference_direct.cxsc` | `Example_ObjDetection_PillBags.dltp` | `images/pill_bag` | detection box binding pending |
| `hd_screws_oriented_detection_reference_direct.cxsc` | `Example_ObjDetection_Oriented_Screws.dltp` | `images/screws` | oriented box binding pending |

## Manual preflight

1. Reload `cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc`.
2. Open Evidence Chain UI.
3. Confirm the seven `[REF] HD ...` scripts appear under `Torch / Model Validation`.
4. Double-click one case row.
5. Confirm the selected image is loaded into Image View.
6. Open `Torch Training Image Set`.
7. Confirm the selected evidence image is added to the training rail with a small `good/anomaly/unlabeled` badge.
8. Use `Add Manifest Images` only after the image set manifest has been reviewed.

## Acceptance language

Allowed:

- `ASSET_PREFLIGHT_PASS`
- `CATALOG_VISIBILITY_PASS`
- `TRAINING_IMAGE_SET_UI_READY`
- `PENDING_BINDING`

Forbidden:

- `MODEL_PASS`
- `SEMANTIC_PASS`
- `DETECTION_PASS`
- `SEGMENTATION_PASS`

