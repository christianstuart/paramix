# Paramix — C++ Deemix port implementation plan

**Status:** Engineering blueprint v0.1 · 2026-11-15
**Companion to:** [`deezer-integration-plan.md`](deezer-integration-plan.md) (assessment,
portability, licensing, UX). This document is the implementation plan for the native C++
port.
**Scope:** concrete modules, classes, algorithms, integration points, build changes,
milestones and acceptance criteria. No source code was changed to produce this document.

---

## 1. Goals and non-goals

### Goals

1. Let a Deezer subscriber sign in with an ARL, search Deezer from the existing Paramix
   track browser, and download tracks, albums, playlists and artist top-tracks as FLAC or
   MP3.
2. Put downloaded files into the Paramix library immediately, tagged and with cover art.
3. Feed downloads into Paramix's existing analysis and stem-separation pipelines.
4. Keep the integration optional at build time and fully removable.
5. Keep the protocol layer isolated so upstream Deezer changes are a one-file fix.

### Non-goals (v1)

- Streaming playback directly from Deezer.
- Deezer favourites/profile synchronisation.
- Spotify/Apple Music link conversion.
- Email/password login with reCAPTCHA (ARL only).
- Synced lyrics, M3U8 export, post-download shell commands.
- Windows/macOS store distribution.

### Constraints

- Paramix is GPL-2.0-or-later; Deemix is GPL-3.0-only. See §19.
- No new runtime dependency (no Node, no Python, no external `deemix` binary).
- The real-time audio engine must never be blocked or made to allocate.
- Network and file I/O must not block the GUI thread.
- No credential, token or signed URL may reach logs or crash reports.

---

## 2. Architecture overview

```
┌──────────────────────────────────────────────────────────────────────┐
│ QML: res/qml/Deezer/*.qml, res/qml/Settings/Deezer.qml               │
│        Mixxx.Deezer (QmlDeezerProxy), image://deezer/...             │
└───────────────▲──────────────────────────────────────────────────────┘
                │ properties, invokables, models
┌───────────────┴──────────────────────────────────────────────────────┐
│ src/qml/qmldeezerproxy, qmldeezersearchmodel, qmldeezerqueuemodel    │
└───────────────▲──────────────────────────────────────────────────────┘
                │ Qt signals/slots (same thread)
┌───────────────┴──────────────────────────────────────────────────────┐
│ src/deezer/DeezerService                                             │
│   ├── DeezerSession        ARL cookie, account, licence flags        │
│   ├── DeezerApiClient      public api.deezer.com                     │
│   ├── DeezerGwClient       private gw-light.php                      │
│   ├── DeezerMediaClient    media.deezer.com/v1/get_url               │
│   ├── DeezerDownloadQueue  scheduling, concurrency, retry            │
│   ├── DeezerDownloadTask   per-track state machine                   │
│   ├── DeezerTrackResolver  metadata + format availability            │
│   ├── DeezerCrypto         MD5, AES-128-ECB, Blowfish-CBC            │
│   ├── DeezerStreamFilter   QIODevice decrypt + depad                 │
│   ├── DeezerPathPlanner    templates, sanitising, collisions         │
│   ├── DeezerImporter       tags, cover, library insert, hooks        │
│   └── DeezerCache          "already downloaded" index                │
└───────────────▲──────────────────────────────────────────────────────┘
                │ TrackCollectionManager, TagLib, Library, StemService
┌───────────────┴──────────────────────────────────────────────────────┐
│ Existing Paramix library / metadata / analysis / stems               │
└──────────────────────────────────────────────────────────────────────┘
```

**Threading model (v1):** everything above runs on the GUI thread. `QNetworkAccessManager`
is asynchronous; the per-stripe decryption is cheap (2 KB per 6 KB); the audio engine has
its own thread, so downloads cannot cause audio dropouts. If profiling shows GUI jank on
very fast links, move `DeezerDownloadTask` into a dedicated `QThread` with its own
`QNetworkAccessManager`; the queue/task signal boundary is already asynchronous.

**Lifetime:** `DeezerService` is created by `CoreServices` and lives for the process; the
`DeezerFeature` only holds a pointer. QML models are owned by `QmlDeezerProxy` with
`QQmlEngine::CppOwnership` (same pattern as `QmlLibraryTrackListModel`).

---

## 3. Build integration

### 3.1 CMake option

Add near the other feature options in `CMakeLists.txt`:

```cmake
option(DEEZER "Enable the Deezer download integration" OFF)
if(DEEZER)
  target_compile_definitions(mixxx-lib PUBLIC __DEEZER__)
endif()
```

Builds without the option must compile and ship exactly as today. CI builds one extra
Linux job with `-DDEEZER=ON`.

### 3.2 Sources

Add to the `target_sources(mixxx-lib PRIVATE ...)` library block (~line 1471):

```cmake
if(DEEZER)
  target_sources(mixxx-lib PRIVATE
    src/deezer/deezerapiclient.cpp
    src/deezer/deezercache.cpp
    src/deezer/deezercrypto.cpp
    src/deezer/deezerdownloadqueue.cpp
    src/deezer/deezerdownloadtask.cpp
    src/deezer/deezerfeature.cpp
    src/deezer/deezergwclient.cpp
    src/deezer/deezerimporter.cpp
    src/deezer/deezerjson.cpp
    src/deezer/deezermediaclient.cpp
    src/deezer/deezerpathplanner.cpp
    src/deezer/deezerservice.cpp
    src/deezer/deezersession.cpp
    src/deezer/deezerstreamfilter.cpp
    src/deezer/deezertrackresolver.cpp
    src/deezer/deezertypes.cpp
    src/deezer/vendor/aes128.cpp
    src/deezer/vendor/blowfish.cpp
  )
endif()
```

Add the QML proxies to the `mixxx-qml-lib` block (~line 4109) under the same guard:

```cmake
if(DEEZER)
  target_sources(mixxx-qml-lib PRIVATE
    src/qml/qmldeezerproxy.cpp
    src/qml/qmldeezersearchmodel.cpp
    src/qml/qmldeezerqueuemodel.cpp
    src/qml/qmldeezerimageprovider.cpp
  )
  target_compile_definitions(mixxx-qml-lib PUBLIC __DEEZER__)
endif()
```

QML files under `res/qml/` are installed as a directory and loaded from disk; only the
`Mixxx` module C++ types need CMake entries.

### 3.3 Vendored crypto licensing

- AES: use a compact public-domain implementation (`tiny-AES-c` style), keep the original
  notice in `src/deezer/vendor/aes128.h`.
- Blowfish: use a public-domain implementation, keep the notice in
  `src/deezer/vendor/blowfish.h`.
- Both are GPL-compatible (public domain / CC0). Record them in `NOTICE` and the
  third-party license list.

---

## 4. Core data model (`deezertypes.h`)

