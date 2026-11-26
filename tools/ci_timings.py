#!/usr/bin/env python3
"""Report per-step GitHub Actions timings for the CI workflow.

Reads the public Actions API (authenticated with GH_TOKEN or GITHUB_TOKEN when
one is set, anonymous otherwise) and prints Markdown tables of step and job
durations over the most recent completed runs. Use it before and after a
workflow change; see docs/ci-performance-plan.md.

Anonymous access allows 60 requests per hour and one request is made per run,
so prefer an authenticated token for anything larger than ~30 runs.
"""

import argparse
import datetime
import json
import os
import statistics
import urllib.error
import urllib.request
from collections import defaultdict

API = "https://api.github.com"


def get(url):
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "paramix-ci-timings",
        },
    )
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        raise SystemExit(f"GitHub API {url} failed: {exc}") from exc


def parse(timestamp):
    return datetime.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))


def minutes(start, end):
    return (parse(end) - parse(start)).total_seconds() / 60


def fetch_runs(repo, workflow, limit):
    runs = []
    page = 1
    while len(runs) < limit and page <= 10:
        url = f"{API}/repos/{repo}/actions/runs"
        url += f"?per_page=50&page={page}"
        batch = get(url).get("workflow_runs", [])
        if not batch:
            break
        for run in batch:
            if run["name"] != workflow or run["status"] != "completed":
                continue
            runs.append(run)
            if len(runs) == limit:
                break
        page += 1
    return runs


def collect(repo, runs):
    steps = defaultdict(list)
    jobs = defaultdict(list)
    for run in runs:
        url = f"{API}/repos/{repo}/actions/runs/{run['id']}/jobs?per_page=50"
        for job in get(url).get("jobs", []):
            if not job.get("completed_at"):
                continue
            total = minutes(job["started_at"], job["completed_at"])
            jobs[job["name"]].append(total)
            for step in job.get("steps", []):
                if not step.get("completed_at"):
                    continue
                steps[(job["name"], step["name"])].append(
                    minutes(step["started_at"], step["completed_at"])
                )
    return steps, jobs


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--repo", default="christianstuart/paramix")
    parser.add_argument("--workflow", default="CI")
    parser.add_argument("--runs", default=20, type=int)
    args = parser.parse_args()
    if args.runs <= 0:
        parser.error("--runs must be a positive integer")

    runs = fetch_runs(args.repo, args.workflow, args.runs)
    if not runs:
        raise SystemExit(
            f"no completed '{args.workflow}' runs found in {args.repo}"
        )

    steps, jobs = collect(args.repo, runs)
    print(f"{len(runs)} most recent completed runs of '{args.workflow}'")
    print(f"in {args.repo}. All times in minutes.")
    print()
    print("| Job | Step | Runs | Median | Max |")
    print("|---|---|---|---|---|")
    ordered = sorted(
        steps.items(),
        key=lambda item: (item[0][0], -statistics.median(item[1])),
    )
    for (job, step), values in ordered:
        median = statistics.median(values)
        print(
            f"| {job} | {step} | {len(values)} | "
            f"{median:.1f} | {max(values):.1f} |"
        )

    print()
    print("| Job | Runs | Median total | Max total |")
    print("|---|---|---|---|")
    for job, values in sorted(jobs.items()):
        print(
            f"| {job} | {len(values)} | "
            f"{statistics.median(values):.1f} | {max(values):.1f} |"
        )


if __name__ == "__main__":
    main()
