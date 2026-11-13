#pragma once

// The process that draws the pictures of slides ProPresenter's API is asked for (see
// slidepictures.h): the app's own program, started by the app (slidepictureservice.h)
// with --slide-picture-worker and the workspace's folder, which reads requests on its
// standard input, a line of JSON each, and writes each picture to its standard output,
// a line of JSON and then the picture's bytes.
//
// Why a process of its own, and not a thread of the app's. A picture of a slide is made
// by the same QML that draws the slide on the screens (SlidePictureMaker, with the app's
// Slide), so that it cannot differ from what is shown; and QML makes a slide's items and
// lays out its words on the thread that runs the app's windows, where some 8 ms a slide
// of words, and 28 of shapes, is a video's frame missed, a dozen times over as a phone
// leafs through a presentation. Only the last step, drawing what has been made, can be
// handed to another thread inside one process. In a process of its own the whole of it
// is off the app's threads: and the process asks Linux to run it only when nothing else
// wants the processor (SCHED_IDLE, which its threads inherit) and to read the disk only
// when nothing else is (the idle I/O class), so that a slide taken live while it works
// is not slowed by it. (The graphics chip is shared all the same: a picture of 400 by
// 225 is a small piece of its work.)
//
// What it does for each request: the presentation is read (ProDocument, as the window
// reads it, kept while the next request is of the same file), the slide picked in the
// arrangement asked for, and the picture looked for in its cache (the app's cache
// folder, slide-pictures/, named by everything it is drawn from: see
// slidepictures::cacheName). If it is not there, the slide is put in the window, which
// is never shown, the window is drawn out of sight and read back once everything in it
// has loaded (or after a few seconds, if something will not), and the picture made a
// JPEG or a PNG and kept. The cache is kept under 200 MB, the oldest pictures going
// first, each time the process starts.
//
// It stops when its standard input closes: when the app closes it, having had nothing
// to ask for a while, or goes.
namespace slidepictures {

// Whether this run of the program is to be the worker: its first argument says so
bool isWorker(int argc, char *argv[]);
int runWorker(int argc, char *argv[]);

}