```cpp
namespace mixxx::deezer {

enum class AudioFormat {
    Unknown = 0,
    Mp3_128 = 1,   // TrackFormats.MP3_128
    Mp3_320 = 3,   // TrackFormats.MP3_320
    Flac = 9,      // TrackFormats.FLAC
    // MP4_RA1/2/3 = 13/14/15 are out of scope for v1.
};

enum class DownloadState {
    Queued,
    Resolving,
    AwaitingUrl,
    Downloading,
    Verifying,
    Tagging,
    Importing,
    Done,
    Failed,
    Cancelled,
};

struct ArtistRef {
    qint64 id = 0;
    QString name;
};

struct AlbumRef {
    qint64 id = 0;
    QString title;
    QString coverMd5;
    QString releaseDate;
    QString label;
    int trackCount = 0;
    int discCount = 0;
    bool variousArtists = false;
};

struct TrackInfo {
    qint64 id = 0;
    QString title;
    QString titleVersion;
    QString isrc;
    int durationSeconds = 0;
    int trackNumber = 0;
    int discNumber = 0;
    int bpm = 0;
    bool explicitLyrics = false;
    QString copyright;
    QString releaseDate;
    QStringList genres;
    QList<ArtistRef> artists;        // main + featured, in order
    QList<ArtistRef> contributors;   // role-qualified in `contributorRoles`
    QStringList contributorRoles;
    AlbumRef album;

    // Private-API fields, only populated after resolve():
    QString md5Origin;          // 32 hex chars
    int mediaVersion = 0;
    QString trackToken;
    QDateTime trackTokenExpiry;
    qint64 sizeFlac = 0;
    qint64 sizeMp3_320 = 0;
    qint64 sizeMp3_128 = 0;
    qint64 fallbackTrackId = 0;
    QList<qint64> alternativeAlbumIds;
};

struct SearchResult {
    enum class Type { Track, Album, Artist, Playlist };
    Type type;
    qint64 id;
    QString title;
    QString subtitle;
    QString coverMd5;
    int durationSeconds = 0;
    int trackCount = 0;
    bool explicitContent = false;
};

struct DownloadError {
    enum class Code {
        None,
        NotSignedIn,
        ArlInvalid,
        ArlExpired,
        WrongPlan,
        GeoBlocked,
        TrackNotAvailable,
        NotEncoded,
        PreviewOnly,
        RateLimited,
        NetworkTimeout,
        DiskFull,
        PermissionDenied,
        PathTooLong,
        TaggingFailed,
        ImportFailed,
        Cancelled,
        Unknown,
    };
    Code code = Code::None;
    QString detail;
    bool retryable = false;

    static DownloadError fromHttp(int status, const QJsonObject& body);
    QString userMessage() const;   // translated in the UI layer
};

} // namespace mixxx::deezer
```

All parsing lives in `deezerjson.{h,cpp}` and must tolerate missing/renamed fields:
unknown keys are ignored, absent numbers become 0, absent strings become empty, and a
malformed required field produces `DownloadError::Unknown` rather than a crash.

---

## 5. Network layer

### 5.1 Shared HTTP client

`DeezerHttpClient` wraps one `QNetworkAccessManager` owned by `DeezerService`:

```cpp
class DeezerHttpClient : public QObject {
    Q_OBJECT
  public:
    struct Request {
        QNetworkAccessManager::Operation method;
        QUrl url;
        QByteArray body;
        QByteArray contentType;
        int timeoutMs = 30000;
        int maxAttempts = 3;
        bool authenticated = true;
    };

    explicit DeezerHttpClient(QObject* parent = nullptr);

    void getJson(const QUrl& url, std::function<void(const QJsonDocument&)> onOk,
            std::function<void(const DownloadError&)> onError);
    void postJson(const QUrl& url, const QJsonObject& body,
            std::function<void(const QJsonDocument&)> onOk,
            std::function<void(const DownloadError&)> onError);

    QNetworkAccessManager* nam() const;
    void setArlCookie(const QString& arl);
    void clearCookies();

  private:
    void send(Request request, int attempt, JsonCallback ok, ErrorCallback err);
    void backoff(int attempt);

    QNetworkAccessManager m_nam;   // owns its QNetworkCookieJar
    QTimer m_rateTimer;
    int m_tokens = 8;              // token bucket
};
```

Requirements:

- `QNetworkCookieJar` is enabled on the QNAM. The ARL is set as a cookie on
  `.deezer.com` (`path=/`, `httpOnly=true`) exactly like Deemix.
- Send a desktop browser `User-Agent`; make it a single constant so it can be updated.
- **Never** call `QNetworkRequest::setAttribute(... ignoreSslErrors)` or connect to
  `sslErrors`. Deemix disables certificate verification; Paramix must not.
- Retry only on network errors, timeouts and 5xx. Use exponential backoff with jitter
  (250 ms, 1 s, 4 s) and honour `Retry-After` on 429.
- Rate limit: token bucket of 8 requests/second with a burst of 8; a 429 drains the bucket
  and pauses for `Retry-After`.
- Every log line passes through `redactForLog()` (§14).

### 5.2 Session and login

```cpp
class DeezerSession : public QObject {
    Q_OBJECT
  public:
    enum class State { SignedOut, SigningIn, SignedIn, Expired };

    void signInWithArl(const QString& arl);
    void signOut();
    void refreshAccount();

    State state() const;
    QString accountName() const;
    QString country() const;
    bool canStreamLossless() const;
    bool canStreamHq() const;
    QString licenseToken() const;   // never logged

  signals:
    void stateChanged();
    void accountChanged();
    void signInFailed(const DownloadError& error);

  private:
    void handleUserData(const QJsonObject& results);
    DeezerHttpClient* m_http;
    QString m_licenseToken;
    QString m_checkForm;
    bool m_canStreamLossless = false;
    bool m_canStreamHq = false;
    State m_state = State::SignedOut;
};
```

Flow:

1. Set the ARL cookie.
2. `POST gw-light.php?method=deezer.getUserData&api_version=1.0&api_token=null&input=3`.
3. If `USER.USER_ID == 0` → `ArlInvalid`.
4. Store `checkForm`, `license_token`, `web_lossless || mobile_lossless`,
   `web_hq || mobile_hq`, `license_country`, account name.
5. Persist the ARL only if "Remember me" is enabled (§11).
6. Any gateway call returning `{"GATEWAY_ERROR":"invalid api token"}` or
   `{"VALID_TOKEN_REQUIRED":"Invalid CSRF token"}` triggers one token refresh and a retry.

### 5.3 Public API client

`DeezerApiClient` exposes only what the UI needs in v1:

| Method | Endpoint |
|---|---|
| `searchTracks(q, index, limit)` | `GET /search/track` |
| `searchAlbums(q, index, limit)` | `GET /search/album` |
| `searchArtists(q, index, limit)` | `GET /search/artist` |
| `searchPlaylists(q, index, limit)` | `GET /search/playlist` |
| `track(id)` | `GET /track/{id}` |
| `album(id)` | `GET /album/{id}` |
| `albumTracks(id)` | `GET /album/{id}/tracks` |
| `artist(id)` | `GET /artist/{id}` |
| `artistTop(id, limit)` | `GET /artist/{id}/top` |
| `artistAlbums(id)` | `GET /artist/{id}/albums` |
| `playlist(id)` | `GET /playlist/{id}` |
| `playlistTracks(id)` | `GET /playlist/{id}/tracks` |

No authentication is required. Rate-limit through the shared `DeezerHttpClient`.

### 5.4 Private gateway client

`DeezerGwClient` owns the `api_token` (`checkForm`) and sends JSON bodies:

```
POST https://www.deezer.com/ajax/gw-light.php
     ?api_version=1.0&api_token=<checkForm>&input=3&method=<method>
Content-Type: application/json
```

v1 methods:

