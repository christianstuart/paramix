#!/usr/bin/env python3
"""External CPU-load harness for Paramix audio dropout reports (issue #61).

This tool does not play audio and does not talk to the running application. It
puts `--cores` Python processes into a busy loop for `--duration` seconds and
then reads whatever the application appended to its log during the run. That is
deliberate: the audio dropout was seen once and never reproduced, so the honest
output is "here is the load, here is what the log says", not a fabricated
verdict.

Usage:
    python tools/paramix_audio_stress.py --cores 6 --duration 180
    python tools/paramix_audio_stress.py --log "C:/Users/me/AppData/Local/Mixxx/mixxx.log"
    python tools/paramix_audio_stress.py --print-recipe

Requires only the Python standard library. No network access.
"""

from __future__ import annotations

import argparse
import multiprocessing
import os
import re
import sys
import time
from pathlib import Path

DROPOUT_MARKER = "Audio dropout:"
STARVATION_MARKER = "Audio starvation"
REPORT_MARKERS = (DROPOUT_MARKER, STARVATION_MARKER)

MANUAL_RECIPE = """\
Manual reproduction recipe (F03 / issue #61)
--------------------------------------------
The harness only creates CPU load and reads the log; you still have to create
the audio workload:

1. Use a build with stem separation enabled and a real audio device (not the
   dummy/null device).
2. Load lossless tracks (44.1 or 48 kHz) into deck 1 and deck 2 and play both.
   Leave the crossfader and EQ at neutral, no effects, no key lock changes.
3. Queue two or three tracks for stem separation and let it run. The stem
   worker must be busy for the whole run.
4. Every 10 seconds load a different track into a stopped deck or a sampler, so
   the load-triggered analyzer runs repeatedly.
5. Use a small buffer: 128 frames (~2.9 ms at 44.1 kHz) if the device accepts
   it, otherwise 256 frames. Try the device API that was in use when the pause
   was heard (on Windows: WASAPI shared, WASAPI exclusive and WDM-KS; on Linux:
   ALSA and JACK; on macOS: CoreAudio). Record the API and buffer size in the
   issue.
6. While that is running, start the load:
       python tools/paramix_audio_stress.py --cores <logical cores - 1> --duration 180
7. Read the newest lines in the application log (mixxx.log in the Mixxx
   settings directory) and look for lines starting with "Audio dropout:" or
   "Audio starvation". Each line has the underflow code,
   [App],audio_latency_usage, the cache-miss delta since the previous line,
   and which background jobs were active.

How to read a report line:
  cache_miss_delta > 0  -> a deck asked for audio the reader had not decoded
                           yet: reader starvation, not a device stall.
  cache_miss_delta == 0 and usage near 1.0
                        -> the callback itself ran out of time (CPU, lock,
                           driver or GPU contention).
  usage well below 1.0 with codes 7, 11 or 19
                        -> the device/driver underflowed outside Mixxx's
                           callback budget.
"""

LOG_FIELD_PATTERNS = {
    "underflow_code": re.compile(r"underflow_code=\s*(-?\d+)"),
    "usage": re.compile(r"audio_latency_usage=\s*([0-9.]+)"),
    "cache_miss_delta": re.compile(r"cache_miss_delta=\s*(\d+)"),
    "overload_count": re.compile(r"audio_latency_overload_count=\s*(\d+)"),
    "stems": re.compile(r"stems=\s*(\w+)"),
    "load_analysis": re.compile(r"load_analysis=\s*(\w+)"),
    "batch_analysis": re.compile(r"batch_analysis=\s*(\w+)"),
    "stem_detail": re.compile(r"stem_detail=\s*(\w+)"),
}

TIMESTAMP_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\.(\d{3})\s")


