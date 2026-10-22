[Simple Presenter](README.md)

# Every version

What each version brought, the newest first. Each version's number leads to its release,
which has the package and the notes written for it at the time. The note at the top of
the [README](README.md) holds for every one of them: an experiment, not a product for use.

## [0.107](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.107) · 2026-10-22

- [Playback markers](docs/features/playback-markers.md), as ProPresenter has them: points
  in a video of a slide or of the media bin, or in a piece of the audio bin, whose actions
  run as playback reaches them. Made in the Media Inspector's Playback Markers tab, renamed,
  coloured (a colour at random to begin with, or any colour), given actions, duplicated,
  moved to the player or dragged along the bar by their flags; shown on the transport's bar
  with a Marker menu; and a stage text box can show a marker's name or the time to it.
- [The copyright display](docs/features/copyright.md), as ProPresenter's is: a song's
  copyright laid over its first, last or every slide on the audience screens, for a
  church's CCLI licence. Set up in Settings → Copyright, with a layout of the song's
  details and a template edited in the editor; a presentation's details are typed in the
  editor's new Copyright tab. Kept in ProPresenter's own files.
- ProPresenter's pictures for the audio tabs and for the transport's presentation tab.

## [0.106](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.106) · 2026-10-21

- [Next Cue](docs/features/media.md#next-cue) for a video in a media playlist, as
  ProPresenter has it: None, Next, Random, First, or another row of its playlist, from its
  right-click menu; followed when it was played from its playlist and has played to its
  end, and made to stop at its end so that it gets there.
- [Tabs over the transport](docs/features/media.md#the-transport): the video on the media
  layer, the video on the announcements layer, or the audio bin's music, each worked by the
  same transport.
- The README's screenshot taken again.

## [0.105](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.105) · 2026-10-21

- [Video inputs](docs/features/video-inputs.md): the computer's webcams and capture
  devices as ProPresenter's video inputs, set up in Settings → Inputs (the list kept in
  ProPresenter's own file, the devices in this computer's settings), with their sound.
- Shown from the media bin's Video Inputs, on a layer of their own under the media, with
  a clear button of their own; cleared by Clear All and the clear groups, and switched
  for each screen by a look.
- A Video Input action for slides and macros, from Add Action or dragged out of the bin;
  a video input as the fill of a shape, live; and the inputs in the HTTP API.
- The README's screenshot taken again.

## [0.104](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.104) · 2026-10-21

- [One preview, of any screen](docs/features/screens.md): the screen picked under it,
  audience or stage, a stage screen with its layout and its Screen Previews; kept for
  the workspace.
- Its area dragged taller or shorter by the line under it, for a tall layout of several
  Screen Previews; a double click gives it back its 16:9.
- [The clears](docs/features/media.md) in two columns beside the preview, pictures and no
  words, as ProPresenter has them: the clear groups, sharing their column's height, and
  the layers in ProPresenter's order.

## [0.103](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.103) · 2026-10-20

- [Media Playlist and Audio Playlist actions](docs/features/actions.md): a playlist of the
  media bin or of the audio bin played from its first row, and left to play on when it is
  playing already, unless the action's Always Retrigger is on.
- [Message actions](docs/features/actions.md) made here: a message shown with words of the
  action's own for its tokens, set in a small panel.
- Each picked from Add Action, or dragged onto a slide or a macro out of its list.

## [0.102](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.102) · 2026-10-20

- [Communication devices](docs/features/devices.md): a slide or a macro sends a MIDI note
  or a RossTalk Custom Control, written into the file as ProPresenter writes them, and
  ProPresenter's own devices for the workspace are offered in Settings → Devices.
- ProPresenter's MIDI Map: a MIDI device's notes work the show.
- HTTP Requests, a kind of device of this app's own: a GET, POST, PUT or DELETE, with a
  body and headers, to a web address.

## [0.101](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.101) · 2026-10-19

- [Builds](docs/features/builds.md): the elements of a slide coming on one after another
  and going again, With Slide, After Previous, With Previous or On Click, set up in the
  editor's Build tab.
- A theme's builds, which a look runs on the screen it gives the theme to.
- A slide's own transition, or its presentation's, made in place of the one chosen.

## [0.100](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.100) · 2026-10-19

- [The Media Inspector](docs/features/media.md#the-media-inspector), for a slide's media,
  a row of the media bin and an element filled with a picture: another file in its
  place, a video's thumbnail from any moment, scaling, turning, flipping and cropping,
  how a video plays on, and ProPresenter's effects.
- [Quick Edit](docs/features/editing.md#quick-edit): a slide's words put right over its
  thumbnail, without the editor.

## [0.99](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.99) · 2026-10-18

- [ProPresenter's HTTP API](docs/features/api.md), switched on in Settings → Network: its
  state, its streams of changes, and what operates the show.

## [0.98](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.98) · 2026-10-17

- [Blackmagic DeckLink output](docs/features/decklink.md): a screen out through a card's
  output in one of ten formats, keyed internally or externally with ProPresenter's alpha
  settings. Tried so far on one card.
- Screens going out through cards are drawn and handed over together, so that their
  outputs change together.

## [0.97](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.97) · 2026-10-16

- [Screen Preview](docs/features/stage-layouts.md#screen-preview): a shape filled with
  what one of the screens is showing, video and all.
- Copy Text Style and Paste Text Style, in a slide's menu. See [Editing](docs/features/editing.md).
- A paste into a text box keeps the box's own format and alignment.
- A green ring round the slide that is live on the announcements.
- Put right: a screen sent over NDI going out black after being switched off and on, and
  a song dressed in a theme losing its chords.

## [0.96](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.96) · 2026-10-16

- [Color Triggers](docs/features/timers.md) for a text box linked to a timer.
- Three public-domain [Bibles](docs/features/bibles.md) come with the app.
- Bible mode reworked: a verse added before or after the passage keeps what is live as it
  is, and the options are in a panel.
- Text boxes set to Single Line keep each word's look.

## [0.95](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.95) · 2026-10-15

- [Bible mode](docs/features/bibles.md): a passage typed is made into slides in a theme,
  with ProPresenter's Bible options, from a Bible imported as an XML file or read from
  api.bible.
- [A passage shared out evenly](docs/features/bibles.md#how-the-words-are-flowed) across
  its slides, broken where it pauses.
- The chord sheet as the Mac and Windows chord editors have it now: a line of hints, the
  line the pointer is on in orange, and sharps and flats drawn as signs.

## [0.94](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.94) · 2026-10-14

- [The announcements layer](docs/features/announcements.md): a presentation on a layer of
  its own, over the slides, while the show stays at the slide it was at.
- [Go To Next timers and the Slide Show](docs/features/go-to-next-and-slide-show.md).
- [Masks](docs/features/masks.md), drawn in the editor and given to a screen in the Looks
  window.
- A name typed where it stands, in the editor's list.

## [0.93](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.93) · 2026-10-14

- [The reflow editor](docs/features/reflow.md): the slides' words as a table. A slide is
  split where the caret is, and two are joined, keeping their fonts, sizes, colours and
  chords.

## [0.92](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.92) · 2026-10-13

- [The arrangement editor](docs/features/arrangements.md): groups dragged into an
  arrangement, along it, or out; arrangements made, copied, renamed and removed.
- The presentation's name in the middle of the line over the slides.

## [0.91](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.91) · 2026-10-13

- [Messages](docs/features/messages.md), with tokens for a name, a room, the time of day
  or a timer, and a layer and a clear button of their own.
- [The stage message](docs/features/stage-layouts.md).
- The order of a slide's elements, by dragging a row of the editor's list. See
  [Editing](docs/features/editing.md).

## [0.90](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.90) · 2026-10-13

- [The audio bin](docs/features/audio.md), with a clear for the audio and a meter in the
  preview.
- A slide's notes, in a pane under the slide in the [editor](docs/features/editing.md).
- What a text box shows, picked from one menu, which has a slide's notes, the system clock
  and a video countdown in it.
- A chord dragged out of the strip onto a word, in the chord editor.

## [0.89](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.89) · 2026-10-12

- [The chord editors](docs/features/chords.md#the-chord-editor) as they came back from the
  chord editor made of them for the Mac and for Windows: a grid of slides that zooms, each
  in a frame of its group's colour; the song's two keys over both editors; Autosave and
  Revert; and a colour for the chords.

## [0.88](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.88) · 2026-10-12

- [A slide by its number](docs/features/keys.md): typed while showing, and Enter takes it
  live.
- Ctrl+, opens the settings.

## [0.87](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.87) · 2026-10-12

- [A guard for a graphics driver that hangs](docs/installing.md#decoding-by-the-processor-alone)
  when its video decoder is tried (an NVIDIA card on the nouveau driver, without its
  firmware), and a notice that says so once.
- Settings → Video: decoding kept to the processor, or the graphics chip's decoder tried
  whatever the app makes of the computer.

## [0.86](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.86) · 2026-10-11

- [Clear groups](docs/features/clear-groups.md): a button each under the clears, a window
  to set them up in, F1 for the first, and actions that clear a group.

## [0.85](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.85) · 2026-10-11

- [Group and Label](docs/features/groups-and-labels.md), in a slide's menu.
- A presentation renamed, with the playlists that have it, or moved to the Trash.
- A box under the list of presentations that narrows it to the names that have what is
  typed.
- [Simple View](docs/features/simple-view.md) can keep the previews and the show controls.

## [0.84](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.84) · 2026-10-11

- New presentations: the + beside Libraries makes one, with a name of your choosing and
  one slide to start on. See [Editing](docs/features/editing.md).

## [0.83](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.83) · 2026-10-10

- [Chords](docs/features/chords.md) on a slide with no words, for an intro or an
  instrumental.
- Nothing is added to a slide until a chord is put on it, and taking the chords off again
  leaves the slide as it was found.

## [0.82](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.82) · 2026-10-10

- [Looks](docs/features/looks.md) as ProPresenter has them: the live look by name in the
  toolbar, the Looks window, and a live look of its own.
- ProPresenter's own pictures on the buttons and the actions.
- [What an AI coding agent is told](docs/ai-help.md), so that it can help someone find out
  what is wrong on their own computer.

## [0.81](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.81) · 2026-10-10

- In the [chord editor](docs/features/chords.md#the-chord-editor), a click on the next
  place keeps the chord being typed and starts the next one.

## [0.8](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.8) · 2026-10-10

- [Chords](docs/features/chords.md): shown on the stage in any key, put on by hand in two
  editors, and brought in from ChordPro files.

## [0.7](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.7) · 2026-10-09

- [Looks](docs/features/looks.md), which say what each audience screen gets.
- [Themes](docs/features/themes.md), to dress slides in.
- [Search](docs/features/search.md), by name or by words.

## 0.62 · 2026-10-09

Not released by itself: its package was 0.7's.

- Setting a screen to [NDI](docs/features/ndi.md) without NDI's library offers to fetch it,
  showing NDI's licence to be agreed to first.

## [0.61](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.61) · 2026-10-09

- The licence is now MIT. Nothing else is different from 0.6.

## [0.6](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.6) · 2026-10-09

- Up to sixteen [screens](docs/features/screens.md), for the audience and for the stage,
  each sent to a window, a display or the network as an [NDI](docs/features/ndi.md) source.
- The screens are kept awake while the show is on one.

## [0.51](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.51) · 2026-10-09

- A slide is shown before its actions are done, so a slide that clears itself is cleared,
  and is still marked as the live slide.
- A macro's colour is a block round its picture.

## [0.5](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.5) · 2026-10-09

- [Actions](docs/features/actions.md) on slides and macros, for timers, clearing, the
  stage, props and macros. Those a ProPresenter presentation already has are run.
- [Macros](docs/features/macros.md), on a tab of their own.
- How a video plays on from its end (stop, loop, a number of times, for a length of time),
  shown on the slide and changed from the slide.
- Menus that lead to more menus open them alongside.
- [Props](docs/features/props.md) shown a collection at a time, and put in order by
  dragging.

## [0.4](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.4) · 2026-10-08

- Shapes in the [editor](docs/features/editing.md) (rectangles, rounded rectangles,
  ellipses, arrows), with colour, gradient and picture fills and feathered edges; elements
  turned; slides added, copied, pasted and deleted.
- Hotkeys for groups, kept where ProPresenter keeps them. See [Keys](docs/features/keys.md).
- Show mode and edit mode, each with a button and a key.
- The [benchmark](docs/how-it-is-made.md#benchmark), for telling whether a change has made
  the app slower.

## [0.3](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.3) · 2026-10-07

- [Simple View](docs/features/simple-view.md): the slides take the whole window.
- A foreground video is played with its sound.
- [Media](docs/features/media.md) dragged in from the file manager: onto a slide, between
  two slides, or into the media bin.
- A video's thumbnail is a picture from a little way into it, not its first frame.
- The output and stage windows are kept out of Alt+Tab.

## [0.2](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.2) · 2026-10-07

- [Media](docs/features/media.md) as a background or a foreground, with a transport for
  the video that is playing.
- [Timers](docs/features/timers.md), and text boxes that show them.
- [Props](docs/features/props.md) and [stage layouts](docs/features/stage-layouts.md).
- Text that scales to fit its box.
- A [log](docs/features/log.md) of each run, for working out what happened when something
  goes wrong.

## [0.1.0](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/v0.1.0) · 2026-10-06

- The first release: presentations and media shown, playlists, and an editor for text
  boxes.