| Method | Purpose | Key response fields |
|---|---|---|
| `deezer.getUserData` | session | `checkForm`, `USER.OPTIONS.*` |
| `song.getData` | track detail | `MD5_ORIGIN`, `MEDIA_VERSION`, `TRACK_TOKEN`, `TRACK_TOKEN_EXPIRE`, `FILESIZE_*`, `FALLBACK` |
| `deezer.pageTrack` | fallback detail page | `DATA`, `LYRICS` |
| `album.getData` | album detail | disc counts, label, contributors |
| `song.getListByAlbum` | album tracks | ordered `data[]` |
| `playlist.getSongs` | playlist tracks | ordered `data[]` |
| `artist.getTopTrack` | artist top | ordered `data[]` |
| `song.getLyrics` | optional | not used in v1 |
| `song.getFavoriteIds` | optional | not used in v1 |

Design rule: **every gateway method maps to one small function** that extracts only the
fields Paramix uses and returns typed structs. If Deezer renames a field, only that
function changes. Do not pass raw `QJsonObject` beyond `deezerjson.cpp`.

### 5.5 Media URL client

```cpp
class DeezerMediaClient {
  public:
    struct ResolvedUrl {
        QUrl url;
        AudioFormat format;
        bool crypted = false;   // /mobile/ or /media/ path
    };

    // Tries the requested format, then lower formats if allowFallback.
    std::optional<ResolvedUrl> resolve(const TrackInfo& track,
            AudioFormat preferred,
            bool allowFallback,
            DownloadError* error);
};
```

Request body:

```json
{
  "license_token": "<session token>",
  "media": [{ "type": "FULL",
              "formats": [{ "cipher": "BF_CBC_STRIPE", "format": "FLAC" }] }],
  "track_tokens": ["<track token>"]
}
```

Rules:

- Format names: `FLAC`, `MP3_320`, `MP3_128`.
- Reject a format when the account flag is false; report `WrongPlan` with a specific
  message ("FLAC requires a Deezer HiFi subscription").
- Error `2002` in `data[].errors[]` → `GeoBlocked` with the account country.
- Missing `media`/`sources` → `TrackNotAvailable`.
- `FILESIZE_* == 0` means the format is unavailable; do not prefer it.
- If `allowFallback` is false, fail rather than silently downgrade.
- A track is a preview if the size is implausibly small or the duration after download
  differs by more than 2 s. Enforce in the verification step (§8.4).

---

## 6. Cryptography

Namespace `mixxx::deezer::crypto`, header `deezercrypto.h`.

### 6.1 API

```cpp
QByteArray md5Raw(QByteArrayView data);          // 16 bytes
QString md5Hex(QByteArrayView data);             // lowercase

QByteArray aes128EcbEncrypt(QByteArrayView key, QByteArrayView data); // no padding
QByteArray aes128EcbDecrypt(QByteArrayView key, QByteArrayView data); // no padding

QByteArray blowfishCbcDecrypt(QByteArrayView key, QByteArrayView iv, QByteArrayView data);

QByteArray blowfishKeyForTrack(qint64 trackId);
QString streamPath(const TrackInfo& track, AudioFormat format);
QUrl streamUrl(const TrackInfo& track, AudioFormat format, bool crypted);

// The decryption primitive used by DeezerStreamFilter.
QByteArray decryptStripe(QByteArrayView stripe, QByteArrayView blowfishKey);
```

### 6.2 Stream path

```
urlPart = md5Origin + "¤" + formatName + "¤" + QString::number(trackId)
        + "¤" + QString::number(mediaVersion)
step1   = md5Hex(urlPart)
step2   = step1 + "¤" + urlPart + "¤"
step2  += QString(16 - (step2.size() % 16), QLatin1Char('.'))
path    = lowercaseHex(aes128EcbEncrypt("jo6aey6haid2Teih", utf8(step2)))
url     = "https://cdns-proxy-" + md5Origin[0] + ".dzcdn.net/api/1/" + path
        // legacy/fallback: "https://e-cdns-proxy-" + md5Origin[0] + ".dzcdn.net/mobile/1/" + path
```

Note: `step2.size()` is a byte count of the UTF-8 string. `¤` is U+00A4, two bytes in
UTF-8, which matters for the padding calculation. Use `QByteArray` throughout, never
`QString::size()`.

### 6.3 Blowfish key

```
md5id = md5Hex(asciiDecimal(trackId))          // 32 ASCII hex chars
key[i] = md5id[i] XOR md5id[i + 16] XOR "g4el58wc0zvf9na1"[i]   for i = 0..15
```

`md5Hex` returns lowercase ASCII, so `md5id[i]` is the byte value of the hex character,
not the nibble. This is the same quirk as the reference implementation; test it.

### 6.4 Stripe decryption

Input is a byte stream where each 6144-byte stripe is:

```
[ 2048 bytes Blowfish-CBC ciphertext ][ 4096 bytes plaintext ]
```

Decrypt the first 2048 bytes with Blowfish-CBC, IV `{0,1,2,3,4,5,6,7}`, no padding, then
append the remaining 4096 bytes unchanged. The final stripe may be shorter: if it is
≥ 2048 bytes, decrypt its first 2048; otherwise pass it through.

Implementation notes:

- Blowfish-CBC is stateless per stripe: re-initialise the IV for every stripe. Do **not**
  chain the IV across stripes.
- `decryptStripe` must handle a 2048-byte input exactly (a multiple of the 8-byte block
  size) and must not apply PKCS padding.
- For `flac` streams, the first decrypted bytes may be zero padding; strip until the
  container magic (`fLaC`) or, for MP4, until bytes 4..7 equal `ftyp`.

### 6.5 `DeezerStreamFilter`

```cpp
class DeezerStreamFilter : public QIODevice {
    Q_OBJECT
  public:
    DeezerStreamFilter(QIODevice* source, QByteArray blowfishKey, QObject* parent = nullptr);
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

  protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override { return -1; }

  private:
    bool fillFromSource();
    void decryptAvailable();
    void stripInitialPadding();

    QIODevice* m_source;            // not owned
    QByteArray m_key;
    QByteArray m_cipherBuffer;      // unprocessed bytes
    QByteArray m_plainBuffer;       // decrypted, not yet read
    bool m_start = true;
    bool m_plainStream = false;     // no /mobile/ or /media/ path
};
```

Contract:

- Sequential, read-only.
- Accumulate source bytes until at least 6144 are buffered (or the source is at EOF),
  decrypt complete stripes, leave the remainder buffered.
- At EOF, decrypt the final partial stripe as described above.
- Strip leading zero padding once, at the very start.
- Never allocate per byte; reuse buffers and `reserve()` in 64 KB increments.
- `readData` copies from `m_plainBuffer` and pulls more from the source as needed.

Unit tests should feed the filter a synthetic encrypted stream in 1-byte, 7-byte, 2048-byte,
6144-byte and 100 KB chunks and assert byte-identical output.

---

## 7. Track resolution

`DeezerTrackResolver` turns a search result into a complete `TrackInfo`:

```cpp
class DeezerTrackResolver {
  public:
    struct Result {
        TrackInfo track;
        DownloadError error;
        bool ok() const { return error.code == DownloadError::Code::None; }
    };

    // Public API first for BPM/genres, then GW for MD5/token/sizes.
    Result resolve(qint64 trackId);
    QList<TrackInfo> resolveAlbum(qint64 albumId);
    QList<TrackInfo> resolvePlaylist(qint64 playlistId);
    QList<TrackInfo> resolveArtistTop(qint64 artistId, int limit);
};
```

