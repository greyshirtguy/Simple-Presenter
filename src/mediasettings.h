#pragma once

#include "graphicsData.pb.h"

#include <QString>
#include <QVariantMap>

// What can be set about one piece of media in its Media Inspector, read from the file
// and written back: how it is fitted to the place it is shown in, turned, flipped and
// cropped, the part of a video that plays and the frame its picture is taken from, and
// the effects it is drawn with. It is the same for a slide's media, a media bin's row
// and an element filled with media, since all three hold a ProPresenter Media element,
// and these are fields of that element (graphicsData.proto: Media's DrawingProperties,
// TransportProperties and VideoProperties).
//
// Plain rules, with nothing of the windows in them (tests/unit/tst_mediasettings.cpp);
// the inspector (qml/MediaInspector.qml) shows what describe() gives and hands back
// changes in the same terms, and what draws media (qml/MediaLook.qml) goes by the same
// map. Whether the media is a background or a foreground is said by the action that
// holds it, not by the media: workspace::MediaBehaviour reads how a piece of media is
// played from both, and is what plays it goes by.
//
// What the file holds of each, as ProPresenter 21.4 writes them, and where that is read
// from the files of the user's workspaces:
//  - fitting: `scale_behavior`, Fit, Fill or Stretch (or Custom: a size set by dragging
//    the media about in ProPresenter's editor, which is drawn here as Fit). A GUESS:
//    that Scale + Blur is Fit with `is_blurred` set, the field beside it.
//  - turning: `native_rotation`, 0, 90, 180 or 270 degrees clockwise, and the two flips.
//  - cropping: `crop_enable` and `crop_insets`, in pixels of the media at its own size
//    from each edge (as ProPresenter wrote them for a 1920 by 1080 video: 77 from the top
//    and 209 from the bottom). Switching cropping off keeps the insets, as the file does.
//  - trimming: the transport's `in_point` and `out_point`, in seconds, and `end_point`,
//    the media's length: ProPresenter writes the out point and the end point both at the
//    length for a video that is not trimmed, and this app's own media has neither.
//  - the picture of a video: `thumbnail_position`, in seconds; ProPresenter has -1 where
//    none was chosen, and 3 for many of the user's videos (a GUESS: its default then).
//  - effects: a list of them (`effects`), each with an id of its own, its catalogue id
//    (`render_id`, see mediaeffects.h), whether it is on (`enabled`), and its variables
//    by name, each with its value and its least, most and default. ProPresenter's files
//    have effects that do not say they are on, beside one that does: those are taken
//    to be off, as the field says (a GUESS that this is how ProPresenter shows them).
namespace mediasettings {

// The settings, as a map with:
//   scale         0 Scale to Fit, 1 Scale to Fill, 2 Stretch to Fill
//   blurred       Scale + Blur: fitted, on a blurred copy of itself that fills
//   rotation      0, 90, 180 or 270
//   flipH, flipV
//   cropOn, cropLeft, cropRight, cropTop, cropBottom
//   naturalWidth, naturalHeight   its size as the file says it, 0 where it does not
//   inPoint       seconds; 0 from the start
//   outPoint      seconds, as the file has it; see trimmedOut
//   endPoint      seconds: the length as the file has it, 0 where it does not say
//   trimmedOut    whether the out point is before the end (and so is where it stops)
//   thumbnail     seconds into a video, as the file has it; 0 or less for none chosen
//   playback      0 stop at the end, 1 loop, 2 loop a number of times, 3 loop for a time
//   effects       [{ id, on, name, renderId, known, shader, animated, variables, numbers,
//                    colors }]: `variables` are [{ name, kind ("int", "float", "double",
//                    "color" or "other"), value, least, most, initial, slot }] in the
//                    file's order, `slot` being which of `numbers`, or of `colors` for a
//                    colour, it is (-1 for none); `shader` is as mediaeffects has it ("" for one not drawn
//                    here); `numbers` and `colors` are what the shader is given: the
//                    effect's numbers and its colours, each in the catalogue's order
//                    (taken from the variables of the same name, or the catalogue's
//                    initial value for one the file does not have)
//   drawn         whether any of it changes how the media is drawn: a crop, a turn, a
//                 flip, a blur or an effect that is on and drawn here (fitting is
//                 always done, and is not counted)
// A still has no transport and no thumbnail: its inPoint, outPoint and endPoint are 0,
// its thumbnail -1 and its playback 0.
QVariantMap describe(const rv::data::Media &media);

// Changes the fields of the media that `changes` names, by the keys describe() gives,
// and nothing else of it. `effects` is the whole list, in the order it is to have: an
// effect is known by its `id`, and keeps everything the file has of it but whether it
// is on and the values of its variables (matched by name), which are set from the map;
// one with an id the media does not have is added, made from the catalogue by its
// `renderId` (newEffect gives such a map), and one the list leaves out is taken off.
// The trim and the thumbnail are only for a video. Returns an error message, empty on
// success, in which case nothing has been changed.
QString apply(rv::data::Media *media, const QVariantMap &changes);

// Whether the media is set to anything at all: fitted otherwise than to fit (unless
// `countFitting` is false: an element's fill says how it is fitted by a key of its own),
// turned, flipped, cropped, trimmed, its picture's moment chosen, or given an effect.
// Looked at in the file's fields, without describing them. Media set to nothing is
// handed to QML with no settings, and is drawn as media always was: describing them
// for every slide's media and every element filled with media, and handing that map of
// a score of things to QML, cost the benchmark's slides of shapes 7 ms a click. The
// inspector reads them whole when it is opened (Catalog::slideMediaSettings and the
// rest).
bool isSet(const rv::data::Media &media, bool countFitting = true);

// An effect from the catalogue, by its id, as describe() gives one: switched on, with a
// new id of its own and every variable at its initial value. An empty map for an id the
// catalogue does not have.
QVariantMap newEffect(const QString &renderId);

// Gives media that is replacing other media (another file chosen in the inspector) the
// settings the other had that still make sense for it: how it is fitted, turned and
// flipped, its effects, and how a video plays on from its end and how loud it is, when
// both are videos. Not its crop, which was in the other file's pixels, nor its trim or
// its thumbnail, which were the other file's times, nor its size.
void carryOver(const rv::data::Media &from, rv::data::Media *to);

} // namespace mediasettings
