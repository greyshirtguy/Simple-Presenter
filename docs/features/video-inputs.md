[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Video inputs

A video input is live video coming into the computer, from a webcam, a USB capture
device or a capture card, put on the screens as ProPresenter puts one: on a **video
input layer** of its own, under the media, from the media bin, from a slide or a macro,
or as the fill of a shape. What it is, how it is shown and what is kept in the files
follow ProPresenter's own Video Inputs.

The devices are those Linux has for video (Video4Linux), as Qt Multimedia finds them:
a webcam, a USB capture stick, or a card whose driver gives it a /dev/video device.
Blackmagic's DeckLink inputs, and NDI sources, are not among them yet (see
[what is not there yet](../todo.md)).

## Setting them up

**Settings → Inputs** has the workspace's video inputs, as ProPresenter's Video Inputs
page does: one row each, with its thumbnail, its name and which device it is. **+ Add
Video Input** adds one, called Input and the first number not taken; a click on a row
opens it:

- **The preview**: what the input is sending now. The eye beside it switches the
  preview off, as ProPresenter's does.
- **Name**: what it is called everywhere, in the media bin, the Add Action menu and the
  editor.
- **Device**: which of this computer's devices it is, or None.
- **Mode**: the size and rate it is captured at. **Automatic** takes the largest the
  device has that is no larger than 1920 by 1080, and of those the quickest up to 30
  frames a second, uncompressed before compressed; it says what it took. Any of the
  device's own modes can be chosen instead.
- **Thumbnail**: the picture it is shown by in the media bin and in the slides'
  thumbnails: **Take from the Input** takes one from what it is sending now, **Choose
  a Picture…** takes a picture file, and **None** goes back to the input's colour.
- **Sound** and **Volume**: where its sound comes from, which is **Automatic** (the
  device's own, for a webcam or a capture device that has a microphone or an audio
  input of its own, found by its name among the computer's audio inputs), **No Audio
  Source**, or any of the computer's audio inputs; and how loud it is played. The
  sound plays while the input is on an audience screen, on the output the app's audio
  plays on (see [Audio](audio.md)).
- **Remove** takes it out of the workspace's inputs.

**What is kept where.** The list of inputs, their names and their order are the
workspace's, kept in ProPresenter's own file of the workspace, so a workspace's inputs
from ProPresenter are here, and one added here is in ProPresenter. Which device an
input is, its mode, its sound and its thumbnail belong to this computer (ProPresenter's
file names a Mac's devices, which are not this computer's), and are kept in the app's
own settings, for each workspace. A sound of None set in ProPresenter is where an input
starts here.

## Showing one

- **The media bin** has **Video Inputs** first in its list, above the media
  playlists, as ProPresenter has it. Picked, it shows the inputs by their thumbnails;
  a click on one puts it on the video input layer, and it is ringed while it is there.
  A right click has Show and the way to the settings.
- **A slide or a macro** shows one with a **Video Input** action: from **Add Action →
  Video Input**, which has the workspace's inputs, or by dragging an input out of the
  media bin onto the slide or the macro. The action's icon is ProPresenter's camera,
  and a right click on it changes which input it shows. In the file it is written as
  ProPresenter writes its own: a media action of live video, on the input layer, by the
  input's place in the list.
- **Clearing.** The video input layer has the last of the clear buttons beside the
  preview, under the media's, as in ProPresenter's column, with ProPresenter's picture,
  lit while an input is on it. Clear All clears
  it, and so does any [clear group](clear-groups.md) that has Video Input ticked.
- **Looks.** A [look](looks.md) can take the layer off one screen and leave it on
  another, as ProPresenter's Video Input row does.

What is on the layer is drawn on every audience screen the look gives it to, and in the
operator's preview of them, each frame of it on a screen and a tenth of a second apart
in the preview.

## As the fill of a shape

In the editor, a shape, a text box or anything else with a fill can be filled with a
video input: **Fill → Video Input**, then which input, and how it is scaled (Scale to
Fit, Scale to Fill, Stretch to Fill), as for media. This is for slides, themes, props
and stage layouts alike, so a camera can sit in a box beside the words, or a stage
screen can show the camera on the speaker. The input is drawn live in the editor, on
the screens and in the operator's preview, and cut to the shape's outline; in the
thumbnails of slides it is drawn as its thumbnail, to keep a grid of slides from
costing as much as the show. Its sound is not a shape's to play: that is the video
input layer's.

## The API

[ProPresenter's API](api.md) has the inputs too: `GET /v1/video_inputs` lists them,
`GET /v1/video_inputs/{id}/trigger` puts one on the layer (by its id, its name or its
place), the layers' status says whether one is on, and `clear/layer/video_input` clears
it.

## What it costs

An input is captured only while something shows it: the layer, a shape on a slide that
is drawn, or its preview in the settings. However many screens show it, it is captured
once, and each screen is handed its frames. It is let go a second and a half after the
last thing showing it has gone, so that going from one slide with it to the next does
not stop and start the camera. Nothing of it is made while no input is shown, and the
computer is not asked for its devices until an input is opened in the settings or is
first shown.

## Where it is not quite ProPresenter's, or is a guess

- **An input that sends nothing** (no device on this computer, or a device not plugged
  in) shows nothing: the black under everything, or whatever else is on the screen.
  What ProPresenter shows then has not been seen.
- **A shape's fill in a thumbnail** is the input's thumbnail; what ProPresenter's
  thumbnails show there has not been seen.
- **Where Video Input is** in Add Action and in the editor's Fill menu is a guess.
- **What the API sends of each input** is the id, name and place, which is what
  ProPresenter's other lists send; its own has not been seen. A Video Input Action in a
  macro is given the type ProPresenter's file has for it, media.
- **Not here yet**: ProPresenter's audio Delay (for sound that comes before its
  picture), its channel levels and routing, and the settings it has for a card's
  inputs; DeckLink and NDI inputs.