Steps:

1. `gw.song.getData(trackId)` → essential fields. If `SNG_ID == 0` or `MD5_ORIGIN` is
   empty → `TrackNotAvailable`.
2. `api.track(trackId)` for BPM, genres, contributors, release date. Failure is
   non-fatal.
3. `gw.album.getData(albumId)` for disc count/label. Failure is non-fatal.
4. Merge with the search result (title, artist, album, duration, cover).
5. If `TRACK_TOKEN_EXPIRE` is in the past, re-fetch once.
6. If the album has no cover MD5, fetch the artist picture as a fallback.

Resolution is cached in memory for 10 minutes keyed by track ID, because opening an album
detail view and then downloading it triggers the same calls twice.

---

## 8. Download pipeline

### 8.1 State machine

```
                 ┌──────────┐
 enqueue ───────▶│  Queued  │
                 └────┬─────┘
                      ▼
                 ┌──────────┐   resolve error   ┌────────┐
                 │ Resolving├──────────────────▶│ Failed │
                 └────┬─────┘                   └────┬───┘
                      ▼                              │ retry
                 ┌──────────┐                        │
                 │AwaitingUrl│◀──────────────────────┘
                 └────┬─────┘
                      ▼
                 ┌──────────┐  network error   ┌────────┐
                 │Downloading├─────────────────▶│ Failed │
                 └────┬─────┘                   └────────┘
                      ▼
                 ┌──────────┐  size/duration mismatch
                 │ Verifying├──────────────────▶ Failed (PreviewOnly)
                 └────┬─────┘
                      ▼
                 ┌──────────┐
                 │ Tagging  │
                 └────┬─────┘
                      ▼
                 ┌──────────┐
                 │Importing │
                 └────┬─────┘
                      ▼
                 ┌──────────┐
                 │   Done   │
                 └──────────┘

Any state ──cancel()──▶ Cancelling ──▶ Cancelled (delete temp file)
```

### 8.2 `DeezerDownloadTask`

```cpp
class DeezerDownloadTask : public QObject {
    Q_OBJECT
  public:
    struct Options {
        AudioFormat preferred = AudioFormat::Flac;
        bool allowFallback = true;
        bool embedCover = true;
        bool saveSidecarCover = true;
        bool autoAnalyze = true;
        bool autoSeparateStems = false;
        QString downloadRoot;
        QString pathTemplate;
        OverwritePolicy overwrite = OverwritePolicy::Skip;
    };

    DeezerDownloadTask(const TrackInfo& track, const Options& options,
            DeezerSession* session, DeezerMediaClient* media,
            DeezerHttpClient* http, DeezerImporter* importer, QObject* parent);

    void start();
    void cancel();

    QUuid id() const;
    DownloadState state() const;
    qint64 bytesReceived() const;
    qint64 bytesTotal() const;
    DownloadError error() const;

  signals:
    void stateChanged(DownloadState state);
    void progress(qint64 received, qint64 total);
    void finished(const QString& filePath, const DownloadError& error);
};
```

Start sequence:

1. Check the target file and overwrite policy (§10). If it exists and the policy is
   `Skip`, transition to `Done` with the existing path.
2. Resolve the media URL.
3. Build the final path with `DeezerPathPlanner`.
4. `QSaveFile` on the final path; `QNetworkRequest` with the standard UA.
5. `QNetworkReply::readyRead` → `DeezerStreamFilter::write`? No: the filter reads from the
   reply. Use `QIODevice* filter = new DeezerStreamFilter(reply, key, this)` and copy
   `filter->readAll()` into the `QSaveFile` in a loop.
6. On `finished`: flush, `commit()` (atomic rename), verify, tag, import.
7. Emit `finished`.

Important details:

- `QSaveFile` writes a temporary file with no audio extension, so the library scanner
  cannot see it. `commit()` is atomic on the same filesystem.
- Set `QNetworkRequest::RedirectPolicyAttribute` to
  `QNetworkRequest::NoLessSafeRedirectPolicy`; CDN URLs redirect.
- Track `Content-Length` for progress; if absent, use `FILESIZE_*` from the resolve step
  and clamp progress to 100%.
- On any failure, `cancelWriting()` and remove the temp file.
- On cancel, abort the reply, delete the temp file, emit `Cancelled`.
- Timeout: 30 s of no bytes → `NetworkTimeout`; abort and retry up to 3 times.

### 8.3 `DeezerDownloadQueue`

```cpp
class DeezerDownloadQueue : public QObject {
    Q_OBJECT
  public:
    explicit DeezerDownloadQueue(DeezerService* service, QObject* parent = nullptr);

    QUuid enqueueTrack(qint64 trackId, AudioFormat preferred);
    QUuid enqueueAlbum(qint64 albumId, AudioFormat preferred);
    QUuid enqueuePlaylist(qint64 playlistId, AudioFormat preferred);
    QUuid enqueueArtistTop(qint64 artistId, int limit, AudioFormat preferred);

    void cancel(QUuid id);
    void cancelAll();
    void retry(QUuid id);
    void pause();
    void resume();

    int activeCount() const;
    int pendingCount() const;

  signals:
    void itemAdded(QUuid id, const QString& title, const QString& subtitle);
    void itemProgress(QUuid id, qint64 received, qint64 total);
    void itemStateChanged(QUuid id, DownloadState state);
    void itemFinished(QUuid id, const QString& path, const DownloadError& error);
    void queueIdle();
};
```

Queue rules:

- Default concurrency 3 (configurable 1–10).
- Resolve at most 4 tracks in parallel, download at most `Concurrency`.
- Collection items expand into tracks and track aggregate progress (completed / total,
  plus per-track detail for the UI).
- A failed track does not abort its album; the album ends with `partial` status.
- Deduplicate by Deezer track ID while queued; completed downloads are checked against
  `DeezerCache`.
- Pause stops dequeuing new work but lets in-flight tracks finish or abort (configurable;
  default abort and requeue).
- Queue state is in memory only; on app restart, unfinished items are dropped, but the
  temp files are cleaned up by a startup sweep of `*.part`/temp names.

### 8.4 Verification

After `QSaveFile::commit()`:

1. Open the file with `SoundSourceProxy` (or `mixxx::audio::StreamInfo`).
2. Compare duration with `TrackInfo::durationSeconds`; fail if `|diff| > 2 s`.
3. Check the container magic: `fLaC` for FLAC, `ID3`/`0xFF 0xFB` for MP3.
4. Check the file size against `FILESIZE_*`; a factor below ~0.5 is a preview.
5. On failure: delete the file and emit `PreviewOnly` (or `TaggingFailed` if step 1
   failed).

Do the check **before** tagging so a broken file never enters the library.

### 8.5 Progress and ETA

- `bytesReceived / bytesTotal` per task; aggregate per collection.
- UI shows `received`, `total`, `percent`, and a smoothed bytes/second estimate over a
  2 s window.
- No per-chunk signals at a rate that could flood the GUI: coalesce to at most 10 Hz.

---

## 9. Metadata, tags and cover art

### 9.1 Mapping

