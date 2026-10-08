#!/usr/bin/env python3
"""Runs the app's benchmark several times over and compares it with the baseline.

    benchmark/run.py                    five runs of build/SimplePresenter, compared with benchmark/baseline.txt
    benchmark/run.py --runs 3           fewer runs
    benchmark/run.py --binary PATH      another build of the app
    benchmark/run.py --save-baseline    make these runs the baseline

Each run is `SimplePresenter --benchmark <file>`: the app makes a workspace of its own,
works through it, writes what it timed and quits (see src/benchmark.h). The runs here
are given settings, caches and a Documents folder of their own, in a temporary folder,
so that nothing of yours is read or touched and every run starts from the same place.

What is compared is the median of the runs. A line is marked when it is worse than
the baseline by more than a fifth and by more than the runs differ among themselves:
anything less is as likely to be the machine as the app.

The windows open on your screens while it runs, the output filling one of them. Leave
the machine alone until it is done: about a minute a run.
"""
import argparse
import os
import statistics
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
BASELINE = os.path.join(HERE, "baseline.txt")


def read(path):
    """A report as (header lines, {name: (value, unit)})."""
    header, values = [], {}
    with open(path, encoding="utf-8") as report:
        for line in report:
            line = line.rstrip("\n")
            if line.startswith("#"):
                header.append(line)
            elif line.count("\t") == 2:
                name, value, unit = line.split("\t")
                values[name] = (float(value), unit)
    return header, values


def one_run(binary, folder, number):
    """Runs the benchmark once, in a home of its own under `folder`. Returns its report's path."""
    home = os.path.join(folder, f"run{number}")
    documents = os.path.join(home, "Documents")
    for part in ("config", "data", "cache", "Documents"):
        os.makedirs(os.path.join(home, part))
    # Where the app's own log of the run goes, rather than among yours
    with open(os.path.join(home, "config", "user-dirs.dirs"), "w", encoding="utf-8") as dirs:
        dirs.write(f'XDG_DOCUMENTS_DIR="{documents}"\n')
    environment = dict(os.environ, XDG_CONFIG_HOME=os.path.join(home, "config"), XDG_DATA_HOME=os.path.join(home, "data"),
                       XDG_CACHE_HOME=os.path.join(home, "cache"))
    report = os.path.join(home, "report.txt")
    done = subprocess.run([binary, "--benchmark", report], env=environment, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, timeout=600, check=False)
    if done.returncode != 0 or not os.path.exists(report):
        sys.exit(f"Run {number} did not finish (the app ended with {done.returncode}).")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--binary", default=os.path.join(HERE, "..", "build", "SimplePresenter"))
    parser.add_argument("--baseline", default=BASELINE)
    parser.add_argument("--save-baseline", action="store_true")
    asked = parser.parse_args()
    binary = os.path.abspath(asked.binary)
    if not os.path.exists(binary):
        sys.exit(f"There is no {binary}: build the app first, or name one with --binary.")

    header, runs = [], []
    with tempfile.TemporaryDirectory(prefix="simplepresenter-benchmark-") as folder:
        for number in range(1, asked.runs + 1):
            print(f"run {number} of {asked.runs}...", file=sys.stderr, flush=True)
            header, values = read(one_run(binary, folder, number))
            runs.append(values)

    names = list(runs[0])
    median = {name: statistics.median(run[name][0] for run in runs if name in run) for name in names}
    spread = {name: max(run[name][0] for run in runs if name in run) - min(run[name][0] for run in runs if name in run)
              for name in names}
    unit = {name: runs[0][name][1] for name in names}

    if asked.save_baseline:
        with open(asked.baseline, "w", encoding="utf-8") as out:
            for line in header:
                out.write(line + "\n")
            out.write(f"# the median of {asked.runs} runs\n")
            for name in names:
                out.write(f"{name}\t{median[name]:.1f}\t{unit[name]}\n")
        print(f"The baseline is now these {asked.runs} runs: {asked.baseline}", file=sys.stderr)

    before_header, before = read(asked.baseline) if os.path.exists(asked.baseline) else ([], {})
    for line in header:
        print(line)
    machine = [line for line in before_header if not line.startswith("# the median")]
    if before and machine != header:
        print("# The baseline was recorded on something else, so the comparison says little:")
        for line in machine:
            print("#   " + line[2:])
    width = max(len(name) for name in names)
    print(f"{'':{width}}  {'now':>9}  {'baseline':>9}  {'change':>7}   spread of the runs")
    worse = 0
    for name in names:
        now = median[name]
        line = f"{name:{width}}  {now:9.1f}  "
        if name in before:
            was = before[name][0]
            change = (now - was) / was * 100 if was else 0.0
            # More frames a second is better; of everything else, less is.
            decline = -change if unit[name] == "fps" else change
            marked = decline > 20 and abs(now - was) > spread[name] and abs(now - was) >= 2
            worse += marked
            line += f"{was:9.1f}  {change:+6.0f}%"
            line += " !" if marked else "  "
        else:
            line += f"{'':9}  {'':7}  "
        print(f"{line} ±{spread[name] / 2:.1f} {unit[name]}")
    if worse:
        print(f"\n{worse} marked ! are worse than the baseline by more than a fifth.")
    return 1 if worse else 0


if __name__ == "__main__":
    sys.exit(main())
