#pragma once

#include "graphicsData.pb.h"

#include <QList>
#include <QVariantMap>

// Where sound goes: the rules of the app's channels, and of how a piece of media's own
// channels and the app's reach the device's. Plain functions on lists and numbers, with
// nothing of what plays the sound in them, so that they can be tried by themselves
// (tests/unit/tst_audiorouting); AudioMixer (audiomixer.h) mixes by them.
//
// The way is ProPresenter's ("Audio Routing in ProPresenter", support.renewedvision.com):
//
//   - The app has channels of its own, up to sixteen (ProPresenter calls them its
//     channels; its file, buses), each with a name, and with Mute, Solo and a test Tone.
//     How many there are and what they are called is the workspace's, in ProPresenter's
//     file of how the workspace is set up (audiosetupfile.h).
//   - The app's channels go to the device's by a matrix of the two: each of the app's
//     channels can go to any of the device's, or to several, or to none. That is this
//     computer's, since another has another device (AudioMixer keeps it in the app's
//     settings, a matrix for each device).
//   - A piece of media's own channels go to the app's by a matrix of their own, set in
//     its Media Inspector, each with a volume and a mute; and the media's sound can come
//     in and go out by a ramp. Those are ProPresenter's fields of the media
//     (graphicsData.proto: Media.AudioProperties, and the transport's fades).
//
// A matrix is kept as where each of its inputs goes: for each input in order, the
// outputs it is sent to, by their numbers from 0. An input that goes nowhere has none.
namespace audiorouting {

// How many channels the app can have, as ProPresenter
constexpr int kMostChannels = 16;

using Routes = QList<QList<int>>;

// ProPresenter's Auto for a piece of media, as its files have it where a media's
// mapping is not its own (`is_custom_mapping` not set): the media's first channel to the
// app's first, its second to the second, and so on, and those beyond the app's last
// round again from its first (a file of sixteen channels, written by ProPresenter for a
// workspace of two, has them 1, 2, 1, 2 and so on). A GUESS, not in its files: that
// sound of one channel goes to the app's first two, as a stereo system plays it in the
// middle.
Routes autoMedia(int mediaChannels, int appChannels);

// ProPresenter's Auto for the device: the app's first channel to the device's first,
// and so on, as far as both go.
Routes autoDevice(int appChannels, int deviceChannels);

// `routes` for `inputs` inputs and `outputs` outputs: an input it does not have goes
// nowhere, and an output beyond the last is let go of.
Routes fitted(const Routes &routes, int inputs, int outputs);

// `routes` with input `input` sent to output `output` if it was not, and not if it was
// (a click on a cell of the matrix), at `inputs` by `outputs`.
Routes toggled(const Routes &routes, int input, int output, int inputs, int outputs);

// Whether two matrices send every input to the same outputs (in any order)
bool same(const Routes &a, const Routes &b);

// Which of the app's channels are heard: one that is not muted, and, if any is soloed,
// one that is soloed. (ProPresenter's Mute and Solo; how they go together is a GUESS:
// as on a mixing desk.)
QList<bool> heard(const QList<bool> &muted, const QList<bool> &soloed);

// How much of a channel's sound is let through, from its volume in decibels and whether
// it is muted: 0 dB is as it is. (A GUESS that ProPresenter's channel volumes are in
// decibels: every channel of every media in the maintainer's files has 0, and each is
// heard.)
float gainOf(double decibels, bool muted);

// How loud a media's sound is by its ramps, `sinceStart` seconds after it began to play
// and `untilEnd` seconds before where it will stop (less than 0 for an end that is not
// known, or not there: one that goes round): coming in over its first `rampIn` seconds,
// and going out over its last `rampOut` (0 for a ramp that is not there). By the curves
// of the app's fades (audiolevel::fadeIn, fadeOut). A GUESS: that ProPresenter's ramps
// are of the sound alone, at the start of a playing and at its end.
double rampGain(double sinceStart, double rampIn, double untilEnd, double rampOut);

// What a player needs of a media's sound, from its file, as a map; empty for a media
// whose sound goes as it is (by Auto, every channel at 0 dB and not muted, no ramps),
// which is nearly every media, and which costs nothing to hand on:
//   outputs   where each of its channels goes, by the app's channels: only for one
//             whose mapping is its own
//   gains     how much of each channel is let through (gainOf), only where any is not 1
//   rampIn, rampOut   seconds, only where there is one
QVariantMap soundOf(const rv::data::Media::AudioProperties &audio, const rv::data::Media::TransportProperties &transport);

// The same of any piece of media: its audio and transport, for a video or a piece of
// audio; empty for anything else
QVariantMap soundOf(const rv::data::Media &media);

} // namespace audiorouting