| Deezer field | Paramix field |
|---|---|
| `SNG_TITLE` / `title` | `TrackInfo::title` |
| `ART_NAME` / `artist.name` | `TrackInfo::artist` (main), `artists` (all) |
| `ALB_TITLE` / `album.title` | `AlbumInfo::title` |
| `TRACK_NUMBER` / `track_position` | `TrackInfo::trackNumber` |
| `DISK_NUMBER` / `disk_number` | `TrackInfo::discNumber` |
| `DURATION` | `StreamInfo` duration (after open) |
| `BPM` | `TrackInfo::bpm` (analysis refines it) |
| `ISRC` | `TrackInfo::isrc` |
| `PHYSICAL_RELEASE_DATE` / `release_date` | `TrackInfo::date` / `AlbumInfo::date` |
| `GENRE_ID` / `genres` | `TrackInfo::genre` |
| `COPYRIGHT` | `TrackInfo::copyright` |
| `TRACK_NUMBER` totals | `TrackInfo::trackTotal`, `discTotal` |
| `contributors` | `TrackInfo::composer`, `TrackInfo::grouping`, comments |
| cover MD5 | `CoverInfoRelative` / embedded image |

### 9.2 Writing tags

Use the existing `mixxx::MetadataSourceTagLib`:

```cpp
mixxx::TrackMetadata metadata;
metadata.setTrackInfo(trackInfo);
metadata.setAlbumInfo(albumInfo);
mixxx::MetadataSourceTagLib source(filePath, fileType);
source.exportTrackMetadata(metadata);
```

- Do **not** port `tagger.ts`.
- For cover art, Mixxx's export path does not embed images, so add a small
  `DeezerCoverWriter` helper (150–200 lines) that uses TagLib directly:
  - FLAC: add a `METADATA_BLOCK_PICTURE` block (type 3, front cover) via
    `TagLib::FLAC::File::addPicture`.
  - MP3: add an ID3v2 `APIC` frame.
  - MP4: add a `covr` atom (only if MP4 is enabled later).
- Also write a `cover.jpg` sidecar when `SaveSidecarCover` is enabled; Mixxx's
  `CoverArtUtils` picks it up with no extra work.
- Cover size: 800×800 for embedding, 1200×1200 for the sidecar (Deemix defaults).
- Cover URL: `https://e-cdn-images.dzcdn.net/images/cover/<md5>/<size>x<size>-000000-80-0-0.jpg`.

### 9.3 Filename and folder planning

`DeezerPathPlanner`:

```cpp
struct PathPlan {
    QString absoluteFilePath;
    QString albumDirectory;
    QString coverPath;
    bool collision = false;
};

PathPlan plan(const TrackInfo& track, const Options& options) const;
static QString sanitiseComponent(const QString& input);
```

Template tokens for v1:

| Token | Value |
|---|---|
| `%artist%` | main artist |
| `%albumartist%` | album artist |
| `%album%` | album title |
| `%title%` | track title |
| `%tracknumber%` | zero-padded track number |
| `%discnumber%` | disc number |
| `%year%` | release year |
| `%isrc%` | ISRC |
| `%playlist%` | playlist name (collection downloads) |

Default template: `%artist%/%album%/%tracknumber% - %title%`.

Sanitising rules:

- Replace `/ \ : * ? " < > |` and control characters with `_`.
- Trim trailing dots/spaces (Windows).
- Reject/rename Windows reserved names (`CON`, `PRN`, `AUX`, `NUL`, `COM1..9`,
  `LPT1..9`).
- Enforce a component length of 120 UTF-8 characters and a full path length below 250
  characters on Windows; if exceeded, shorten the title component, never the extension.
- Normalise Unicode to NFC on macOS to avoid duplicate files.
- Handle empty components ("Unknown Artist", "Unknown Album").
- Overwrite policies: `Skip`, `Overwrite`, `KeepBoth` (`name (1).flac`),
  `OnlyTags`, `OnlyLowerBitrates`.

### 9.4 Library import

`DeezerImporter`:

```cpp
class DeezerImporter {
  public:
    struct Result {
        TrackId trackId;
        DownloadError error;
    };
    Result importFile(const QString& path, const TrackInfo& info);
};
```

Steps:

1. `m_pTrackCollectionManager->resolveTrackIdsFromUrls({QUrl::fromLocalFile(path)}, true)`.
   This adds the file to the collection even if its folder is not a watched source.
2. If the track was added, optionally `Library::scheduleAnalysis({track})`.
3. If `AutoSeparateStems` and the track is analysed, call
   `mixxx::stems::StemService::queueExport(...)`. Only when the stem feature is compiled
   and the model is installed; otherwise show a one-time hint.
4. Update `DeezerCache` with `{trackId, isrc, path, format, timestamp}`.
5. Emit `imported(trackId, path)`.

Ensure the download directory is a library source: on first use, if it is not already a
root directory, ask once and call `Library::requestAddDir()`. This is a UX affordance, not
a hard requirement, because step 1 works regardless.

---

## 10. Persistence

### 10.1 ARL

- Default: memory only (the user signs in per session).
- "Remember me": write `<settings>/deezer-arl` with `QFile::setPermissions(ReadOwner |
  WriteOwner)`; never write it to `mixxx.cfg`.
- Never log it. The diagnostics page shows `<redacted>`.
- On sign-out, delete the file.
- v1.1: replace with OS keychain via `qtkeychain` (macOS Keychain, Windows Credential
  Manager, libsecret).

### 10.2 Download index

`DeezerCache` is a small JSON document at `<settings>/deezer-downloads.json`:

```json
{
  "version": 1,
  "tracks": {
    "3135556": {
      "isrc": "GBDUW0000059",
      "path": "/Music/Daft Punk/Discovery/01 - One More Time.flac",
      "format": "flac",
      "downloadedAt": "2026-11-15T18:22:01Z"
    }
  }
}
```

Used for "already downloaded", skipping, and quality upgrades. Load at startup, save
debounced (2 s) and atomically (`QSaveFile`). A SQLite table is a later optimisation if the
file grows beyond a few thousand entries.

### 10.3 Settings keys

```ini
[Deezer]
Enabled=true
RememberArl=false
DownloadDir=
Quality=auto
AllowFallback=true
Concurrency=3
OverwritePolicy=skip
PathTemplate=%artist%/%album%/%tracknumber% - %title%
EmbedCover=true
SaveSidecarCover=true
AutoAnalyze=true
AutoSeparateStems=false
RequestDelayMs=250
FeelingLucky=false
```

Expose them through `QmlConfigProxy` with the same `PROPERTY_IMPL_GETTER` pattern used for
the stem settings.

---

## 11. Library and UI integration

### 11.1 The `viewId` seam

Add to `LibraryFeature` (`src/library/libraryfeature.h`):

```cpp
/// Identifier of a custom QML pane. Empty means "show the track table".
virtual QString viewId() const {
    return {};
}
```

`DeezerFeature::viewId()` returns `"deezer"`.

`QmlSidebarModelProxy` gains:

```cpp
Q_PROPERTY(QString currentView READ currentView NOTIFY currentViewChanged)

QString currentView() const { return m_currentView; }
void setCurrentView(const QString& view);

signals:
    void currentViewChanged();

void QmlSidebarModelProxy::activate(const QModelIndex& index) {
    if (!index.isValid() || index.model() != this) {
        return;
    }
    if (index.internalPointer() == this) {
        setCurrentView(m_sFeatures[index.row()]->viewId());
    }
    SidebarModel::clicked(index);
}

void QmlSidebarModelProxy::slotShowTrackModel(QAbstractItemModel* pModel) {
    setCurrentView({});   // a track table is being shown
    ...
}
```

