# CI performance plan — faster GitHub Actions runs

**Date:** 2026-11-25
**Status:** merged in #110 (2026-11-26) and green on `main`. Phases 0, 1, 2 and
3c are implemented and measured; 3a/3b/3d are deferred.
**Scope:** `.github/workflows/ci.yml`, `.github/workflows/stems-onnx.yml`,
`.github/workflows/nightly-ci.yml` (new), `tools/debian_buildenv.sh`,
`tools/ci_timings.py` (new), `.github/workflows/qml-qt-api-guard.yml`
(audited, no change needed).

---

## 1. Goal and constraints

**Goal:** cut the wall-clock time from push to a green PR from ~18–23 minutes
(median) and up to 1 hour when a cache or mirror misbehaves, to **≤ 12 minutes
at the median and ≤ 20 minutes at p90**, without reducing coverage.

Constraints and decisions taken for this plan:

1. **All five required checks stay on every pull request**, including
   `macOS x86_64`. No job is dropped and no target is demoted to nightly-only.
2. The repository is **public**, so standard GitHub-hosted runner minutes are
   free. Runner-minutes still matter as a proxy for wall clock, queue pressure
   and flake exposure, but the plan optimizes for **time-to-green and
   reliability**, not billing.
3. `macOS x86_64` is the slowest target per compiled edge (~2.3× the arm64
   runner) and the most likely to be the critical path after Phase 1. It is
   therefore optimized, not removed.
4. Caching is hand-wired with `actions/cache/restore` + `actions/cache/save` so
   the save policy (main-only) is explicit, and change detection is hand-rolled
   shell inside a `changes` job (no third-party action). Both choices are small
   enough to read in the workflow and avoid a supply-chain dependency on the
   required path. `dorny/paths-filter` was considered and rejected for the
   second reason; its merge-base handling was reproduced with one shallow
   `git fetch` of the base commit.

**Non-goals:** packaging, signing, model downloads, Windows test coverage
(flagged in §9 as a separate gap), and anything under `docs/` that is not this
plan.

---

## 2. How this plan was measured

Per-step timings were pulled from the public Actions API for the 15 most recent
completed CI runs (2026-11-23 → 2026-11-25), including PR, push and `main`
runs, and reduced to medians and maxima. Reproduce with the command in
Appendix A.

### 2.1 Baseline: per-job step profile (median / worst, minutes)

| Job | Step | Median | Worst |
|---|---|---|---|
| Repository checks | whole job | 0.3 | 0.5 |
| **Linux x64** | Install build dependencies (apt) | **1.4** | **56.1** |
| Linux x64 | Restore ccache | 0.3 | 0.5 |
| Linux x64 | Configure | 0.1 | 0.2 |
| Linux x64 | Build | 5.1 | 7.0 |
| Linux x64 | Test (1 302 tests, `-j3`) | 2.9 | 3.0 |
| **Linux x64 job total** | | **10.0** | **63.6** |
| **Windows x64** | Pre-fetch buildenv | 1.5 | 3.9 |
| Windows x64 | Configure | 1.7 | 2.6 |
| Windows x64 | Build | **14.4** | **15.2** |
| **Windows x64 job total** | | **17.8** | **20.6** |
| **macOS arm64** | Pre-fetch buildenv (3.7 GB) | **4.2** | **28.0** |
| macOS arm64 | Restore ccache | 0.5 | 0.8 |
| macOS arm64 | Configure | 1.3 | 1.6 |
| macOS arm64 | Build | 5.4 | 7.7 |
| macOS arm64 | Save ccache | 0.4 | 0.5 |
| **macOS arm64 job total** | | **12.1** | **37.0** |
| **macOS x86_64** | Pre-fetch buildenv (3.7 GB) | 2.9 | 21.0 |
| macOS x86_64 | Restore ccache | 1.1 | 2.0 |
| macOS x86_64 | Configure | 2.5 | 4.1 |
| macOS x86_64 | Build | **8.1** | **46.2** |
| **macOS x86_64 job total** | | **15.3** | **52.4** |

Aggregate: **~56 runner-minutes per PR run**, with a **~18-minute critical
path** set by the Windows job. Whole-run medians from the API: 23.2 min for
`pull_request`, 19.8 min for `push`. Worst observed wall clock for a single
attempt: **63.6 min** (Linux, apt stall), `52.4` min (macOS x64, cold build),
`37.0` min (macOS arm64, slow buildenv download).