def default_log_candidates() -> list[Path]:
    candidates: list[Path] = []
    home = Path.home()
    if os.name == "nt":
        for variable in ("LOCALAPPDATA", "APPDATA", "USERPROFILE"):
            base = os.environ.get(variable)
            if base:
                candidates.append(Path(base) / "Mixxx" / "mixxx.log")
    elif sys.platform == "darwin":
        candidates.append(home / "Library" / "Application Support" / "Mixxx" / "mixxx.log")
    candidates.append(home / ".mixxx" / "mixxx.log")
    return candidates


def find_log(explicit: str | None) -> Path | None:
    if explicit:
        path = Path(explicit).expanduser()
        return path if path.is_file() else None
    for path in default_log_candidates():
        if path.is_file():
            return path
    return None


def burn_cpu(duration_seconds: float) -> None:
    """Child process: keep one logical core busy for the whole run."""
    deadline = time.monotonic() + duration_seconds
    value = 0
    while time.monotonic() < deadline:
        for _ in range(200_000):
            value = (value * 1103515245 + 12345) & 0xFFFFFFFF


def start_load(cores: int, duration_seconds: float) -> list[multiprocessing.Process]:
    # "spawn" is the only start method on Windows and keeps the tool working
    # when it is launched with the same Python that the user runs elsewhere.
    context = multiprocessing.get_context("spawn")
    processes = [
        context.Process(target=burn_cpu, args=(duration_seconds,), daemon=True)
        for _ in range(cores)
    ]
    for process in processes:
        process.start()
    return processes


def stop_load(processes: list[multiprocessing.Process]) -> None:
    for process in processes:
        if process.is_alive():
            process.terminate()
    for process in processes:
        process.join(timeout=5)


def log_size(path: Path) -> int:
    try:
        return path.stat().st_size
    except OSError:
        return 0


def read_appended(path: Path, offset: int) -> str:
    try:
        with path.open("rb") as log_file:
            log_file.seek(offset)
            data = log_file.read()
    except OSError:
        return ""
    return data.decode("utf-8", errors="replace")


def timestamp_ms(match: re.Match[str]) -> int:
    hours, minutes, seconds, milliseconds = (int(group) for group in match.groups())
    return ((hours * 60 + minutes) * 60 + seconds) * 1000 + milliseconds


def parse_dropouts(text: str) -> list[dict[str, object]]:
    events: list[dict[str, object]] = []
    for line in text.splitlines():
        if not any(marker in line for marker in REPORT_MARKERS):
            continue
        event: dict[str, object] = {"line": line.strip()}
        event["kind"] = (
            "underflow" if DROPOUT_MARKER in line else "starvation"
        )
        timestamp = TIMESTAMP_RE.match(line)
        if timestamp:
            event["time_ms"] = timestamp_ms(timestamp)
        for name, pattern in LOG_FIELD_PATTERNS.items():
            match = pattern.search(line)
            if match:
                event[name] = match.group(1)
        events.append(event)
    return events


def longest_report_gap_ms(events: list[dict[str, object]]) -> int | None:
    times = [int(event["time_ms"]) for event in events if "time_ms" in event]
    if len(times) < 2:
        return None
    longest = 0
    for previous, current in zip(times, times[1:]):
        gap = current - previous
        if gap < 0:
            # Log timestamps have no date; a negative step means midnight.
            gap += 24 * 60 * 60 * 1000
        longest = max(longest, gap)
    return longest