`res/qml/Library.qml` becomes:

```qml
Item {
    id: trackBrowserPane
    StackLayout {
        anchors.fill: parent
        currentIndex: root.sidebar.currentView === "deezer" ? 1 : 0
        LibraryComponent.TrackList { model: root.sidebar.tracklist }
        LibraryComponent.DeezerPane { }
    }
    Skin.TaskStatusStrip { ... }
}
```

Keep the task status strip below the stack so downloads and stem jobs share one status
area.

### 11.2 Search routing

Change `Library.qml::applySearch()` to call `root.sidebar.search(text)` instead of
`root.sidebar.tracklist.search(text)`, and extend the proxy:

```cpp
void QmlSidebarModelProxy::search(const QString& searchText) {
    setSearchText(searchText);
#ifdef __DEEZER__
    if (m_currentView == QLatin1String("deezer")) {
        if (auto* pDeezer = QmlDeezerProxy::instance()) {
            pDeezer->search(searchText);
            return;
        }
    }
#endif
    if (m_pTracklist) {
        m_pTracklist->search(searchText);
    }
}
```

Do not re-apply the local search while the Deezer pane is active. When the user switches
back to "All tracks", `tracklistChanged` triggers `applySearch()` and the local query is
restored, unchanged.

### 11.3 `DeezerFeature`

```cpp
class DeezerFeature : public LibraryFeature {
    Q_OBJECT
  public:
    DeezerFeature(Library* pLibrary, UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);
    ~DeezerFeature() override;

    QVariant title() override;                 // tr("Deezer")
    QString viewId() const override;           // "deezer"
    TreeItemModel* sidebarModel() const override { return nullptr; }
    bool hasTrackTable() override { return false; }

  public slots:
    void activate() override;                  // ensure the service exists
};
```

Register it in `Library::Library` after `AnalysisFeature`:

```cpp
#ifdef __DEEZER__
    if (m_pConfig->getValue(ConfigKey("[Deezer]", "Enabled"), true)) {
        addFeature(make_parented<DeezerFeature>(
                this, m_pConfig, m_pTrackCollectionManager));
    }
#endif
```

The feature does not emit `showTrackModel`; `currentView` selects the QML pane.

### 11.4 `QmlDeezerProxy`

```cpp
class QmlDeezerProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Deezer)
    QML_SINGLETON

    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
    Q_PROPERTY(QString country READ country NOTIFY accountChanged)
    Q_PROPERTY(bool canStreamLossless READ canStreamLossless NOTIFY accountChanged)
    Q_PROPERTY(bool canStreamHq READ canStreamHq NOTIFY accountChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QmlDeezerSearchModel* results READ results CONSTANT)
    Q_PROPERTY(QmlDeezerQueueModel* queue READ queue CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  public:
    static QmlDeezerProxy* create(QQmlEngine*, QJSEngine*);
    static QmlDeezerProxy* instance();

    Q_INVOKABLE void signIn(const QString& arl);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void search(const QString& text);
    Q_INVOKABLE void searchMore();
    Q_INVOKABLE void setResultType(int type);       // Track/Album/Artist/Playlist
    Q_INVOKABLE void openAlbum(qint64 id);
    Q_INVOKABLE void openArtist(qint64 id);
    Q_INVOKABLE void openPlaylist(qint64 id);
    Q_INVOKABLE void downloadTrack(qint64 id);
    Q_INVOKABLE void downloadAlbum(qint64 id);
    Q_INVOKABLE void downloadPlaylist(qint64 id);
    Q_INVOKABLE void downloadArtistTop(qint64 id);
    Q_INVOKABLE void cancel(QUuid id);
    Q_INVOKABLE void retry(QUuid id);
    Q_INVOKABLE void revealInLibrary(qint64 trackId);
    Q_INVOKABLE void openHelp();
};
```

Models:

```cpp
class QmlDeezerSearchModel : public QAbstractListModel {
    // roles: title, subtitle, coverUrl, duration, type, deezerId, quality,
    //        downloadState, explicit
};

class QmlDeezerQueueModel : public QAbstractListModel {
    // roles: title, subtitle, progress, state, errorText, bytesReceived,
    //        bytesTotal, speed, etaSeconds, path
};
```

Both reset on new searches and use `beginInsertRows` for paging. Never delete a model
exposed to QML; give it `CppOwnership`.

### 11.5 Cover image provider

`DeezerImageProvider : QQuickAsyncImageProvider`, registered in `qmlapplication.cpp`
next to `AsyncImageProvider`:

```cpp
auto* pDeezerImages = new DeezerImageProvider(service);
m_pAppEngine->addImageProvider(QStringLiteral("deezer"), pDeezerImages);
```

URL shape: `image://deezer/<md5>/<size>` (e.g. `image://deezer/abc123/250`). The response:

- returns from an in-memory `QCache<QString, QImage>` (default 64 MB),
- otherwise fetches `https://e-cdn-images.dzcdn.net/images/cover/<md5>/<size>x<size>-000000-80-0-0.jpg`
  on a worker thread and caches the decoded image,
- returns a neutral placeholder while loading or on failure.

Do not use the library's `mixxx` provider for remote art; it reads only local files.

### 11.6 QML screens

```
res/qml/Deezer/DeezerPane.qml       StackView, sign-in gate, queue drawer
res/qml/Deezer/SignInPanel.qml      ARL field, help, plan badges
res/qml/Deezer/SearchView.qml       search field, type tabs, result list/grid, paging
res/qml/Deezer/AlbumView.qml        cover header, track table, Download all
res/qml/Deezer/ArtistView.qml       top tracks + albums
res/qml/Deezer/PlaylistView.qml     cover header, track table, Download all
res/qml/Deezer/TrackRow.qml         title/artist/duration/quality/state + action
res/qml/Deezer/QueueDrawer.qml      active/completed/failed, global progress
res/qml/Deezer/ErrorState.qml       retry + explanation
res/qml/Settings/Deezer.qml         Account / Download / After download / Advanced
```

UI requirements:

- Use `Theme` tokens only (`Theme.accent`, `Theme.surface*`, `Theme.space*`,
  `Theme.fontSize*`, `Theme.radius*`), matching `Library/Browser.qml`.
- Every interactive control has `Accessible.name` and `Accessible.role`.
- The result list is keyboard-navigable; Enter downloads, Space previews metadata.
- Empty, loading, signed-out, offline and geo-blocked states are distinct.
- No blocking dialogs; the queue is visible and cancellable.

---

## 12. Concurrency and lifetime rules

1. All `Deezer*` objects live on the GUI thread. `QNetworkAccessManager` is created there
   and used there.
2. Never call `QCoreApplication::processEvents()` while downloading.
3. Do not block on `QNetworkReply::waitForReadyRead()`; drive everything from signals.
4. `DeezerDownloadQueue` owns `DeezerDownloadTask` objects; tasks are deleted with
   `deleteLater()` after `finished`.
5. Track metadata writes go through `TrackCollectionManager` on the GUI thread. Never
   touch `Track` objects from the image-provider thread.
6. The image provider's worker uses its own `QNetworkAccessManager` or a thread-safe
   `QNetworkDiskCache`; it must never call into `DeezerService`.
7. On shutdown, `CoreServices` calls `DeezerService::shutdown()`: cancel the queue, abort
   replies, delete temp files, flush settings/cache.

