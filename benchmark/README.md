# The benchmark

This is for telling whether a change to the app has made it slower. It times the things
an operator waits for, and compares them with a set of times kept from before.

    cmake --build build
    benchmark/run.py

That runs the app five times over, about a minute each. The windows open on your
screens while it does, the output filling one of them: leave the machine alone until it
is done. Nothing of yours is read or changed. The app makes a workspace of its own to
work on, with three presentations of fixed content, and each run has settings, caches
and a Documents folder of its own in a temporary folder.

What comes out is a table: each thing timed, as the median of the runs, beside the
baseline and the change between them. A line is marked `!` when it is worse than the
baseline by more than a fifth and by more than the runs differ among themselves.
Anything less is as likely to be the machine as the app.

## What is timed

- **Starting up**: from the program being started to its windows being made, to the
  first frame of each, and to the operator window standing still, which is when it is
  ready to be used.
- **Opening a presentation**: from asking to its slides all being drawn, the first time
  and again.
- **Showing slides**, with the output on and filling its screen: from the click to the
  frame the slide is on the output in, for every slide in turn. Then the same with a
  transition between the slides: how many frames a second it ran at, and the longest
  wait for one.
- **The editor**: opening and closing it, and how long a change takes to reach the
  screen while something is dragged: the corners of a rounded rectangle, its opacity,
  its width.
- **Standing by**: how much of the processor the app uses with a slide on the output
  and nothing happening, and whether it is drawing frames it has no need to.
- **Memory**: how much the app holds at the end, and the most it held.

The three presentations are words (large text, as most slides are), shapes (one of each
kind the app draws, with a gradient, pictures cut to shapes and feathered edges, which
is the slide that costs most to draw) and pictures (words over a picture the slide
brings with it as its background). There is no video in it yet.

## The baseline

`baseline.txt` is the median of five runs, with what it was recorded on at its top. A
baseline only means something on the machine and the screens it was recorded on. The
one here was recorded on the laptop the app is written on (a two-core Intel i5-7300U
with its built-in graphics), inside a desktop with no screen attached, which is where
the app's other tests are run. To compare like with like on your own screens, record
your own once, and again whenever a slowdown has been accepted or a speed-up made:

    benchmark/run.py --save-baseline

## One run by hand

    build/SimplePresenter --benchmark report.txt

writes the times to `report.txt` and says them on the terminal as it goes. Run like
that it uses your own settings folder for the app's log of the run, which is the only
thing of yours it touches.

The run itself is `qml/Benchmark.qml`; the workspace it works on, the clock and the
report are `src/benchmark.cpp`.
