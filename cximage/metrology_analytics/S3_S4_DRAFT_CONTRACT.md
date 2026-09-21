# S3 → S4 Draft Contract

This is a draft bridge contract only. It is intentionally not wired into Parser, Suite, Manual UI or runtime promotion.

## Producer

`cximage/metrology_analytics` may produce value-only analytics:

- `CxSurfaceBasicStats`
- `CxSurfaceAreaResult`
- `CxRoughness1DResult`
- `CxPlaneCoeffs`
- `CxSurfaceFieldSnapshot`

## Consumer

Future S4 measurement semantic relation code may consume these values as observations.

## Forbidden Coupling

- Do not pass `CxSurfaceField*` or `Find*` pointers across the bridge.
- Do not let analytics code call Parser or modify runtime objects.
- Do not let S4 inference write back formal gauge parameters directly.

## Draft API Shape

```cpp
struct CxAnalyticsObservationBridgeDraft {
    using ObservationId = std::string;
    static bool TryQuerySurfaceBasicStats(ObservationId id, CxSurfaceBasicStats& out);
    static bool TryQueryAreaResult(ObservationId id, CxSurfaceAreaResult& out);
    static bool TryQueryRoughness1D(ObservationId id, CxRoughness1DResult& out);
};
```

S4 should treat missing analytics as `PENDING_ANALYTICS_BINDING`, not as a failed algorithm result.

## Current Draft Implementation

Implemented by:

- `CxAnalyticsObservationBridgeDraft.h`
- `CxAnalyticsObservationBridgeDraft.cpp`

Current draft observation ids:

- `draft.flat5`
- `draft.sine_A1_full_cycle`
- `draft.plane_tilt`

Current smoke-covered queries:

- `TryQuerySurfaceBasicStats("draft.flat5", ...)`
- `TryQueryAreaResult("draft.flat5", ...)`
- `TryQueryRoughness1D("draft.sine_A1_full_cycle", ...)`
- unknown observation id returns `false`

Current status:

```text
S3_11_DRAFT_ONLY_PENDING_S4_OWNER_REVIEW
```

This bridge is a value boundary for later semantic relation work. It must not be used as a production S4 binding until S4 ownership, evidence identifiers and promotion rules are reviewed separately.