---

## 13. Error handling and UX mapping

| Code | Trigger | User-facing action |
|---|---|---|
| `NotSignedIn` | download attempted while signed out | "Sign in to Deezer" button |
| `ArlInvalid` | `USER_ID == 0` | Re-enter the ARL, link to help |
| `ArlExpired` | 401/403 on `<redacted>` cookie refresh | "Session expired, sign in again" |
| `WrongPlan` | `can_stream_lossless/hq == false` | "FLAC requires Deezer HiFi" + quality selector |
| `GeoBlocked` | media error 2002 | `Not available in <country>` |
| `TrackNotAvailable` | missing MD5/source | Skip and continue the album |
| `NotEncoded` | `MD5 == 0` | Skip and continue |
| `PreviewOnly` | size/duration check failed | Delete file, explain, do not import |
| `RateLimited` | HTTP 429 | Automatic retry with countdown |
| `NetworkTimeout` | 30 s no bytes | Retry ×3, then "Check your connection" |
| `DiskFull` | `QFile::write` ENOSPC | Free space, retry |
| `PermissionDenied` | EACCES | Choose another folder |
| `PathTooLong` | preflight | Shorten the template |
| `TaggingFailed` | TagLib error | File kept, library entry added without tags |
| `ImportFailed` | `resolveTrackIdsFromUrls` failed | "Downloaded, but could not add to library" |
| `Cancelled` | user | Neutral; offer retry |

Queue-level errors (album partially failed) show "12 of 14 tracks downloaded" with a
per-track detail list, never a modal.

---

## 14. Security and privacy

- **TLS:** verify certificates everywhere. Do not copy Deemix's
  `rejectUnauthorized: false`. Add a test that a bad certificate fails.
- **Secrets:** ARL, `checkForm`, `license_token`, `TRACK_TOKEN`, signed CDN URLs are
  secret. Implement:

  ```cpp
  QString redactForLog(const QString& value);
  QJsonObject redactJson(const QJsonObject& object);
  ```

  Redact values shorter than 8 characters entirely, otherwise keep the first 3 and last 2
  characters.
- **Logging:** `kLogger` at `info` for lifecycle, `debug` for request method/path only
  (never query strings with tokens). `qWarning` on retries without URLs.
- **Diagnostics:** a "Copy diagnostics" button exports app version, platform, account
  plan, endpoint hostnames, error codes and redacted logs. No cookies, no URLs with
  signatures.
- **Network exposure:** search and metadata go to `api.deezer.com`, `www.deezer.com` and
  `media.deezer.com`; audio comes from `*.dzcdn.net`. Document this in the settings page.
- **No telemetry** from the Deezer module.

---

## 15. Testing

### 15.1 Unit tests (no network)

New files under `src/test/`:

| Test | Covers |
|---|---|
| `deezercryptotest` | MD5, AES vectors, Blowfish key, stream path golden values, stripe decryption, depad |
| `deezerstreamfiltertest` | chunk boundaries 1/7/2048/6144/100k, plain vs crypted, short final stripe |
| `deezerjsontest` | public/GW payloads, missing/renamed fields, empty `data`, preview sizes |
| `deezerpathplannertest` | templates, sanitising, reserved names, collisions, long paths, Unicode |
| `deezerqualitytest` | plan flags × file sizes × preference × fallback |
| `deezerqueuetest` | ordering, concurrency, cancel, retry, partial album |
| `deezercachetest` | load/save, dedupe, upgrade |

Golden values: generate them once from the reference implementation and hard-code them.
Do not call Deezer in tests.

### 15.2 Integration tests (local server)

- Replay recorded JSON through `MockNetworkAccessManager` or a local `QTcpServer`.
- Serve a synthetic encrypted 6144-byte-stripe blob and assert the decoded file matches
  the original byte-for-byte.
- Simulate a mid-stream disconnect and assert:
  - no final file exists,
  - the temp file is removed,
  - the item is retryable.
- Import a generated FLAC fixture and assert the track is in the collection, tagged, and
  readable by `SoundSourceProxy`.

### 15.3 Manual matrix

| Dimension | Cases |
|---|---|
| Account | free, HQ, HiFi, expired ARL, logged out elsewhere |
| Track | FLAC, MP3 320, MP3 128, geo-blocked, removed, fallback, preview-only |
| Collection | album with 2 discs, playlist with 100 tracks, artist top 50 |
| Filesystem | FAT32, APFS, NTFS, network share, read-only folder, disk full |
| Names | emoji, CJK, RTL, 300-character title, reserved name, duplicate title |
| Network | offline, 3G, VPN, proxy, mid-download loss, 429 |
| Platform | Windows x64, macOS arm64, macOS x86_64, Linux x64 |

### 15.4 CI

- Unit + integration tests run in the normal `mixxx-test` binary, guarded by
  `#ifdef __DEEZER__`.
- No live network in CI.
- One Linux job builds with `-DDEEZER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`.
- `clang-format`, `clang-tidy`, `qmllint`, `markdownlint` clean.

---

## 16. Implementation phases

### Phase 0 — Spike (3–5 days)

Tasks:

- Add the CMake option and an empty `src/deezer/` module.
- Vendor AES and Blowfish with license headers.
- Implement `deezercrypto` + `deezerstreamfilter` with golden tests.
- Throwaway `deezer_spike` target (not installed): ARL sign-in, search, resolve, download
  one FLAC to a temp directory.
- Open the result in Paramix and confirm tags/cover.

Exit criteria: a real FLAC downloads and plays; crypto tests pass; no secret in logs.

### Phase 1 — Protocol client (1.5–2 weeks)

Tasks:

- `deezertypes`, `deezerjson`, `deezerhttpclient`, `deezersession`.
- `deezerapiclient`, `deezergwclient`, `deezermediaclient`, `deezertrackresolver`.
- Token refresh, rate limiter, retry, error mapping.
- Fixture-based unit tests.

Exit criteria: headless resolve + download for FLAC, MP3 320 and MP3 128; plan and
geo errors mapped correctly.

### Phase 2 — Download engine (1.5–2 weeks)

Tasks:

- `deezerdownloadtask`, `deezerdownloadqueue`, `deezerpathplanner`.
- Atomic writes, verification, cancel/retry/pause.
- `deezerimporter`: tags, cover, library insert.
- `deezercache`.
- Analysis and stem hooks behind settings.
- Integration tests.

Exit criteria: a 10-track album downloads, an individual failure does not abort the
album, no partial file enters the library.

### Phase 3 — UI (2–3 weeks)

Tasks:

- `viewId` seam + `QmlSidebarModelProxy::currentView`.
- `DeezerFeature` and `DeezerService`.
- `QmlDeezerProxy`, search model, queue model, image provider.
- QML screens and settings page.
- Search routing and local search restore.
- Accessibility, keyboard, theme, empty/error states.

Exit criteria: a new user can sign in, search, download an album, and load a track without
leaving Paramix.

### Phase 4 — Hardening (1–2 weeks)

Tasks:

- Proxy/VPN support and diagnostics.
- Windows path limits, Unicode normalisation, collision matrix.
- ARL expiry, 403 refresh, preview guards, rate limits.
- Documentation: user guide, ARL help, legal notice.
- CI coverage and release packaging notes.

Exit criteria: manual matrix passes; no known data-loss path; license/NOTICE updated.

### Phase 5 — Paramix extras (post-v1)