def report(events: list[dict[str, object]], log_path: Path, duration: float) -> None:
    print()
    print(f"Log: {log_path}")
    print(f"Dropout/starvation lines in the {duration:.0f} s window: {len(events)}")

    if not events:
        print()
        print("No 'Audio dropout:' or 'Audio starvation' line was appended during")
        print("the run. That can mean either that no underflow happened, or that")
        print("the running binary does not contain the F03 diagnostics. Check that")
        print("the build is current before treating this as a clean run.")
        print()
        print(MANUAL_RECIPE)
        return

    underflows = sum(1 for event in events if event.get("kind") == "underflow")
    starvations = len(events) - underflows
    print(f"Device-underflow reports: {underflows}")
    print(f"Reader-starvation reports: {starvations}")

    overload_counts = [int(event["overload_count"]) for event in events if "overload_count" in event]
    usages = [float(event["usage"]) for event in events if "usage" in event]
    cache_deltas = [int(event["cache_miss_delta"]) for event in events if "cache_miss_delta" in event]
    codes = sorted({event["underflow_code"] for event in events if "underflow_code" in event})
    gap = longest_report_gap_ms(events)

    print(f"Underflow codes seen: {', '.join(codes) if codes else 'unknown'}")
    if overload_counts:
        print(
            "Audio latency overload count: "
            f"{overload_counts[0]} -> {overload_counts[-1]} "
            f"(+{overload_counts[-1] - overload_counts[0]})"
        )
    if usages:
        print(f"Peak [App],audio_latency_usage: {max(usages):.3f}")
    if cache_deltas:
        print(f"Largest cache-miss delta between reports: {max(cache_deltas)}")
    if gap is not None:
        print(
            "Longest gap between dropout reports: "
            f"{gap} ms (proxy only: reports are rate-limited to one per second, "
            "so this is not a sample-accurate silence measurement)"
        )

    states: dict[str, set[str]] = {}
    for event in events:
        for name in ("stems", "load_analysis", "batch_analysis", "stem_detail"):
            value = event.get(name)
            if isinstance(value, str):
                states.setdefault(name, set()).add(value)
    if states:
        print("Background jobs seen while dropping out:")
        for name in ("stems", "load_analysis", "batch_analysis", "stem_detail"):
            if name in states:
                print(f"  {name}: {', '.join(sorted(states[name]))}")

    print()
    print("Interpretation hints:")
    print("  cache_miss_delta > 0  -> reader starvation (I/O, decode or worker priority)")
    print("  cache_miss_delta == 0 with usage near 1.0 -> callback CPU budget or a stall")
    print("  low usage with code 7/11/19 -> device or driver underflow")
    print()
    print("Attach the matching log excerpt to issue #61. Do not edit the counts.")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create external CPU load and report Paramix audio dropout lines.",
    )
    parser.add_argument(
        "--cores",
        type=int,
        default=max(1, (os.cpu_count() or 2) - 1),
        help="number of CPU-burner processes (default: logical cores - 1)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=120.0,
        help="seconds to keep the load running (default: 120)",
    )
    parser.add_argument(
        "--log",
        default=None,
        help="path to mixxx.log (default: the usual Mixxx settings directory)",
    )
    parser.add_argument(
        "--print-recipe",
        action="store_true",
        help="print the manual reproduction recipe and exit",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.print_recipe:
        print(MANUAL_RECIPE)
        return 0
    if args.cores < 1:
        print("--cores must be at least 1", file=sys.stderr)
        return 2
    if args.duration <= 0:
        print("--duration must be positive", file=sys.stderr)
        return 2

    log_path = find_log(args.log)
    if log_path is None:
        print("Could not find mixxx.log.")
        print("Pass --log <path> if the application writes its log elsewhere.")
        print()
        print("The harness will still create load, but it cannot report counts.")
        print()
        print(MANUAL_RECIPE)

    offset = log_size(log_path) if log_path is not None else 0
    print(f"Starting {args.cores} CPU burner(s) for {args.duration:.0f} s...")
    if log_path is not None:
        print(f"Watching {log_path}")
    print("Create the audio workload described in the manual recipe now.")
    processes = start_load(args.cores, args.duration)
    try:
        time.sleep(args.duration)
    except KeyboardInterrupt:
        print("Interrupted; stopping the load.")
    finally:
        stop_load(processes)

    if log_path is None:
        print("No log was readable, so no counts are reported.")
        return 3

    events = parse_dropouts(read_appended(log_path, offset))
    report(events, log_path, args.duration)
    return 0


if __name__ == "__main__":
    multiprocessing.freeze_support()
    raise SystemExit(main(sys.argv[1:]))