Measurement caveat: a run's `created_at → updated_at` includes every attempt.
Re-runs hours later inflate that span (the API reports up to 10 hours for runs
that were simply re-run in the evening), so all numbers above are per-attempt
job durations from the jobs endpoint, not run spans.


The `Stems ONNX` job measured separately: **36.8 min build**, every time
(see §3.4).

### 2.3 After: measured result

Measured by running the same commit twice (a cold run, then a re-run that
restores the caches), then confirmed on `main` after the merge:

| Job | Baseline | Cold | **Warm** | ccache hits |
|---|---|---|---|---|
| Windows x64 | 17.8 min | 30.5 | **4.8** | 98.9% (980/991) |
| macOS arm64 | 12.1 | 23.3 | **5.3** | 99.0% (983/993) |
| macOS x86_64 | 15.3 | 44.7 | **7.1** | 99.5% (862/866) |
| Linux x64 | 10.0 | 27.2 | **7.2** | 98.3% (1 134/1 154) |
| Stems ONNX | 36.8 (build) | — | **2.0 (build)** | 98.3% (1 129/1 148) |

Whole run: ~56 → ~25 runner-minutes, and wall clock ~20–23 → **7–14 minutes**
(the spread is runner variance; macOS x86_64 is usually the critical path).

The first run after the merge ([45890228471](https://github.com/christianstuart/paramix/actions/runs/45890228471),
push to `main`) was cold and green: macOS arm64 28.3 min, Windows 33.1,
Linux 33.7, macOS x86_64 45.8. That run seeded the `main` cache chain, so every
subsequent pull request starts warm.

Cold runs are slower than baseline on Windows because PCH is off; that is the
trade that produces the 98.9% warm hit rate. Judge the change from the second
run onward.

### 2.2 What is *not* slow

Worth stating so nobody optimizes it: checkout (0.1 min), configure (0.1–2.5
min), ccache restore (0.3–1.1 min), ccache save (0.1–0.9 min), repository
checks (0.3 min), QML Qt API guard (0.3 min), and the 1 302-test Linux suite
(2.9 min). The whole plan below is about four specific defects and two
structural inefficiencies.

---

## 3. Root causes

Line references in this section point at the workflow as it stood before this
change (commit `5cb50b6`). The rewritten file keeps the same step names, so the
descriptions remain findable; the paths were also verified against the new file
when each fix was applied.

### 3.1 Windows has no compiler cache — and cannot use the in-tree ccache path

`ci.yml:410-478` installs MSVC and the release buildenv, configures, and builds
with nothing between compilers and objects. Every run pays a cold 14.4-minute
MSVC compile of ~1 000 translation units, and it is the **critical path** of
the workflow.

The trap: `CMakeLists.txt:1010-1039` wires **sccache** on MSVC and only falls
through to **ccache** in the `else()` branch for non-MSVC compilers
(`1041-1092`). So `brew install ccache`-style thinking does not transfer: on
Windows the supported path is sccache, or an explicit
`-DCMAKE_CXX_COMPILER_LAUNCHER`.

Both tools require embedded debug info (`/Z7`) because `/Zi` writes a PDB that
defeats object caching. The tree already rewrites `/Zi` to `/Z7` for
RelWithDebInfo (`CMakeLists.txt:718-733`), so it was cache-ready; only the tool
was missing. PCH must be turned off for the cache to work at all. That trade is
what to measure, not assume: PCH + no cache versus no PCH + cache.

### 3.2 The pinned 3.7 GB buildenv is re-downloaded on every macOS run

`ci.yml:250-277` and `ci.yml:343-370` pre-fetch the archive with `curl` because
`CMakeLists.txt:147-160` uses `file(DOWNLOAD)` without a `STATUS` check. The
fetch is correct and resilient, but it is repeated in full on every run even
though the archive is pinned by SHA-256 in `tools/macos_buildenv.sh` (and
`tools/windows_buildenv.bat`) and changes only on an intentional bump.

Cost: 4.2 min (arm64) and 2.9 min (x64) at the median; 28 and 21 minutes at the
worst. This is the largest single source of variance in the workflow.

### 3.3 apt is unbounded

`ci.yml:173-177` runs `tools/debian_buildenv.sh setup`, which does
`apt-get update` plus a large install (Qt 6 dev, clazy, clang-tidy, lcov,
debhelper, devscripts, …). Median 1.4 min, but **38.9 and 56.1 minute
outliers** in the sample. No retry policy, no timeout, no failure isolation.
When it stalls, it owns the critical path and turns a 20-minute run into an
hour.

The script also never installs `ninja-build`, which is why Linux falls back to
Unix Makefiles (§3.6).

### 3.4 The Stems ONNX ccache is write-only in theory, never in practice

`stems-onnx.yml:104-111` saves the cache only when
`github.event_name == 'push'`, but the workflow's triggers are `pull_request`
and `workflow_dispatch` (`stems-onnx.yml:8-19`) — **there is no push trigger,
so the save step can never run.** `stems-ort-linux-*` is never written, every
restore misses, and each qualifying PR compiles the backend tests cold:
36.8 minutes, measured.

It triggers on 18 of the last 100 runs, partly because
`CMakeLists.txt` is in its `paths` list.

### 3.5 PRs cannot write cache, and the key churns

`ci.yml` restores with `key: <os>-${{ hashFiles('src/**', 'CMakeLists.txt', 'cmake/**') }}`
and a prefix `restore-keys`, and saves **only on push** (`ci.yml:211-220`,
`311-318`, `401-408`). Consequences:

- A source change always changes the exact key, so the exact key misses and
  the run relies on "most recent prefix match" — which may be a different
  branch's tree.
- Every push to a feature branch uploads a new ~0.5 GB entry. Feature branches
  are in the push trigger (`ci.yml:24-26`), so the 10 GB repository cache is
  churned by entries that will never be reused by a PR at the same key.
- A PR that touches a widely included header, or that adds translation units,
  recompiles nearly everything **every run** and cannot persist the result.
  Observed: a 46.2-minute macOS x64 build on a first-attempt PR run.

### 3.6 Linux builds with Makefiles

`ci.yml:199-204` never sets a generator, so CMake defaults to Unix Makefiles
on Linux. The macOS and Windows buildenv scripts export
`CMAKE_GENERATOR=Ninja`; Linux gets no equivalent. Ninja is typically 5–15%
faster on this tree and gives better parallel scheduling.

### 3.7 Every target builds tests; only Linux runs them

`CMakeLists.txt:3150` adds `mixxx-test` (164 test translation units plus
`res/mixxx.qrc`) to `ALL`. `docs/parallel-work.md:167` confirms macOS and
Windows "build but do not run tests". Those three jobs spend 1.5–3 minutes
each compiling and linking a binary nobody executes there.

### 3.8 No path gating, and push/PR runs do not cancel each other

Every push, including a docs-only edit, runs the full four-platform matrix.
`docs/parallel-work.md:169-171` tells contributors to keep documentation
changes separate precisely because of this, but nothing enforces it.

Separately, the concurrency group is `ci-${{ github.workflow }}-${{ github.ref }}`
(`ci.yml:33-35`). A PR's ref is `refs/pull/N/merge`, a branch push's is
`refs/heads/<name>`, so the two runs for the same tip never cancel. Four of 43
distinct tips in the sample had exactly this duplication.

### 3.9 Minor

- `RelWithDebInfo` in CI produces debug info no CI consumer uses; it inflates
  objects, cache size and link time.
- `brew install ccache` (0.2–0.3 min) runs on every macOS job because ccache is
  not part of the buildenv bundle.
- The pre-fetch and `brew install` steps are serial although they are
  independent.

---

## 4. Phase plan

Estimates are ranges with an explicit measurement gate; §6 defines acceptance.

| Phase | Work | Wall clock (p50) | Runner-min per code PR |
|---|---|---|---|
| Baseline | — | ~18–23 min | ~56 |
| 0 | Instrumentation | unchanged | unchanged |
| 1 | Fix the four defects (3.1–3.4) + Ninja + test-build trim | **~12 min** | **~37** |
| 2 | Path gating, cache policy, concurrency, nightly warm | **~11 min** (docs: ~1.5) | ~34 |
| 3 | Prebaked Linux image, stems consolidation (3c already applied) | **~9–10 min** | **~28–30** |

### Phase 0 — Instrument (implemented)

1. `ccache --show-stats --verbose` and `ccache -p --print-stats` run in every
   job and are appended to `$GITHUB_STEP_SUMMARY`, so the hit rate survives log
   rotation.
2. `tools/ci_timings.py` reports per-step medians and maxima for any number of
   recent runs (`python3 tools/ci_timings.py --runs 20`, authenticated when
   `GH_TOKEN` is set).
3. The "before" numbers in §2 were produced with that script.

Before changing keys again, record cache entry sizes with
`gh cache list -R christianstuart/paramix --limit 100`.

**Why first:** every claim below is a hit-rate or step-duration claim. Without
this, the Windows cache experiment in 1c cannot be judged.

### Phase 1 — Fix the defects (1–2 days)

#### 1a. Repair the Stems ONNX cache (implemented)

`stems-onnx.yml` now saves on every run with a
`stems-ort-linux-${run_id}-${run_attempt}` key and the existing prefix
`restore-keys`. The comment in the step records why the old
`event_name == 'push'` condition could never fire. Expected: 36.8 → ~6 min after
two warm runs.

#### 1b. Cache the buildenv archives (implemented)

Each desktop job restores `*.zip` from `actions/cache` before the pre-fetch,
skips the download on a hit, and saves the zip immediately after the pre-fetch
(before Configure extracts into the same directory). Keys are
`buildenv-<os>-<arch>-<hash of the buildenv script>`, so a cache hit means the
pinned archive is byte-identical. Only the zip is cached; the extracted tree
would be 8–10 GB and would evict every compiler cache.

Expected: macOS arm64 4.2 → ~1.2 min, macOS x64 2.9 → ~1.2, Windows 1.5 → ~0.2.

The buildenv lives at `${{ runner.workspace }}/buildenv` (the parent of the
checkout, outside the source tree) exactly as before. `runner.workspace` is not
in the documented `runner` context, so `actionlint` flags it; real runs report
it (verified in the 2026-11-24 arm64 job log), and the value is unchanged by
this PR.

#### 1c. Give Windows a compiler cache (implemented as variant B)

The job installs ccache **4.14.3** from the upstream release, pinned by
SHA-256 (`c845c3d6…a67dd`, verified against the downloaded archive), puts it on
`PATH`, and configures with:

```text
-DCMAKE_C_COMPILER_LAUNCHER=ccache
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
-DCMAKE_C_FLAGS_RELWITHDEBINFO="/O2 /Ob1 /DNDEBUG /Z7"
-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="/O2 /Ob1 /DNDEBUG /Z7"
```

Passing the launcher explicitly avoids the fact that `CMakeLists.txt` wires
only **sccache** on MSVC. The tree already rewrites `/Zi` to `/Z7` for
RelWithDebInfo (`CMakeLists.txt:718-733`), so ccache can cache the objects
without any flag override. PCH is off because MSVC precompiled headers are not
cacheable. `CCACHE_DIR` is `${{ runner.temp }}/ccache` with
`CCACHE_COMPILERCHECK=content` (file mtimes are unreliable on Windows) and a
1 GB cap.

Fallbacks if the measured hit rate disappoints: (A) sccache, which is the path
the build system natively supports on MSVC; (C) cache `build/` and rely on
Ninja's incremental build.

Measurement gate: if the steady-state build does not drop below **8 minutes**
with a ≥ 40% hit rate, revert this one commit and try A or C.

Expected: 14.4 → 4–6 min, job 17.8 → ~8.5. This is the single most valuable
change in the plan.

#### 1d. Stop building tests where they are not run (implemented)

`-DBUILD_TESTING=OFF` is passed on macOS arm64, macOS x64 and Windows. The
Linux job keeps tests on. `CMakeLists.txt:2841` already guards every test target
on this option. `-DBUILD_BENCH=OFF` must accompany it: Google Benchmark is
present in all three buildenvs, so `BUILD_BENCH` defaults to ON and
`CMakeLists.txt:2863` aborts the configure without unittests (caught by the
first CI run of this change). Expected: −1.5 to −3 min per job, and a smaller
ccache working set.

#### 1e. Ninja and apt hardening on Linux (implemented)

- `ninja-build` is installed by `tools/debian_buildenv.sh` and the Linux
  configure step passes `-G Ninja`.
- `apt-get update` and every `apt-get install` in that script use
  `-o Acquire::Retries=5 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30`,
  so a stalled mirror fails fast and retries instead of owning the run for an
  hour.

Expected: build −0.5 to −1 min; apt tail 56 → single-digit minutes, usually a
transparent retry.

#### 1f. Bound a single test (implemented)

`ctest --timeout 300` is passed on Linux. During measurement, the
analysis-engine test `Engine.DetectsBpmAndDownbeatForEveryRateBucket` ran in
~9 s in one run and stalled for 17+ minutes in the next, with identical build
output — a pre-existing flake in `lib/paramix-analysis` (single-threaded DSP,
no locks), not a flag or generator regression. Without a per-test timeout it
silently consumes the 75-minute job budget. 300 s is ~30x the slowest observed
test, so a hang now fails in 5 minutes and names the test.

**Phase 1 exit criteria:** all five checks still green on a draft PR, Windows
build ≤ 8 min with reported hit rate, macOS pre-fetch ≤ 2 min on the second
run, Stems ONNX ≤ 10 min, Linux apt ≤ 2 min.

### Phase 2 — Structure (2–3 days)

#### 2a. Gate heavy jobs on what actually changed (implemented)

A `changes` job classifies the diff with plain `git`: for a `pull_request` it
fetches `github.event.pull_request.base.sha` once with `--depth=1`, for a `push`
it uses `github.event.before`, and it sets `code=true` unless it can prove the
change touches only non-build paths (`src/**`, `lib/**`, `cmake/**`,
`packaging/**`, `res/*.qrc`, `CMakeLists.txt`, `tools/*buildenv*`, or the two CI
workflow files). A root commit, a force push, a missing base, or a
schedule/dispatch run all fall back to `code=true`.

Every heavy job declares `needs: changes` and

```yaml
if: ${{ !cancelled() && needs.changes.outputs.code != 'false' }}
```

so an empty or failed classification can only ever run *more* work, never skip
a required check. Job-level `if:` is deliberate: a skipped job reports a
successful check, whereas a workflow-level `paths` filter leaves required checks
**pending**. Verify the skip behavior on a draft PR before relying on it (§7).

`res/qml/**` and `res/skins/**` are intentionally not build-affecting: QML is
read from disk at runtime (`AGENTS.md`) and the Qt-API guard covers minimum-Qt
compatibility. `res/*.qrc` **is** included, because a resource manifest change
can break `rcc`.

Expected: docs-only PR → `checks` + classification only (~1.5 runner-minutes)
with all five required checks still reporting; no change for code PRs.

#### 2b. Replace per-hash ccache entries with a main-fed cache chain (implemented)

Per platform, restore is:

```yaml
key: ccache-linux-${{ github.run_id }}-${{ github.run_attempt }}
restore-keys: |
  ccache-linux-
```

and save is gated on `github.ref == 'refs/heads/main'`. The attempt suffix
matters: a re-run shares `run_id`, and `actions/cache/save` cannot overwrite an
existing key.

Properties: PRs always restore the newest `main`-saved entry (a consistent,
mostly-overlapping tree instead of a random feature branch), and no upload
happens on PRs or feature branches, so the post-build save no longer sits
between "build finished" and "check green".

Set `CCACHE_MAXSIZE=1G` and `CCACHE_COMPRESS=true` in each job's `env:`. 0.5 GB
observed working sets fit comfortably.

Growth policy: every `main` build writes one entry, and old entries are dropped
by LRU/quota, which is harmless because restore always takes the newest. If
`gh cache list` ever shows the buildenv zip being evicted, add a prune step
(`gh cache list --key <prefix>` + `gh cache delete`, needs `actions: write`) to
keep the two newest per platform. It was left out of this change to avoid
platform-specific shell in four jobs before the growth rate is known.

Measurement override: setting the repository variable `CI_CCACHE_BRANCH` to a
branch name lets that branch's runs save the cache. It exists so a cache change
can be measured with two runs of the same commit (a cold run that saves, then a
re-run that restores) instead of waiting for a merge. Delete the variable when
the measurement is done; the default remains main-only.

Trade-off to record: a PR now only benefits from `main`'s cache, not from its
own earlier runs. For a PR that changes a core header and is pushed 10 times,
that means repeated partial cold compiles. That is the accepted cost of a
stable, small cache; the alternative (save per branch) is what causes today's
churn and the 46-minute cold builds.

#### 2c. Run each tip once (implemented)

Two changes, both required:

- `push` is limited to `main`. Contributor branches are validated by their
  `pull_request` run, so a branch tip no longer produces two runs.
- `concurrency` cancels a superseded run of the same branch:

```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.head_ref || github.ref_name }}
  cancel-in-progress: true
```

`github.head_ref` is set only on `pull_request` and equals the branch name; for
a push to `main`, `github.ref_name` is the same form. `github.ref` would not
work — it carries the `refs/heads/` prefix, so the strings never match (observed
on the first run of this PR). `github.workflow` keeps a nightly run from
cancelling an in-flight run of the same commit.

Why the trigger change is not optional: cancelling a same-commit run leaves its
check runs in a `cancelled` state, and GitHub's status rollup treats those as a
failure. PR #110 measured `mergeStateStatus: BLOCKED` with a `FAILURE` rollup
while every surviving check was green. With one run per tip, there is nothing
to cancel on the same SHA, and a superseded run's cancelled checks belong to
the older commit and are ignored by the PR rollup.

A branch with no PR can still be built with
`gh workflow run ci.yml --ref <branch>`.

#### 2d. Nightly warm build (implemented)

`.github/workflows/nightly-ci.yml` calls `ci.yml` through `workflow_call` at
04:00 UTC daily and on demand, so no job definitions are duplicated. It keeps
the compiler-cache and buildenv entries warm (GitHub evicts entries unused for
seven days) and keeps the macOS x64 target exercised when no PR touches C++.
On a public repo it costs no billed minutes. A failed nightly marks `main` red
without blocking merges.

**Phase 2 exit criteria:** docs-only PR completes in < 2 min with all five
required checks reported; a PR restored from `main`'s chain reports a higher
hit rate than the current per-hash scheme; no run is canceled that produced a
required check for the current tip.

### Phase 3 — Bigger bets (measure before keeping)

| Item | Change | Expected | Risk |
|---|---|---|---|
| 3a. Prebaked Linux image | Build `ghcr.io/<owner>/paramix-ci` from `tools/debian_buildenv.sh` in its own workflow; run the Linux job in `container:` pinned by digest | apt 1.4 → 0.0 (and the 56-min tail disappears) | Container paths move `HOME`; set `CCACHE_DIR=/github/home/.cache/ccache` and cache that path |
| 3b. Fold stems into the Linux build | Pass `-DSTEM_SEPARATION=ON` (ONNX Runtime install is 0.1 min) in the main Linux job; delete the separate compile, keep only the `-R DemucsOnnxSeparatorTest` invocation | removes a whole ~26–37 min job and one cache chain | `STEM_SEPARATION` is `AUTO` by default (`CMakeLists.txt:1183-1209`); forcing ON must not break the non-stems matrix — verify all five jobs |
| 3c. CI build type (implemented) | `-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG -g0"` on Linux/macOS, `/Z7` on Windows (keep `-DNDEBUG`; dropping it re-enables asserts) | −5–10% compile, ~40% smaller cache entries | No debug symbols in CI artifacts; nothing in CI consumes them today |
| 3d. Unity build | `-DCMAKE_UNITY_BUILD=ON` in a throwaway job only | −20–40% compile if it works | ODR/macro collisions are plausible in a 1 000-TU tree; do not put this on the required path until proven |

3c is done; 3a and 3b are the two remaining structural wins; 3d is optional.

---

## 5. Cache budget and eviction policy

GitHub allows 10 GB per repository on this plan and evicts least-recently-used
entries, plus anything unused for 7 days. The entries this plan adds:

| Entry | Size | Value | Priority |
|---|---|---|---|
| `ccache-linux-*` | ~0.5 GB | saves a 20+ minute cold build | 1 |
| `ccache-windows-x64-*` | ~0.6 GB | saves a 14-minute cold build | 1 |
| `ccache-macOS-arm64-*` | ~0.5 GB | saves a 26-minute cold build | 1 |
| `ccache-macOS-x64-*` | ~0.5 GB | saves a 46-minute cold build | 1 |
| `buildenv-macOS-arm64-*` zip | ~3.7 GB | saves ~3 min per run, removes the 28-min tail | 2 |
| `buildenv-Windows-x64-*` zip | ~1–2 GB | saves ~1.3 min per run | 3 |
| `buildenv-macOS-x64-*` zip | ~3.7 GB | saves ~1.7 min per run, removes the 21-min tail | 4 |

ccache entries are worth far more per gigabyte than buildenv zips: a cold build
costs 14–46 minutes, a buildenv download costs 1.5–4 minutes at the median.
The policy follows that ordering:

1. Never cache an extracted buildenv (8–10 GB; it would evict everything).
2. Cap each ccache at 1 GB (`CCACHE_MAXSIZE=1G`, compressed). Four platforms
   ≈ 2 GB per generation; old generations age out by LRU. Add the prune step
   from §2b if `gh cache list` shows pressure.
3. Cache the macOS arm64 zip and the (smaller) Windows zip: ≈ 5–6 GB with
   ccache, comfortably inside quota.
4. Cache the macOS x64 zip only while the total stays below ~9 GB. It is the
   first entry to drop: that job then pays a ~2.9-minute download, still far
   cheaper than losing a ccache chain.
5. If `gh cache list` shows repeated eviction of ccache entries anyway, move
   the buildenv zips out of Actions cache into a GHCR OCI artifact or a release
   asset in this repository, pinned by digest/SHA and pulled with a checksum
   check. That removes 5–7 GB from the quota entirely at the cost of mirroring
   the pinned bundles once per buildenv bump.

Monitoring: record cache sizes and hit rates in the Phase 0 summary; treat "a
platform's ccache hit rate below 40% for 5 runs" as a quota or key bug, not as
noise.


---

## 6. Acceptance criteria

Measured over ≥ 20 runs after each phase, using Appendix A:

| Metric | Baseline | Target after Phase 1 | Target after Phase 2 |
|---|---|---|---|
| Code-PR wall clock, p50 | ~20 min | ≤ 12 min | ≤ 11 min |
| Code-PR wall clock, p90 | ~30 min | ≤ 20 min | ≤ 18 min |
| Windows build | 14.4 min | ≤ 8 min | ≤ 8 min |
| macOS pre-fetch | 4.2 / 2.9 min | ≤ 2 min | ≤ 2 min |
| Linux apt | 1.4 min (56 max) | ≤ 2 min | ≤ 2 min |
| Stems ONNX build | 36.8 min | ≤ 10 min | job retired (3b) |
| Docs-only PR | ~56 runner-min | ~1.5 min | ~1.5 min |
| Infra-caused failures | apt stalls to 56 min; truncated buildenv fetches (`docs/baseline-report.md` §4) | 0 in 20 | 0 in 20 |

Any phase that misses its target is reverted rather than carried forward. Each
phase lands as its own PR with the measured before/after table in the
description, so a regression has a single commit to bisect.

---

## 7. Branch-protection safety

All five checks are required (`docs/agent-context.md:108`), so any change to
when a job runs must preserve five reporting contexts:

1. **Job-level `if:` only.** Skipped jobs report a successful check; a
   workflow-level `paths` filter does not create the check at all and leaves it
   pending. Never add `paths`/`paths-ignore` to `on:` for `ci.yml` while the
   checks are required.
2. **Fail-safe classification.** `changes` is not a required check, so it must
   never be able to skip a build by failing: the condition is
   `!cancelled() && needs.changes.outputs.code != 'false'`, which runs the
   matrix on an empty or failed result. `code=false` is only written after a
   successful diff against a base commit that exists.
3. **Draft-PR verification before this merges:** push a docs-only commit to a
   scratch PR and confirm all five contexts show green (not "expected"), then a
   code commit and confirm all five actually execute.
4. If (2) turns out not to hold for this repository's ruleset, fall back to
   step-level gating inside each job: keep the job unconditional and put
   `if: needs.changes.outputs.code != 'false'` on the install/configure/build/test
   steps. The job still reports success and costs ~10 s of runner setup.
5. Never rename a job without updating branch protection in the same change.

---

## 8. Risks

| Risk | Mitigation |
|---|---|
| MSVC caching does not pay off with PCH disabled | Variant ladder A → B → C with an 8-minute gate; keep the change only if measured |
| Cache quota eviction degrades ccache | §5 priority order; monitor `gh cache list` and hit rate |
| Skipped required checks behave unexpectedly | Draft-PR verification; step-level fallback (§7) |
| `actions/cache` restore of a 3.7 GB zip is slow | Measured at ~1.2 min; it is still 3× faster than the download and removes the 28-min tail |
| Buildenv release environment misbehaves with `RelWithDebInfo` | Out of scope for this plan; the pinned debug bundle stays in use |
| Third-party supply chain | No new third-party dependencies: change detection is shell, caching is `actions/cache`, and the only added install (ccache 4.14.3) is pinned by SHA-256 |
| A classification bug silently skips builds | `code=false` requires a successful diff against an existing base; anything else runs the full matrix (§2a, §7) |
| Removing `STEM_SEPARATION`'s dedicated job hides a break | Fold-in is Phase 3b and requires the full matrix green plus the backend test still run by name in the Linux job |

---

## 9. Out of scope but recorded

- **Windows runs no tests.** Green on Windows currently means "compiles"
  (`docs/parallel-work.md:167`). Adding a Windows `ctest` job on the nightly
  workflow would close a real coverage gap; it is a correctness change, not a
  speed change, so it is deliberately not bundled here.
- **macOS x64 still builds with `-DQML=OFF`** (the `macos-x64` Configure step),
  a known gap against the QML-only UI decision (`docs/agent-context.md:253-254`).
  Fixing it costs ~11 minutes on the slowest runner; do it as its own change
  once the new cache warm-up has been measured, and note that it will move the
  critical path back to macOS x64.
- **`qml-qt-api-guard.yml`** needs no change: 0.3 min, path-filtered, static
  only.

---

## 10. Documentation touchpoints

When each phase lands, update the docs that currently describe CI behavior so
contributors are not working from stale advice:

- `docs/parallel-work.md:162-171` — job list, "a full clean build takes ~20–40
  minutes", and the "keep documentation-only changes in their own PR" note
  (documentation-only PRs now skip the matrix automatically).
- `docs/baseline-report.md` §4 — the 2026-11-11 timings are now historical;
  add a pointer to this document.
- `docs/agent-context.md` §5 — the buildenv-download trap gains "the archive is
  now cached; a checksum mismatch still means a truncated transfer".
- `AGENTS.md` needs no change; it contains no CI timing claims.

To be filled in after the first measured run: the "after" table (per-step
medians and maxima, cache hit rates from the job summaries) belongs in the PR
description for this change and then in this document under §2.


---

## 11. First-run checklist

The first run after this change is a guaranteed compiler-cache miss: the
compile flags changed, so every object is rehashed. Judge the change from runs
2–3 onward. In order:

1. **Linux**: the `Install build dependencies` step should be clean and, if the
   mirror stalls, fail fast and retry rather than run for 40+ minutes. Configure
   should print `Found ccache` and build with Ninja.
2. **macOS x64 / arm64**: run 1 downloads the buildenv and saves it (one-time
   ~1–2 min upload); run 2 must skip `Pre-fetch build environment`
   (`cache-hit` true) and finish configure in ~1 min.
3. **Windows**: `Install ccache` must print a version; the build log should show
   `ccache` as the launcher. Compare the build time and the hit rate printed in
   the step summary against the 14.4-minute baseline. If the build is slower
   than ~8 minutes across two warm runs, revert the ccache commit and try
   variant A or C from §4/1c.
4. **Classify changes**: open a docs-only PR (or push a `docs/**` commit to a
   scratch PR) and confirm the four build jobs report as skipped while all five
   required checks stay satisfied. If they report "Expected", switch to the
   step-level fallback in §7.
5. **Caches on `main`**: the chain only starts after the first `main` build, so
   PRs opened before that are cold. Run `gh cache list` and compare the entry
   sizes with §5's budget.
6. **Nightly**: confirm `Nightly CI` can be dispatched and that its jobs are
   named `Nightly CI / ...`, leaving the `CI / ...` required contexts untouched.

Rollback granularity is per file section: the Windows cache is confined to the
`windows-x64` job, the path gating to the `changes` job plus four `needs:`/`if:`
lines, and the cache keys to the restore/save steps. Commit in those three
groups if a partial revert is likely.

---

## Appendix A — measuring

```bash
# Per-step medians and maxima over the 20 most recent completed CI runs.
GH_TOKEN=... python3 tools/ci_timings.py --runs 20

# A single run's job and step breakdown.
gh run view <id> --json jobs

# Cache entries and sizes (quota view).
gh cache list -R christianstuart/paramix --limit 100
```

The script is stdlib-only and falls back to anonymous API access (60 requests
per hour, one per run) when no token is set.