- Deezer playlist → Mixxx playlist/crate.
- "Download missing" from a Mixxx playlist.
- "Download & prepare stems" one-click flow.
- Favourites sync.
- OS keychain.
- URL paste import.

---

## 17. Acceptance criteria (v1)

1. Signed-out users can search and browse Deezer inside the library pane.
2. Signing in with an ARL shows account name, country and lossless/HQ capability.
3. Downloading a track, album, playlist and artist top works with progress, cancel and
   retry.
4. FLAC is used when available and permitted; MP3 320/128 otherwise according to the
   setting.
5. Preview streams are rejected before import.
6. Files are tagged, cover art is embedded and/or written as a sidecar, and tracks appear
   in the library immediately.
7. No partial file is ever visible to the scanner or the library.
8. Downloads never block or glitch audio playback.
9. All failures produce an actionable, translated message.
10. The module can be compiled out with `-DDEEZER=OFF` and the app behaves exactly as
    before.

---

## 18. Risks specific to the port

| Risk | Mitigation |
|---|---|
| GW field rename | typed extraction per method; fixtures; one-file fix |
| Token/ARL expiry | refresh-on-403, clear re-auth state, test fixtures |
| Quality/plan mismatch | per-batch plan check; verify format after write |
| Rate-limit ban | token bucket, concurrency 3, `Retry-After` |
| Geo-block | per-track error, country message, optional fallback IDs |
| Corrupt/truncated file | `QSaveFile`, duration+magic+size verification |
| Windows path limits | preflight, component shortening, tests |
| Unicode duplicates on macOS | NFC normalisation |
| Cover embed regressions | direct TagLib tests for FLAC/MP3 |
| TagLib write race | import/tag on the GUI thread after download |
| GPLv3 obligations | §19 decision before merge |
| Upstream protocol change | track Deemix releases; keep `deezerjson` tolerant |

---

## 19. Licensing and compliance checklist

1. Decide: **GPL-3.0 combined work** (port) or **clean-room** (stay GPL-2.0-only).
2. If porting:
   - add `SPDX-License-Identifier: GPL-3.0-only` headers to `src/deezer/*`;
   - add a `src/deezer/README.md` with upstream attribution and the Deemix commit;
   - update `NOTICE` and `README.md` to say that builds with `-DDEEZER=ON` are GPL-3.0;
   - keep the AES/Blowfish public-domain notices.
3. Add the Deezer ToS / "personal use only" notice to the settings page and docs.
4. Do not bundle an ARL, a `license_token`, or any account data.
5. Do not ship the module in store builds without a legal review.
6. Keep `-DDEEZER=OFF` as the default for official binaries unless the policy decision
   says otherwise.

---

## Appendix A — Endpoint reference

| Purpose | Method | URL |
|---|---|---|
| Search tracks | GET | `https://api.deezer.com/search/track?q=&index=&limit=` |
| Search albums | GET | `https://api.deezer.com/search/album?q=&index=&limit=` |
| Search artists | GET | `https://api.deezer.com/search/artist?q=&index=&limit=` |
| Search playlists | GET | `https://api.deezer.com/search/playlist?q=&index=&limit=` |
| Track | GET | `https://api.deezer.com/track/{id}` |
| Album | GET | `https://api.deezer.com/album/{id}` |
| Album tracks | GET | `https://api.deezer.com/album/{id}/tracks` |
| Artist | GET | `https://api.deezer.com/artist/{id}` |
| Artist top | GET | `https://api.deezer.com/artist/{id}/top` |
| Playlist | GET | `https://api.deezer.com/playlist/{id}` |
| Playlist tracks | GET | `https://api.deezer.com/playlist/{id}/tracks` |
| Session | POST | `https://www.deezer.com/ajax/gw-light.php?method=deezer.getUserData` |
| Gateway | POST | `https://www.deezer.com/ajax/gw-light.php?method=<m>&api_token=<t>` |
| Media URL | POST | `https://media.deezer.com/v1/get_url` |
| Cover art | GET | `https://e-cdn-images.dzcdn.net/images/cover/{md5}/{size}x{size}-000000-80-0-0.jpg` |

## Appendix B — Upstream → C++ file map

| Upstream | Port |
|---|---|
| `deezer-sdk/src/deezer.ts` | `deezersession.cpp` |
| `deezer-sdk/src/api.ts` | `deezerapiclient.cpp` |
| `deezer-sdk/src/gw.ts` | `deezergwclient.cpp` |
| `deezer-sdk/src/utils.ts` | `deezerjson.cpp` |
| `deezer-sdk/src/types.ts` | `deezertypes.h` |
| `deemix/src/decryption.ts` | `deezerstreamfilter.cpp` + `deezercrypto.cpp` |
| `deemix/src/utils/crypto.ts` | `deezercrypto.cpp` |
| `deemix/src/downloader.ts` | `deezerdownloadtask.cpp` + `deezerdownloadqueue.cpp` |
| `deemix/src/download-objects/*` | `deezerdownloadqueue.cpp` |
| `deemix/src/utils/pathtemplates.ts` | `deezerpathplanner.cpp` |
| `deemix/src/utils/getPreferredBitrate.ts` | `deezermediaclient.cpp` |
| `deemix/src/utils/downloadUtils.ts` | `deezerdownloadtask.cpp` (existence/overwrite) |
| `deemix/src/types/Track.ts` | `deezertrackresolver.cpp` |
| `deemix/src/tagger.ts` | **not ported** — `MetadataSourceTagLib` + `DeezerCoverWriter` |
| `deemix/src/settings.ts` | `[Deezer]` settings |
| `deemix/src/utils/localpaths.ts` | `QStandardPaths` |
| `deemix/src/utils/downloadImage.ts` | `DeezerImageProvider` |
| `deemix/src/plugins/spotify.ts` | deferred |

## Appendix C — Header skeleton

```cpp
// src/deezer/deezerservice.h
#pragma once

#include <QObject>
#include <memory>

#include "deezer/deezertypes.h"

namespace mixxx::deezer {

class DeezerSession;
class DeezerApiClient;
class DeezerGwClient;
class DeezerMediaClient;
class DeezerDownloadQueue;
class DeezerDownloadTask;
class DeezerTrackResolver;
class DeezerImporter;
class DeezerCache;

class DeezerService : public QObject {
    Q_OBJECT
  public:
    static DeezerService* createInstance(UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);
    static DeezerService* instance();
    static void destroy();

    DeezerSession* session() const;
    DeezerApiClient* api() const;
    DeezerGwClient* gw() const;
    DeezerMediaClient* media() const;
    DeezerDownloadQueue* queue() const;
    DeezerTrackResolver* resolver() const;
    DeezerImporter* importer() const;
    DeezerCache* cache() const;

    void shutdown();

  signals:
    void accountChanged();
    void errorOccurred(const DownloadError& error);

  private:
    DeezerService(UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);
    ~DeezerService() override;
    // ...
};

} // namespace mixxx::deezer
```

## Appendix D — Definition of a finished task

A task is done only when:

- the code builds with `-DDEEZER=ON` and with `-DDEEZER=OFF`;
- new logic has unit tests and they pass in `mixxx-test`;
- no secret appears in a log even at `debug` level;
- cancellation leaves no temp file and no partial library entry;
- `clang-format` and `clang-tidy` are clean;
- the commit message explains the protocol assumption behind any new endpoint.
