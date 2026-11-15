# Paramix — Deezer / Deemix integration plan

**Status:** Assessment and proposal v0.1 · 2026-11-15
**Scope:** architecture, portability assessment, phased delivery plan. No source code was
changed to produce this document.
**Upstream reference:** [`bambanah/deemix`](https://github.com/bambanah/deemix) at
`b611d63` (monorepo: `deezer-sdk`, `deemix`, `webui`, `gui`, `cli`).

---

## 0. Executive summary

**Verdict: the core of Deemix ports well to C++/Qt. The web/Node layer does not, and should
not, be ported. A native integration is feasible and fits Paramix.

Three findings drive the plan:

1. **Deemix is mostly plumbing around a small protocol core.** The monorepo is ~13.4k
   lines of TypeScript excluding tests (including 4.7k for the Vue web UI), but only ~4k
   are the download engine: the Deezer private/public API calls, the Blowfish/AES stream
   decryption, the download orchestration and the path/metadata rules. The rest is `got`,
   `tough-cookie`, Node streams, Electron, Vue and a CLI. Qt provides
   equivalents for all of that (`QNetworkAccessManager`, `QNetworkCookieJar`,
   `QSaveFile`, QML), so those lines are replaced rather than translated.
2. **The hard parts are not code, they are protocol and product.** The private
   `gw-light.php` API is undocumented and changes without notice; the `media.deezer.com`
   token exchange is licensed per account/region; and the UX (search, browse, queue,
   sign-in, error states) is larger than the port itself.
3. **Paramix already has the expensive half of the product.** TagLib metadata writing,
   cover-art pipeline, library import, beatgrid/waveform analysis, and stem separation all
   exist. Deemix's `tagger.ts` (~550 lines of ID3/FLAC writing) can be deleted, not ported.
   The integration should feed downloads straight into the existing analysis/stems
   pipeline — that is the differentiator versus running Deemix next to Mixxx.

**Recommended shape:** a native `src/deezer/` module (no Node, no sidecar), a
`DeezerFeature` library source, and a dedicated QML pane in the existing library area. Ship
in phases; the first externally testable milestone is to search + download one FLAC

---

## 1. What Deemix actually is

| Package | Role | Relevant to Paramix? |
|---|---|---|
| `deezer-sdk` | REST wrapper for public API, private GW API, media URL exchange, cookie/ARL login | **Yes — port** |
| `deemix` | Decryption, download queue, path templates, tagging, fallbacks | **Yes — port the logic, drop tagging** |
| `deemix/webui` | Express + Vue web UI and server | No — replace with QML |
| `deemix/gui` | Electron shell | No |
| `deemix/cli` | Command-line front-end | No (optional diagnostic tool) |
| `plugins/spotify.ts` | Spotify link → search fallback | Not in v1 |

### 1.1 The protocol in one page

**Authentication.** The user supplies an `arl` cookie from a logged-in `deezer.com`
browser session. Deemix sets it on `.deezer.com` and calls
`deezer.getUserData` on the private gateway. The response yields:

- `checkForm` — CSRF/API token used for subsequent gateway calls,
- `USER.OPTIONS.license_token` — required for `media.deezer.com/v1/get_url`,
- `USER.OPTIONS.web_lossless` / `mobile_lossless` / `web_hq` / `mobile_hq` — plan flags,
- `USER.OPTIONS.license_country` — used to explain geo-blocks.

**Search/browse (no auth required).** `https://api.deezer.com/{search,track,album,artist,playlist,...}`
returns JSON. The public API is enough to build the whole browse experience; only playback
URLs need auth. This is a UX win: users can search before signing in.

**Track resolution.** The public track object does not contain everything needed to
download. The private `song.getData`/`deezer.pageTrack` response adds:

- `MD5_ORIGIN` (a.k.a. `md5_origin`) and `MEDIA_VERSION`,
- `TRACK_TOKEN` and `TRACK_TOKEN_EXPIRE`,
- per-format file sizes (`FILESIZE_FLAC`, `FILESIZE_MP3_320`, …),
- `FALLBACK` / `fallback_id` and `alternative_albums` for unavailable tracks.

**Media URL exchange.** `POST https://media.deezer.com/v1/get_url` with the account
`license_token`, the `track_token`, and the desired format. The response contains a signed
CDN URL (`https://cdns-proxy-X.dzcdn.net/media/...` or `/mobile/...`), which is
`BF_CBC_STRIPE` encrypted. A `2002` error means geo-blocked; a plan flag of `false` means
the account cannot stream that quality.

**Legacy fallback ("feeling lucky").** Deemix can also compute a CDN URL directly from
`MD5_ORIGIN`, `MEDIA_VERSION` and the track ID without a token exchange. That is more
brittle and more clearly an access-control bypass. Recommend shipping it off by default and
not exposing it in normal UI.

### 1.2 Decryption (the only genuinely non-obvious code)

> These constants and flows are protocol facts. Implement them from the public
> specification above and verify against test vectors; do not copy the source file
> line-by-line if the GPL-2.0-only route is chosen.

**Stream path (AES-128-ECB, no padding):**

```
urlPart = md5_origin + "¤" + format + "¤" + songId + "¤" + mediaVersion
md5val  = MD5(urlPart)                       // lowercase hex
step2   = md5val + "¤" + urlPart + "¤"
step2  += "." * (16 - (len(step2) % 16))     // pad to 16-byte multiple
path    = AES128_ECB_encrypt("jo6aey6haid2Teih", step2)  // lowercase hex
url     = "https://cdns-proxy-" + md5_origin[0] + ".dzcdn.net/api/1/" + path
        // or "https://e-cdns-proxy-" + md5_origin[0] + ".dzcdn.net/mobile/1/" + path
```

**Blowfish key:**

```
md5id = MD5(songId)                           // 32 hex chars / 32 ASCII bytes
key[i] = md5id[i] XOR md5id[i + 16] XOR "g4el58wc0zvf9na1"[i]   for i in 0..15
```

**Stream decryption:** Blowfish-CBC, IV `{0,1,2,3,4,5,6,7}`, no padding. The ciphertext is
interleaved in 6144-byte stripes: decrypt the first 2048 bytes of each stripe, copy the
remaining 4096 bytes verbatim. After that, strip leading zero padding until bytes 4..7 are
`ftyp` (MP4) or the container magic is recognised.

Everything else in the download path is ordinary HTTP and file I/O.

---

## 2. Portability assessment

| Subsystem | Deemix implementation | C++/Qt replacement | Effort | Risk |
|---|---|---|---|---|
| HTTP + cookies | `got` + `tough-cookie` | `QNetworkAccessManager` + `QNetworkCookieJar` | Low | Low |
| JSON | `JSON.parse` + zod schemas | `QJsonDocument` + hand-written parsing | Low | Low |
| Public API | `deezer-sdk/api.ts` | `DeezerApiClient` | Low | Low |
| Private GW API | `gw.ts` (~590 LOC) | `DeezerGwClient` | Medium | **High (undocumented, drifts)** |
| Login/ARL | `deezer.ts` (~200 LOC) | `DeezerSession` | Low | Medium (ARL expiry) |
| Media URL exchange | `get_tracks_url()` | `DeezerMediaClient` | Low | Medium (geo/licence) |
| AES-128-ECB | Node `crypto` | vendored tiny AES or OpenSSL EVP | Low | Low |
| Blowfish-CBC | Node `crypto` / `blowfish.cjs` | vendored public-domain Blowfish | Low | Low |
| Stream decryption | `decryption.ts` | `QIODevice` filter + `QSaveFile` | Medium | Low |
| Download queue | `downloader.ts` (~830 LOC) | `DeezerDownloadQueue` | Medium | Medium |
| Path templates | `pathtemplates.ts` | `DeezerPathPlanner` | Medium | Low |
| Metadata/tagging | `tagger.ts` + 2 npm deps | **Existing TagLib layer** | **Zero** | Low |
| Cover art | `downloadImage.ts` | `QNetworkAccessManager` + `AsyncImageProvider` | Low | Low |
| Local paths/defaults | `localpaths.ts` | `QStandardPaths`, Paramix settings | Low | Low |
| Extra tools | `executeCommand`, M3U8, logs | drop or gate | Low | Low |
| Web GUI | Vue/Express | QML pane | **High** | Medium |
| Electron/CLI | — | not needed | — | — |
| Spotify links | `spotify.ts` (~880 LOC) | defer | — | — |

### 2.1 What "not easy" means concretely

- **A literal port is not possible.** Node streams and `got`'s retry/cancel model do not
  exist in Qt; the download pipeline is a rewrite, roughly 700 lines.
- **Crypto is the easy part.** ~500 lines of vendored AES/Blowfish plus ~300 lines of
  stream filter. Generate test vectors from Deemix and assert them in CI.
- **The private API is the maintenance tail.** Budget ~1 day per upstream protocol change,
  and design the client so a changed endpoint or field is a one-file fix.
- **The UI is the largest single chunk.** Search, tabs, album/artist/playlist detail,
  queue, sign-in, settings, empty/error states, accessibility — 1.2–1.8k lines of QML, since they must be cleanly integrated into Paramix UI.
- **Rate limits and bans are real.** Public API allows roughly 50 requests / 5 s per IP;
  downloading a large playlist at concurrency 10 is how accounts get flagged. Default to
  3 concurrent downloads and a token-bucket request limiter.

---

## 3. Paramix integration points

The QML library is already feature-driven and the sidebar mirrors `LibraryFeature`s, so the
integration surface is small.

| Concern | Existing code | Change |
|---|---|---|
| Register a library source | `src/library/library.cpp` (~line 150, after `AnalysisFeature`) | `addFeature(make_parented<DeezerFeature>(this, m_pConfig, m_pTrackCollectionManager))` |
| Feature contract | `src/library/libraryfeature.h` | subclass; `title()`, `sidebarModel()`, `activate()` |
| Sidebar → QML view routing | `src/qml/qmlsidebarmodelproxy.{h,cpp}` | add `viewId()` to `LibraryFeature`; expose `currentView` on the proxy (≈20 LOC) |
| Library pane | `res/qml/Library.qml` | `StackLayout`: `TrackList` + `DeezerPane` |
| Search box routing | `res/qml/Library.qml`, `QmlSidebarModelProxy::search()` | route to Deezer when the Deezer pane is active |
| Settings page | `res/qml/Settings.qml` (after `Settings.Stems`) | `Settings.Deezer` |
| Settings storage | `src/qml/qmlconfigproxy.{h,cpp}`, `preferences/usersettings.h` | new `[Deezer]` group |
| QML API | new `src/qml/qmldeezerproxy.{h,cpp}`, registered in `qmlapplication.cpp` | `Mixxx.Deezer` singleton |
| Cover images | `src/qml/asyncimageprovider.{h,cpp}`, registered in `qmlapplication.cpp` | `image://deezer/...` provider |
| Import a file | `TrackCollectionManager::resolveTrackIdsFromUrls({url}, true)` | add immediately, no full scan |
| Write tags | `mixxx::MetadataSourceTagLib` / `Track::exportMetadata` | reuse; no `tagger.ts` port |
| Cover in library | `CoverArtUtils` (sidecar `cover.jpg`) | works without new code |
| Analysis | `Library::scheduleAnalysis()`, `AnalysisFeature` | optional post-download hook |
| Stems | `mixxx::stems::StemService::queueExport()` | optional post-analysis hook |
| Build | `CMakeLists.txt` (~1471–1500, ~4109–4150) | new `src/deezer/*` and `src/qml/qml*.cpp` entries |
| Tests | `src/test/`, `src/test/mock_networkaccessmanager.h` | crypto vectors + HTTP fixtures |

### 3.1 Minimal core change for a custom pane

Today `QmlSidebarModelProxy::activate()` calls `SidebarModel::clicked()`, which calls
`feature->activate()`; features that have a track table emit `showTrackModel(...)` and the
QML track list appears. A feature with a custom pane needs one extra bit of metadata:

```cpp
// library/libraryfeature.h
/// Identifier of a custom QML pane. Empty means "show the track table".
virtual QString viewId() const {
    return {};
}
```

```cpp
// qml/qmlsidebarmodelproxy.h
Q_PROPERTY(QString currentView READ currentView NOTIFY currentViewChanged)
```

`activate()` sets `currentView` from the feature; `slotShowTrackModel()` clears it. In
`res/qml/Library.qml`:

```qml
StackLayout {
    currentIndex: root.sidebar.currentView === "deezer" ? 1 : 0
    LibraryComponent.TrackList { model: root.sidebar.tracklist }
    LibraryComponent.DeezerPane { }
}
```

This keeps every existing source working and gives future online sources (Bandcamp,
SoundCloud, …) the same seam.

---

## 4. Proposed architecture

```
src/deezer/
  deezertypes.{h,cpp}          // Track, Album, Artist, Playlist, DownloadItem, errors
  deezerjson.{h,cpp}           // tolerant parsing of public + GW payloads
  deezercrypto.{h,cpp}         // MD5, AES-128-ECB, Blowfish-CBC, stream path
  deezerstreamfilter.{h,cpp}   // QIODevice that decrypts/depads a CDN stream
  deezersession.{h,cpp}        // ARL cookie, getUserData, licence flags, sign-out
  deezerapiclient.{h,cpp}      // public API + rate limiter + retry
  deezergwclient.{h,cpp}       // private gw-light calls + token refresh
  deezermediaclient.{h,cpp}    // get_url, quality selection, preview guard
  deezerdownloadtask.{h,cpp}   // one track: resolve → stream → write → verify
  deezerdownloadqueue.{h,cpp}  // queue, concurrency, cancel, retry, errors
  deezerpathplanner.{h,cpp}    // templates, sanitisation, collisions, padding
  deezerimporter.{h,cpp}       // tags, cover, library insert, analysis/stems hooks
  deezerfeature.{h,cpp}        // LibraryFeature, owns the service
  deezerservice.{h,cpp}        // app-lifetime facade (session + queue + caches)
  deezercache.{h,cpp}          // "already downloaded" index (JSON/SQLite)
  vendor/aes128.{h,cpp}        // small public-domain AES
  vendor/blowfish.{h,cpp}      // small public-domain Blowfish
src/qml/
  qmldeezerproxy.{h,cpp}       // QML singleton: signIn, search, detail, queue
  qmldeezersearchmodel.{h,cpp} // QAbstractListModel of results
  qmldeezerqueuemodel.{h,cpp}  // download queue model
  qmldeezerimageprovider.{h,cpp}
res/qml/Deezer/
  DeezerPane.qml               // StackView host
  SignInPanel.qml
  SearchView.qml               // tabs + result list/grid
  AlbumView.qml / ArtistView.qml / PlaylistView.qml
  TrackRow.qml
  QueueDrawer.qml
  ErrorState.qml
res/qml/Settings/Deezer.qml
```

### 4.1 Threading and lifetime

- **Network and file I/O on the main thread.** `QNetworkAccessManager` is asynchronous;
  the per-stripe Blowfish work is trivial (2 KB per 6 KB), and the audio engine runs on its
  own thread, so downloads cannot cause audio dropouts. This avoids all cross-thread
  lifetime bugs and is the right v1 choice. If profiling shows UI jank on very fast links,
  move `DeezerDownloadTask` into a dedicated `QThread` with its own QNAM; the signal
  boundary is already there.
- **Ownership.** `DeezerService` is created in `CoreServices` and lives for the app's
  lifetime; `DeezerFeature` holds a pointer. QML models are owned by the proxy
  (`QQmlEngine::CppOwnership`), matching `QmlLibraryTrackListModel`.
- **Library writes** happen on the main thread through `TrackCollectionManager`.

### 4.2 Persistence

- Settings in `[Deezer]` in `mixxx.cfg` through `UserSettings`/`QmlConfigProxy`.
- ARL: **not** in `mixxx.cfg` by default. Store it in a separate file with `0600`
  permissions (`<settings>/deezer-arl`) behind an explicit "Remember me" checkbox, and
  redact it from all logs. Adding `qtkeychain` (OS keychain) is a good v1.1 upgrade.
- Download index: a small JSON file or a dedicated SQLite table keyed by Deezer track ID,
  storing `isrc`, path, bitrate, timestamp. This powers "already downloaded", "upgrade to
  FLAC", and avoids adding a migration to Mixxx's library schema in v1.

---

## 5. User experience

The whole point is to make the existing track browser the place where music arrives.

### 5.1 Flows

1. **First run / signed out.** The Deezer source is visible and searchable. Results show a
   "Sign in to download" affordance. Opening it shows the ARL panel with a short
   "where to find your ARL" help text and a link to `deezer.com`.
2. **Search.** The existing library search box is reused when the Deezer source is active:
   type "Daft Punk", get tabs for **Tracks / Albums / Artists / Playlists**. Results page in
   as the user scrolls. Local tracks are untouched; switching back to "All tracks"
   restores the previous local search.
3. **Browse.** Clicking an album/artist/playlist pushes a detail view (cover, year, label,
   track count, per-track quality badges) with a **Download** button per row and
   **Download all**.
4. **Queue.** A drawer/tab shows active and completed downloads with progress, speed, ETA,
   cancel, retry, and "Show in library". Failures carry an actionable message
   (geo-blocked, plan too low, track removed, network).
5. **After download.** The file is tagged, cover is written, the track is inserted into the
   collection immediately, and the source switches to a brief "Downloaded" confirmation
   with **Load to deck** and **Separate stems** actions. Optional settings auto-run
   analysis and stem separation.
6. **Offline / errors.** Search shows a retry state; downloads pause with a banner and
   resume when the network returns.

### 5.2 Deliberate UX rules

- **Never download a 30-second preview silently.** Check `FILESIZE_*`, verify the final file
  duration against the Deezer duration (±2 s) and fail loudly if it is a preview.
- **Quality is visible before download.** A badge per row ("FLAC", "MP3 320", "MP3 128")
  derived from the account flags and the track's file sizes.
- **No auto-download.** Every transfer is user-initiated.
- **The library is the destination, not a separate folder.** The download folder should be
  a library source (ask once, offer to add it). The track is inserted directly after
  download, so a full rescan is never required.

---

## 6. Data flow

### 6.1 Search

```
QML search box → QmlSidebarModelProxy::search() → DeezerService::search(text, type, page)
  → DeezerApiClient GET api.deezer.com/search/... → QmlDeezerSearchModel::reset(items)
```

### 6.2 Download a single track

```
queue.enqueue(trackId)
  → GwClient.song.getData(trackId)            // MD5, MEDIA_VERSION, TRACK_TOKEN
  → GwClient.song.getAlbum(albumId)           // disc numbers, label, contributors
  → MediaClient.getUrl(trackToken, quality)   // signed CDN URL
  → QNetworkReply → DeezerStreamFilter → QSaveFile
  → verify (duration/format) → tag (TagLib) → cover (embed/sidecar)
  → TrackCollectionManager::resolveTrackIdsFromUrls({url}, true)
  → optional Library::scheduleAnalysis / StemService::queueExport
```

### 6.3 Collections

Album/playlist/artist downloads expand to a list of track IDs, then run through the same
per-track pipeline with a shared concurrency limiter. A collection item tracks aggregate
progress; partial failures are reported per track, never abort the whole album by default.

---

## 7. Quality, fallbacks and error taxonomy

**Quality selection** (mirrors Deemix but simpler):

```
if user chose FLAC and can_stream_lossless and FILESIZE_FLAC > 0 → FLAC
else if can_stream_hq and FILESIZE_MP3_320 > 0 → MP3_320
else → MP3_128
```

- "Prefer highest available" is the default; a fixed-quality setting exists for DJs who
  want uniform files.
- `FILESIZE_* == 0` or a missing media source means "not available in this region/plan" —
  do not fall back to a lower quality silently; show the reason and let the user opt in to
  fallback.
- Track tokens carry an expiry timestamp (`TRACK_TOKEN_EXPIRE`). Refresh before it lapses;
  retry once on a 403.
- Detect preview streams before writing: verify final duration and container magic.

**Error ids for the UI:** `NotSignedIn`, `ArlInvalid`, `ArlExpired`, `WrongPlan`,
`GeoBlocked`, `TrackNotAvailable`, `NotEncoded`, `PreviewOnly`, `RateLimited`,
`NetworkTimeout`, `DiskFull`, `PermissionDenied`, `PathTooLong`, `TaggingFailed`,
`ImportFailed`, `Cancelled`.

**Retry policy:** 3 attempts with exponential backoff + jitter for network/5xx; no retry for
4xx/geo/plan. Rate-limit `429` with `Retry-After` honouring.

---

## 8. Settings

```ini
[Deezer]
Enabled=true
RememberArl=false
DownloadDir=                     ; default: <Music>/Paramix Downloads
Quality=auto                     ; auto | flac | mp3_320 | mp3_128
AllowFallback=true
Concurrency=3
OverwritePolicy=skip             ; skip | overwrite | keep_both | only_tags
PathTemplate=%artist%/%album%/%tracknumber% - %title%
CreatePlaylistFolder=true
EmbedCover=true
SaveSidecarCover=true
AutoAnalyze=true
AutoSeparateStems=false
AddToMixxxPlaylist=true
RequestDelayMs=250
FeelingLucky=false               ; legacy CDN fallback, off by default
```

QML page sections: **Account**, **Download**, **After download**, **Advanced**. Mirror the
existing `Settings/Stems.qml` structure (`load()`, `save()`, `reset()`, `dirty`).

---

## 9. Phased plan

### Phase 0 — Protocol spike (3–5 days)

**Goal:** prove the two risky primitives and the API session in C++ before building
anything around them.

- `src/test/deezercryptotest.cpp`: vectors for `generateStreamPath`, Blowfish key derivation,
  stripe decryption and zero-depadding (captured samples from a test account).
- Throwaway `deezer_spike` executable (not shipped): ARL sign-in, search "Daft Punk", resolve
  one track, download a FLAC to a temp dir, open it in Paramix.
- **Gate:** a real FLAC plays in Paramix and its tags are visible in the library.

### Phase 1 — Core client (1.5–2 weeks)

- `deezertypes`, `deezerjson`, `deezercrypto`, `deezerstreamfilter`.
- `deezersession` (ARL, `getUserData`, plan flags, sign-out), `deezerapiclient`,
  `deezergwclient`, `deezermediaclient`.
- Token refresh, rate limiter, retry policy.
- Unit tests with recorded JSON fixtures and crypto vectors.
- **Gate:** headless test downloads one track of each available format and tags it.

### Phase 2 — Download pipeline (1.5–2 weeks)

- `deezerdownloadtask`, `deezerdownloadqueue`, `deezerpathplanner`, `deezerimporter`,
  `deezercache`.
- Single track, album, playlist, artist top-tracks; cancel/retry; atomic writes.
- Tag writing via `MetadataSourceTagLib`; cover embedding helper + sidecar.
- Immediate library insert; optional analysis/stems hooks.
- **Gate:** download a 10-track album, cancel mid-way, retry, and verify no partial files
  enter the library.

### Phase 3 — UI integration (2–3 weeks)

- `DeezerFeature` + `viewId()` seam + `QmlSidebarModelProxy::currentView`.
- `QmlDeezerProxy`, search model, queue model, image provider.
- `DeezerPane.qml` (sign-in, search, tabs, detail, queue), `Settings/Deezer.qml`.
- Search-box routing and restoring local search.
- Keyboard navigation, accessible names, theme tokens, empty/error states.
- **Gate:** a user who has never seen Deemix can sign in, search, download an album, and
  load a track without leaving Paramix.

### Phase 4 — Hardening (1–2 weeks)

- Proxy support (`QNetworkProxyFactory::setUseSystemConfiguration(true)` is currently not
  enabled; add an explicit proxy setting or use the system configuration).
- TLS verification on all hosts (do **not** copy Deemix's `rejectUnauthorized: false`).
- Disk-space preflight, filename sanitisation (Windows reserved names, `MAX_PATH`, Unicode
  normalisation), collision policy.
- ARL rotation/expiry, 403/token refresh, preview detection, plan changes.
- Diagnostics: a redacted log viewer with "copy diagnostics".
- Docs: user guide, ARL help, legal notice, supported formats/plans.
- CI: fixtures only, no live network.

### Phase 5 — Paramix-specific value (post-v1)

- Download a Deezer playlist → create a Mixxx playlist or crate (via `PlaylistDAO`).
- "Download missing" from an existing Mixxx playlist.
- Queue downloads into stem separation with a single action ("Download & prepare stems").
- Re-download/upgrade at higher quality when the account plan changes.
- Favorites sync and URL-paste import (`deezer.com/track/...`).
- OS keychain for the ARL (`qtkeychain`).

---

## 10. Testing strategy

**Unit (no network):**

- Crypto vectors: stream path, BF key, stripe decrypt, depad edge cases (short final
  stripe, already-plain stream, MP4 vs FLAC).
- JSON parsing: public and GW payloads, missing fields, renamed fields, empty `data`.
- Path planner: separators, reserved names, emoji/CJK, length limits, collisions,
  padding, various-artists albums.
- Quality selection: plan flags × file sizes × user preference.
- Queue: ordering, concurrency, cancel, retry, partial album failure.

**Integration (local HTTP server + `MockNetworkAccessManager`):**

- Recorded API fixtures replayed; a synthetic encrypted blob served from a local server.
- Atomic write: kill mid-stream, assert no `.flac` appears in the destination.
- Import: a downloaded fixture is inserted, tagged and readable by `SoundSourceProxy`.

**Manual matrix:**

- Free / HQ / HiFi accounts, expired ARL, geo-blocked track, removed track, fallback track,
  30 s preview, Unicode album, very long title, disk full, offline mid-download, proxy/VPN.
- Windows, macOS arm64/x86_64, Linux x64.

**CI:** no live Deezer calls. Fixtures and crypto vectors only.

---

## 11. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Private GW API changes | Downloads break | Isolate in `deezergwclient`, typed parsing, endpoint override setting, fast-fix playbook, track upstream Deemix releases |
| Deezer rate-limits or bans the account/IP | Feature unusable | Token-bucket limiter, default concurrency 3, honour `Retry-After`, no parallel search storms |
| ARL expiry / logout elsewhere | User confusion | Detect 401/403 early, clear "sign in again" state, keep queue intact |
| Geo-blocking | Album partly fails | Per-track errors, optional fallback album/track IDs, clear country message |
| Plan downgrade mid-download | FLAC silently becomes 128 | Re-check plan flags per batch; verify format after write |
| 30 s previews | Broken DJ files | Size+duration+magic verification; hard fail |
| GPLv3 obligations | Project licensing change | Decide before merge; module headers GPL-3.0; update README/NOTICE |
| Deezer ToS / copyright | Distribution/legal exposure | Opt-in, subscriber-only messaging, no bundled credentials, direct distribution, legal review |
| TLS pitfalls | Security regression | Never disable verification; test CDN chains on all platforms |
| Proxy/VPN users | "Network error" reports | System proxy support + explicit proxy setting + diagnostics |
| Long paths/reserved names on Windows | Write failures | Sanitiser + `MAX_PATH` preflight + tests |
| Library scanner picks partial files | Corrupt tracks | `QSaveFile` temp names, final atomic rename |
| Audio/UI jank | Bad DJ experience | Main-thread async first; move to worker thread if profiling demands |

---

## 12. Legal, licensing and policy

This is a product decision, not a code detail.

1. **GPL compatibility.** Deemix is `GPL-3.0-only`; `deezer-sdk` is `GPL-3.0-or-later`.
   Paramix is `GPL-2.0-or-later` (`LICENSE` says "version 2 … or any later version").
   A port is a derivative work and must remain GPL-3.0. Combined with Paramix's "or later"
   clause, distributing the combined work under GPL-3.0 is permitted, but:
   - the new module carries GPL-3.0 headers and attribution;
   - `README.md`/`NOTICE` must state that binaries including the Deezer module are
     GPL-3.0;
   - GPL-3.0 adds patent and anti-tivoisation terms that may matter for signed macOS/
     Windows builds — get a legal read before shipping installers.
   If Paramix must remain GPL-2.0-only, **do not port the code**: implement the protocol
   independently from the documented behaviour above and the public API, and keep the
   implementation clean-room.
2. **Deezer terms of service.** Deezer's terms prohibit downloading and circumventing
   technical protection measures. Deemix is widely used but is not sanctioned by Deezer.
   The feature should be opt-in, require the user's own subscription, never bundle
   credentials, and carry a clear "personal use only" notice. Distribution through the
   Mac App Store / Microsoft Store is likely to be rejected; direct distribution only.
3. **Privacy.** Search and metadata requests go to Deezer; no audio is uploaded. Say so in
   the settings page. Redact the ARL, `license_token`, `track_token` and signed CDN URLs
   from logs and crash reports.
4. **Trademarks.** "Deezer" is a trademark; use it descriptively ("Deezer integration"),
   not in the product name or icon.

---

## 13. Alternatives considered

| Option | Pros | Cons | Verdict |
|---|---|---|---|
| **Native C++ port** | Fast, single binary, real integration, no runtime deps | 6–9 weeks, protocol maintenance | **Recommended** |
| Bundle Node/Deemix as a sidecar | Fastest to prototype; reuses upstream | ~60–100 MB runtime, IPC, packaging/signing, two update streams, security surface | Prototype only |
| Shell out to an installed `deemix` CLI | Days of work | Requires Node + CLI installed, poor UX, no progress/queue integration | Not shippable |
| Embed Deemix JS in `QJSEngine` | Single process | Node APIs (`fs`, `got`, `crypto`, streams) need large shims; ESM/TS build | Not viable |
| Stream from Deezer without downloading | No files | Paramix needs local files for stems/analysis; offline DJ use; ToS worse | No |
| Clean-room protocol reimplementation | GPL-2.0 compatible | Same effort, more protocol reverse-engineering, less upstream leverage | Only if GPL-2.0-only is a hard requirement |

**Hybrid recommendation:** use the CLI/sidecar only as a **Phase 0 spike** to validate the
UX and the account flow, then delete it. Keep the native module as the shipped path.

---

## 14. Decisions needed before implementation

1. **Licence:** accept GPL-3.0 for the combined work, or require a clean-room
   implementation?
2. **Distribution:** is the Deezer module in official Paramix builds, or an optional
   plugin/build flag (`-DDEEZER=ON`)? A build flag is the lowest-risk default.
3. **Auth UX:** ARL paste only (recommended), or also email/password with reCAPTCHA?
4. **Download folder:** default to `<Music>/Paramix Downloads` and auto-add it as a library
   source, or force the user to pick an existing source?
5. **Default quality:** "highest available" vs always FLAC (and fail if the plan lacks it)?
6. **Auto-stems:** off by default (recommended) or on for every download?
7. **Concurrency:** 3 (recommended) vs Deemix's 10.
8. **Search surface:** reuse the library search box (recommended) vs a dedicated field in
   the pane.

---

## 15. Definition of done (v1)

- Signed-out users can search and browse Deezer inside the library pane.
- Sign-in with an ARL shows account name, country and plan; sign-out clears the secret.
- Downloading a track, album or playlist works with progress, cancel, retry, and per-track
  errors.
- Files are tagged, cover art is available, and tracks appear in the library immediately.
- FLAC is downloaded when the account supports it; previews are impossible.
- No partial files ever enter the library; no download causes audio dropouts.
- Settings cover account, quality, folder, filename template, and post-download actions.
- Unit/integration tests run offline in CI; a manual test matrix passes on all three
  platforms.
- Licence, attribution, ToS notice and ARL help are in the app and the repo.

---

## Appendix A — Upstream file map (for porting reference)

| Deemix file | Port to | Notes |
|---|---|---|
| `packages/deezer-sdk/src/api.ts` | `deezerapiclient.cpp` | drop most optional endpoints in v1 |
| `packages/deezer-sdk/src/gw.ts` | `deezergwclient.cpp` | keep `getUserData`, `song.*`, `album.*`, `playlist.*`, `artist.*`, `search.music` |
| `packages/deezer-sdk/src/deezer.ts` | `deezersession.cpp` | ARL path only; skip reCAPTCHA login |
| `packages/deezer-sdk/src/utils.ts` | `deezerjson.cpp` | `mapGwTrackToDeezer`, `map_album` |
| `packages/deemix/src/decryption.ts` | `deezerstreamfilter.cpp` | rewrite as `QIODevice` |
| `packages/deemix/src/utils/crypto.ts` | `deezercrypto.cpp` | vendored AES/Blowfish |
| `packages/deemix/src/downloader.ts` | `deezerdownloadtask.cpp` | drop extras/exec/log files in v1 |
| `packages/deemix/src/utils/pathtemplates.ts` | `deezerpathplanner.cpp` | keep `%artist%`, `%album%`, `%title%`, `%tracknumber%`, `%year%`, `%playlist%` |
| `packages/deemix/src/utils/getPreferredBitrate.ts` | `deezermediaclient.cpp` | simplify to a preference ladder |
| `packages/deemix/src/tagger.ts` | — | **delete**; use TagLib |
| `packages/deemix/src/settings.ts` | `[Deezer]` settings | trim to the fields in §8 |
| `packages/deemix/src/download-objects/*` | `deezerdownloadqueue.cpp` | collapse Single/Collection into one item type |
| `packages/deemix/src/plugins/spotify.ts` | — | defer |
