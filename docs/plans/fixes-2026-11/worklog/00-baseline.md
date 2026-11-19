# Wave 0 baseline — local verification (2026-11-19)

Machine: macOS x86_64 (Intel i9-9980HK, 8 cores / 16 threads, 16 GB RAM),
Apple command line tools only (no full Xcode), CMake 4.4.5 and Ninja from the
Python user site, shared `buildenv/` vcpkg tree. `main` at `2229c27`
(the plans PR #72 merged).

## Commands

```sh
export PATH="$HOME/Library/Python/3.9/bin:$HOME/Library/Python/3.9/lib/python/site-packages/cmake/data/bin:$PATH"
./build/mixxx-test --gtest_filter='QmlSkins/QmlStartupSmokeTest.Starts/NewUi'
./build/mixxx-test --gtest_filter='QmlSkins/QmlStartupSmokeTest.Starts/LateNightQML'
./build/mixxx-test --gtest_filter='QmlResponsiveBoundsTest*'
```

## Result

| Test | Result | Note |
|---|---|---|
| `QmlStartupSmokeTest.Starts/NewUi` | PASS alone | Segfaults when the two skin cases share one process (`Singleton class has already been created!`). Run each skin in its own process. |
| `QmlStartupSmokeTest.Starts/LateNightQML` | PASS alone | Same process-ordering constraint. |
| `QmlResponsiveBoundsTest.BandsStayInsideTheWindow` | PASS | Required the settle fix in this PR; failed on `main` before it. |

## What was wrong with the bounds test

The pre-check wait only required the item count to stay constant for 0.5 s.
On this slower machine the count can level off during a lull while the band
`Loader`s are still constructing. The first `checkSize()` then measured a
half-built tree (1318 items instead of ~3000) and reported `PreviewDeckBar` as
missing. The same size passed once the tree had grown.

The fix waits up to 10 s for `PreviewDeckBar` before the first measurement. The
bar stays instantiated while hidden (`active: true`), so the wait does not
depend on it being visible. This is a test-harness fix only; the QML and C++
product code are unchanged.

## Rules for lane agents on this machine

- Run the two `QmlStartupSmokeTest` skin cases as separate processes.
- Treat `QmlResponsiveBoundsTest` as the local layout gate; it now passes on a
  clean `main`.
- CI remains the authority for QML: the macOS jobs only compile, so QML test
  truth comes from the Linux job.
- Visual grabs cannot be produced headlessly here; request them from the
  integrator/owner and say so in the PR.
