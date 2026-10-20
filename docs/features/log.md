[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Log

Each run of the app keeps a log: a text file in `~/Documents/SimplePresenter/Logs`,
named for when the app was started. It is there for working out what happened when
something has gone wrong. It is written to be read by a person, and to be handed to an
AI model, which makes good sense of one. It is not a line to a support desk: there is
none. The twenty most recent are kept, so the one from the time it went wrong is still
there after the app has been started again. The About section of the settings says
which file is this run's, and has a button that opens the folder.

A log starts with what the app is running on: its version and Qt's, the system, the
processor and memory, the screens, and what draws the windows, which is the graphics
chip and its driver. After that there is a line for each thing that was done or that
happened, with the time and the kind of thing it is:

```
10:03:18.915  open        "Move Of God" [Default], 43 slides, from the library "Demo"
10:03:18.940  live        slide 1 of 43 of "Move Of God"; with its background video "Hopeful Horizon Bliss - 4K.mp4", looping
10:03:19.045  media       video "Hopeful Horizon Bliss - 4K.mp4": first picture after 105 ms; 3840x2160, NV12, frames arriving as textures (decoded by the graphics chip); H264, 30 frames a second, 30.0 s long
10:03:19.052  transition  media layer: Dissolve over 0.60 s, shaders/dissolve.frag.qsb
10:03:19.645  transition  media layer: Dissolve done: 36 frames in 0.59 s, 61 a second
```

So it has slides going live, media and what each video turned out to be, transitions
and how many frames each managed, clears, props, timers, the editor, files being saved,
windows shown, hidden or moved to another screen, screens connected and disconnected,
fonts that a presentation uses and the machine does not have, and whatever Qt itself
says. Every five minutes a line says how much of a processor and how much memory the
app has been using. It holds the names of files, presentations and playlists, and where
they are on the disk, and nothing of what is in them: no words of any slide.

A line whose second column is in capitals is something that went wrong:

- `WARNING`, `ERROR`: what Qt, or a library under it, has complained of;
- `PROBLEM`: something the app could not do, including every error it showed;
- `STALLED`: the app has not answered for two seconds, because it is busy or stuck.
  A `recovered` line follows when it answers again, with how long it was;
- `CRASH`: the app has stopped, with what stopped it and where it was at the time.

The case it is for above all is the app stopping dead, which leaves nothing else to
go on. A transition's shader is run by the graphics driver, and a driver that does not
agree with one can take the app down with it. So a transition goes into the log before
its shader is used, by name and by file: if nothing follows that line, it says which
transition it was. A crash writes its own last lines: what stopped the app, and where
it was at the time, as a list of places. Those in Qt, in the graphics driver and in the
other libraries come with the names those libraries give out; those in the app itself
are numbers, which the list of names made with each release turns back into names (see
[Building from source](../installing.md#building-from-source)). And the next run's log begins by saying
so if the run before it crashed, was killed, or is still running.

Keeping the log costs nothing that can be noticed. It is written when something
happens, a few lines for a click, and never for a frame; each line is handed to the
system as it is made, which takes four millionths of a second and waits for no disk,
and is why the last line before a crash is in the file. What Qt says is kept within
bounds: a message that repeats is counted and not written again, and of a flood one a
second gets through. A log that grows past 2 MB carries on in a new file and keeps the
one before, so a session's log is never more than twice that. The one thing done by
the clock is the question put to the app once a second, to see that it is answering.
