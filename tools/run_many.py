#!/usr/bin/env python3
"""Run the board several times side by side, a run a processor core.

    python tools/run_many.py <machine ms> <folder> [--seeds 0 1 2 3] [--jobs N]
                             [--every N] [--video] [--exe path/to/run_board]

Each run is sim/board/run_board.cpp's with its own power-up state (seed=N;
0 is all zero), into <folder>/seed<N>/ with its log in run.log.  A run
takes as long as one alone; side by side they make a night's checks many
times over.  At the end, a line a run from its "summary:" line, and the
verdict: good when the fetches kept to the emulator's and the last frame
compared is the emulator's screen.
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
EXE = os.path.join(REPO, "build", "Release", "sim", "run_board.exe" if os.name == "nt" else "run_board")


def summary(log):
    """The last "summary: key=value ..." line of a run's log, as ints."""
    lines = [l for l in log.splitlines() if l.startswith("summary:")]
    if not lines:
        return None
    return {k: int(v) for k, v in (item.split("=") for item in lines[-1].split()[1:])}


def good(s):
    """The run kept to the emulator's fetches and ended on its screen."""
    return s is not None and s["diverged"] == 0 and s["last_differ"] == 0


def run(exe, ms, folder, seed, every, video):
    out = os.path.join(folder, f"seed{seed}")
    os.makedirs(out, exist_ok=True)
    args = [exe, str(ms), out, str(every)] + (["video"] if video else []) + [f"seed={seed}"]
    with open(os.path.join(out, "run.log"), "w", encoding="utf-8") as log:
        subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, check=False)
    with open(os.path.join(out, "run.log"), encoding="utf-8", errors="replace") as log:
        return seed, summary(log.read())


def main(argv):
    p = argparse.ArgumentParser(description="the board's runs side by side")
    p.add_argument("ms", type=int)
    p.add_argument("folder")
    p.add_argument("--seeds", type=int, nargs="+", default=[0, 1, 2, 3])
    p.add_argument("--jobs", type=int, default=os.cpu_count())
    p.add_argument("--every", type=int, default=30)
    p.add_argument("--video", action="store_true")
    p.add_argument("--exe", default=EXE)
    a = p.parse_args(argv[1:])
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futures = [pool.submit(run, a.exe, a.ms, a.folder, s, a.every, a.video) for s in a.seeds]
        results = sorted(f.result() for f in concurrent.futures.as_completed(futures))
    failed = 0
    for seed, s in results:
        if s is None:
            print(f"seed {seed}: no summary - see {a.folder}/seed{seed}/run.log")
            failed += 1
            continue
        verdict = "good" if good(s) else "LOOK"
        failed += verdict != "good"
        print(f"seed {seed}: {verdict}  {s['matched']}/{s['fetches']} fetches as the emulator's, "
              f"{s['waits']} loops waited out, last frame {s['last_differ']} samples off, {s['frames']} frames")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
