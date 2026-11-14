[Simple Presenter](README.md)

# Every version

What each version brought, the newest first. Each version's number leads to its release,
which has the package and the notes written for it at the time. The note at the top of
the [README](README.md) holds for every one of them: an experiment, not a product for use.

## [0.139](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.139) · 2026-11-14

- ProPresenter's timeline: the Timeline button beside Arrangements opens it under the
  line over the slides, with its ruler, its tracks of slides, media, audio and actions,
  and Rewind, Play, Record, Loop and the Duration. Played, its slides go live and its
  media, audio and actions are done at their times; played from part way, the slide
  before goes live and the audio and video under the playhead start where it is in
  them. With Record on, what is clicked is put on it. Slides are carried up onto it
  from the slides, and pictures, videos and audio dragged onto it from the bins; cues
  are dragged along it and taken off by their menu. It is kept in the presentation's
  file as ProPresenter keeps it, read from either of its two lists of cues
  ([Timeline](docs/features/timeline.md)).
- The timeline is played, paused and rewound by ProPresenter's API routes for it
  (those ProPresenter Remote and Bitfocus Companion's module ask by; not tried with
  either), and by MIDI's Start, Stop and Rewind Timeline; a clear group's Stop
  Timeline stops it.
- A light guard against dragging by accident, everywhere in the operator window: a
  drag starts only once the pointer is twelve pixels off with the button held for
  150 ms, so a click made while the pointer is still moving stays a click.
- A drag started from the show controls is no longer offered first to wherever the
  last one was let go.

## [0.138](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.138) · 2026-11-14

- ProPresenter Remote, Renewed Vision's app for a phone or a tablet, works with the
  app as with ProPresenter: Settings → Network, Enable ProPresenter Remote, with a
  controller's password, which may operate the show, and an observer's, which may only
  follow it. The app is advertised on the network as ProPresenter is (through Avahi),
  and the Remote's messages are encrypted as ProPresenter's are; it asks by the API's
  own routes. Building the app now needs `libssl-dev`
  ([ProPresenter Remote](docs/features/api.md#propresenter-remote)).
- Pictures of slides through the API, as ProPresenter gives them and the Remote shows
  its slides by: by a presentation's UUID or a playlist's item, at the size asked, JPEG
  or PNG, with the slide's media behind it and ProPresenter's checkerboard where it is
  transparent. They are drawn by a second process of the app's at idle priority, stopped
  outright while anything goes live, so that a slide's change and a video under it pay
  nothing for them ([Pictures of slides](docs/features/api.md#pictures-of-slides)).
- With Enable Network on and no client, every slide taken live cost the app's thread
  some 2 ms more: it no longer does.
- Music or a sound started under the logo of Clear to Logo leaves the logo up
  ([Clear to Logo](docs/features/clear-groups.md#clear-to-logo)).
- The benchmark times a video with pictures of slides being made, and slides changed
  over it (benchmark/README.md).

## [0.137](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.137) · 2026-11-13

- Easy View and Table View beside the grid, picked by three buttons at the right of the
  bar under the slides, before the size buttons, the one in use lit. Easy View shows each
  slide's words alone, large, over a colour, without drawing the slides; Table View a row
  for each slide, its words and its notes beside its thumbnail, the size buttons making
  the rows larger or smaller. A … after the three opens the view's format: font, size,
  colour, bold, italic, alignment and background, with Easy View's Show Media and Table
  View's Truncate; in the grid it is dimmed
  ([Grid, Easy View and Table View](docs/features/slide-views.md)).
- Clear to Logo, on F12 and in the clear buttons' right-click menu: everything is cleared
  and the logo chosen in Settings → Screens, a picture or a video, is put up on the media
  layer, until anything else comes on (a slide, media, a prop, a message, an
  announcement, a video input or audio). ProPresenter's "clear to logo" action in a slide
  or a macro does the same ([Clear groups](docs/features/clear-groups.md#clear-to-logo)).

## [0.136](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.136) · 2026-11-12

- The two transition duration sliders, the slides' and the media's, have a see-through
  handle: a faint fill with a light ring, so that the marks and the track show through it
  as it is slid onto them, the orange of the track ending at its middle. The marks are
  drawn across the middle of the track instead of under it, and fainter. Other sliders
  are as they were ([Transitions](docs/features/transitions.md)).

## [0.135](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.135) · 2026-11-11

- A look and a layer changing together. With a look live that kept the slides from a
  screen (a stream), a slide whose action went over to a look that gave them back showed
  the slide before it there, dissolving away, and for a moment even with a cut. A layer
  the look gives back to a screen now comes back with what is being put on it, which
  comes in from nothing by its own transition; and a layer the look takes from a screen
  by a fade goes with what it had, a slide or an announcement that changes in that
  moment being kept from that screen until the layer has gone
  ([Looks](docs/features/looks.md)).
- **Edit Action**, in a slide's menu between Add Action and Remove Action, in a playback
  marker's, and for each action in a macro's: an action that is there is changed for
  another of its kind, picked from a list with the one it is ticked. A macro, a look, a
  prop (and whether it is put on or taken off), what a clear clears, a clear group, a
  video input, and a stage layout for each stage screen. The menu of an action's own
  icon has the same ([Actions](docs/features/actions.md)).
- An action changed where it is keeps whether ProPresenter had it enabled (it was
  enabled by any change); and a clear changed from the announcements to a layer no
  longer names the announcements.
- A look that also changes a screen's theme has the slide that came with it shown there
  once, in the theme, where it was shown twice.

## [0.134](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.134) · 2026-11-11

- A PowerPoint file (`.pptx`) dragged in from the file manager and dropped on a library in
  the list, or on a playlist, comes in at once as a presentation, with nothing asked: into
  that library, or into the library the last import went into and added to that playlist.
  Several dropped together come in one after another, and the panel says what came of
  each ([Import PowerPoint](docs/features/import-powerpoint.md)).
- The sliders for how long the slides' and the media's transitions take are marked at
  each second and, less, at the halves, to slide to by eye; and with Shift held while
  dragging they move a tenth as far as the pointer, for a value between the marks
  ([Transitions](docs/features/transitions.md)).

## [0.133](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.133) · 2026-11-11

- The − and + that make the media bin's thumbnails smaller and larger are in the bar
  under the thumbnails, at its right, beside the media transition at its left, as the
  slides' are in the bar under the slides; they had floated over the thumbnails' bottom
  corner. That bar is now as tall as the slides' ([Media](docs/features/media.md)).

## [0.132](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.132) · 2026-11-10

- A PowerPoint file (`.pptx`) comes in as a presentation of the workspace, from
  Import PowerPoint… under the `+` beside Libraries: slide for slide, its words as
  text boxes that can be edited, with what a placeholder takes from its layout, its
  master and the theme, and its lines falling where PowerPoint's do; its shapes with
  their fills, lines and shadows; its pictures cropped as they were placed, and its
  videos; its slide backgrounds as the slides' background media, or left out by the
  panel's switch; its speaker notes, its sections as groups and its hidden slides as
  disabled ones. Pictures and videos go under the workspace's Media/Imported. What
  cannot come over (tables, charts, SmartArt, transitions and animations) is listed
  at the end, by slide, with the fonts not installed here
  ([Import PowerPoint](docs/features/import-powerpoint.md)).

## [0.131](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.131) · 2026-11-09

- Coming back to the presentation that is live, after looking at another, puts the
  slides' grid where its live slide is in view, at once, as the setting for following it
  would have it; it went to the top. The same with the slide cleared, since stepping goes
  on from it.
- The menus that list actions (Remove Action on a slide, a marker or a macro, and a
  macro's own rows of them) call each by its name beside its icon, where they said what
  it does in a sentence: a timer's with what it does to the timer after its name, a
  prop's that is cleared with "(Clear)" ([Actions](docs/features/actions.md)).

## [0.130](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.130) · 2026-11-09

- The slides' grid starts to move to the live slide as the slide changes, where it waited
  for the slide's transition and 150 ms more.
- With Next held down, the grid keeps up with the slides as they go, where it did not
  move until the key was let go.
- A jump to a slide out of view (a hotkey, a typed number, the API) takes a tenth of a
  second, whatever is set for a step.
- A slide triggered through the [API](docs/features/api.md) in the presentation being
  viewed moves the grid to it, as a click does.

## [0.129](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.129) · 2026-11-09

- Importing an exported playlist asks first where some of its presentations are in the
  library already, or a playlist here has its playlist's name: **Replace Them** (the old
  presentations kept among the editor's backups, each playlist put in place of the one of
  its name) or **Keep What Is Here**, as it always did
  ([Workspaces](docs/features/workspaces.md)).
- Media a slide brings comes in by the media transition, as media played from the bin
  does, and no longer by the slide's.
- The media transition's controls are under the media bin's thumbnails, and beside the
  slides' while the bin is hidden or in Simple View; the slides' are on a faint orange
  and the media's on a faint purple ([Transitions](docs/features/transitions.md)).
- **Ctrl+=** and **Ctrl+-** make the thumbnails under the pointer larger and smaller,
  the media bin's or the slides'.
- Settings → Slides has how long the slides' grid takes to move to the live slide, and
  whether it eases in and out of the move (on unless switched off).

## [0.128](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.128) · 2026-11-08

- A shape can be filled with a still picture of the slide that is live, or of the one
  after it: ProPresenter's stage layouts' Current Slide and Next Slide, which were left
  empty, are drawn, and the editor's Fill menu has **Slide Image**
  ([Stage layouts](docs/features/stage-layouts.md#pictures-of-the-slides)).
- A picture of a screen shows the pictures that screen shows, one level down, never of a
  screen it is already inside: a stage screen's picture of another stage screen has that
  one's Screen Previews and pictures of the slides in it.
- The operator's preview of a stage screen is drawn as the stage draws it: shadows,
  faded edges, a timer's hundredths, a video input at every frame.
- The slides' grid moves smoothly to follow the live slide, a moment after the change;
  Settings → Slides has when: **Early** (the row after a live slide in the bottom row
  brought into view) or **Just in Time**.

## [0.127](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.127) · 2026-11-08

- A slide with a background video has a speaker icon under its other two, struck
  through while the background is muted, which a background is here. A plain click on
  it has the background heard, and another mutes it again, at once if it is playing; it
  stays a background. This is the app's own, kept in the workspace's folder in
  `SimplePresenter/Background Sound.json` ([Media](docs/features/media.md)).
- Media dropped on a slide labels the slide with the file's name, in place of the label
  it had; and dropped on a slide that has media, it plays as that did: its layer, how it
  plays on from its end, how loud, how it is fitted and its effects.
- Next (Space, Right, the API's next) after the slide is cleared is the slide after the
  one cleared, not the cleared one again; Previous is the one before it.
- The icon that says whether media is a background or a foreground is drawn as
  ProPresenter draws it: the foreground's layer at the top left, the background's at the
  bottom right.
- A slide that goes live in the bottom row in view brings the next row into view.

## [0.126](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.126) · 2026-11-08

- The media bin has a [transition](docs/features/transitions.md) of its own, chosen with
  the same controls at the foot of the bin, as ProPresenter's media bin has at its bottom
  left: media played from the bin, or by an action, comes in by it, so that a background
  can cut in while the slides dissolve, or the other way about. Media a slide brings with
  it comes in with the slide, by the slide's transition (a guess at ProPresenter).
- Rows of a [playlist](docs/features/workspaces.md#rows-of-a-playlist) are copied and
  pasted: Ctrl+C copies the selected rows and Ctrl+V pastes them under the row selected,
  while the rows are what was last clicked, and a row's menu has Copy and Paste too. A
  pasted row is the row it was copied from, its arrangement and all, and the copies can
  go into another playlist.
- A slide's menu adds an action to every slide selected, when the slide clicked is among
  them, the file written once; the menu says so.

## [0.125](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.125) · 2026-11-08

- Rows of a [playlist](docs/features/workspaces.md#rows-of-a-playlist) are selected
  together: a click with Ctrl held selects a row as well or lets it go, one with Shift
  held every row from the last one selected to it, and a heading can be selected too;
  what is open stays open. Delete takes the selected rows out, and the menu's Remove
  does the same for all of them. The [search](docs/features/search.md) adds a
  presentation under the row selected, says so, and lights the new row for a moment.
- A meter beside the preview for each of the app's channels, where there were two bars
  of what the device was sent: how many, from two to sixteen, and how solid they are,
  are settings on the [Audio](docs/features/audio.md) page. They are laid out as
  ProPresenter's: two up the left edge for two, two up each edge for three or four,
  four up each for five to eight, and above eight two to a column at half the height.
- A song's words brought in from
  [SongSelect](docs/features/workspaces.md#a-songs-words-from-songselect), from the
  lyrics file it gives out or the words copied to the clipboard: a panel asks the lines
  on a slide, a theme slide to dress the slides in, the library and a playlist, and
  remembers the last three; the song's details go where the
  [copyright display](docs/features/copyright.md) looks.
- The network [API](docs/features/api.md) sets a timer up by PUT, by itself or along
  with starting, stopping or resetting it, which is what Companion's Timer Set sends.
- The menu under a slide's media icon plays the media from its Media caption, as the
  slide's own menu does; and a drag from another application that the app refuses is
  said in the [log](docs/features/log.md), with what came.

## [0.124](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.124) · 2026-11-06

- A timer's or a clock's text on a slide over a playing video is changed as the video's
  frames come, and drawn in them: each change was a frame of the output's own, which could
  put the video's next frame on the screen a refresh late. The fades of a look's layers,
  and of props and messages, keep to the screen's own frames too, where they were Qt's
  animations, stepped by a timer out of time with the screen. See
  [Built for modest hardware](docs/how-it-is-made.md#built-for-modest-hardware).
- The network [API](docs/features/api.md) keeps no more of one client than it should: what
  a client sends down its stream's connection is no longer kept, a client that does not
  read its answers is let go, sixty-four clients are kept at most, and one that is quiet
  for two minutes is let go. Its page says that it listens on every network the computer
  is on, and answers any web page.
- A playlist bundle's entry that inflates to more than it says it is, is found out at that
  point, and not after it has filled the memory or the disk.
- The benchmark's baseline is recorded afresh, at this version, with every line the
  benchmark now has.

## [0.123](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.123) · 2026-11-05

- How long the ~ key is held to switch [Simple View](docs/features/simple-view.md) is a
  setting, **Hold ~ for** in Settings → Simple View: a slider and a box, in seconds, from 0
  to 2. It is 0.56 s to begin with, where it was 0.7 s; at 0 a press of the key switches
  the view at once, and held on, the key does not switch it back.

## [0.122](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.122) · 2026-11-04

- A picture or a video can be dragged out of the media bin onto a
  [macro](docs/features/macros.md), as a piece of the audio bin could: held over the macros'
  tab, it brings the tab forward; dropped on a macro, it gives the macro an action that
  shows it on the media layer, as a click on it in the bin does; dropped on the strip along
  the bottom of the macros, it is given a macro of its own, called what its file is called.
- A playback marker's action that shows a picture or a video is done, where it was kept
  and not done.

## [0.121](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.121) · 2026-11-01

- A video playing on an audience screen is kept moving as slides change over it. A slide's
  words, and their shadow, are painted before it is shown, on threads of their own; the
  next slide, the one before and the first slide of each group are painted after each
  change, ahead of being asked for; and a change over a playing video is made with the
  video's next frame. On the 2017 laptop a cut held a frame of the video too long about one
  time in ten, and a dissolve one in six, where most did. See
  [Built for modest hardware](docs/how-it-is-made.md#built-for-modest-hardware).
- The stage and the operator's pictures of the screens paint their slides' words the same
  way, and are not waited for; the stage's plain view takes a slide's words a sixteenth of
  a second after it.
- Two clicks in quick succession over a playing video show the second slide only.

## [0.120](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.120) · 2026-10-31

- A box filled with a blur or an invert of what is behind it is set in the editor's Shape
  tab, as ProPresenter has it: Background Blur and Background Invert in its Fill menu, a
  blur with the Text tab's Saturation and Blur, its amount shared by every blur on the
  slide. See [Text fills](docs/features/text-fills.md#setting-them).
- A media fill of words has ProPresenter's gear beside the file's name, for another file,
  and its magnifier, which shows the file in the file manager.
- An empty space no longer stays under a fill's menu after a blur is changed for another
  fill.

## [0.119](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.119) · 2026-10-30

- Text fills: letters ProPresenter fills with a gradient, a picture, a blur or an invert
  of what is behind them, or cuts out of their box's colour, and boxes filled with a blur
  or an invert of what is behind them, drawn as ProPresenter draws them, on the screens,
  in the operator's preview, in the thumbnails and in the editor; the blur measured
  against ProPresenter's own pictures. See [Text fills](docs/features/text-fills.md).
- The editor's Text tab has ProPresenter's Fill menu (Text Color, Text Gradient, Text Cut
  Out, Media Fill, Background Invert, Background Blur), each with ProPresenter's controls
  under it; a blur's amount is shared by every object on its slide, as ProPresenter
  shares it. Fills go with the words when they are edited, and into the file as
  ProPresenter writes them, everything it wrote of them kept.

## [0.118](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.118) · 2026-10-29

- Export to PowerPoint: a right click on a presentation, in a library or a playlist,
  writes it as a PowerPoint file, slide for slide as it is shown, with its words as text
  boxes that can be edited, its media behind them (kept on the slides after, as
  ProPresenter keeps it up), its notes as speaker notes and its groups as sections, with
  ProToPPTX's options and a report of what could not be carried over. It is the
  maintainer's ProToPPTX for the Mac, ported. See
  [Export to PowerPoint](docs/features/export-powerpoint.md).
- Words ProPresenter fills with a blur, an invert or a cut-out of what is behind them,
  which PowerPoint has no fill for, go over filled with a picture of what is behind, the
  blur's size measured against ProPresenter's own pictures; a media fill with its picture.

## [0.117](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.117) · 2026-10-28

- Combined screens: an audience screen can be two outputs side by side drawn as one
  picture, as ProPresenter's Combined arrangement is (two 1920x1080 outputs as one
  3840x1080 screen). Under Settings → Screens its Arrangement is Single or Combined, and
  its second output goes wherever a screen can, with its own settings. Two outputs of a
  card are drawn in the same pass and handed to it together. The arrangement is written
  into ProPresenter's file as ProPresenter 7 writes it, and one ProPresenter made of more
  rows and columns is drawn as it is. See
  [Screens](docs/features/screens.md#combined-screens).
- The operator's preview and Screen Previews draw every layer inside the screen's own
  shape.

## [0.116](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.116) · 2026-10-28

- Fonts are drawn as their files name them: a family's Light, Thin, Medium, Black,
  Condensed or other face as itself, where it had been drawn as its Regular or Bold. See
  [Editing](docs/features/editing.md).
- A family installed from two makers, which Qt lists as `CMG Sans [ULA]` and `CMG Sans
  [PYRS]`, is called by its own name: the editor's font box no longer calls it missing,
  lists it once, and writes it into the file by its own name with its face's real
  PostScript name, where it had written Qt's form, which ProPresenter would not find.
- Where the face a file names is not installed, the family's face of the same style is
  used (a guess at ProPresenter's way).

## [0.115](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.115) · 2026-10-27

- ProPresenter's [Text](docs/features/text-format.md) button, after Search in the toolbar:
  a popover of the font, its style and colour, capitals, size, the alignment in the line
  and in the box, the outline and the shadow, for the words of every slide of the
  presentation at once, or of the slides selected. Each control shows what the words have
  in common, and is empty where they differ. The arrows beside its numbers step what each
  has: the size's make every size of the words a point larger or smaller, so that large
  words and small keep apart. Revert puts the file back as it was while it is open.
- [Slides selected](docs/features/text-format.md#selecting-slides) with Ctrl and a click,
  one at a time, and with Shift and a click, every slide from the last one selected,
  each with a blue ring. Only the Text button goes by them so far.
- [Apply This Text Style to All](docs/features/text-format.md#apply-this-text-style-to-all),
  this app's own, under Copy Text Style in a slide's menu: the slide taken as a theme for
  every other slide of its presentation, its text boxes alone.
- A theme or a text style put on the live slide while showing is drawn on the screens at
  once. Before, the operator's preview showed it and the screens did not until the next
  slide.

## [0.114](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.114) · 2026-10-26

- A [video delay](docs/features/screens.md#video-delay) for each audience screen, as
  ProPresenter's for an output: Settings → Screens, from none to fifteen of the screen's
  own frames, with what that comes to in milliseconds. In a window, on a display, over
  NDI or out of a card. The operator's preview and Screen Previews show the screen as it
  is now.
- A [Clock Source](docs/features/decklink.md): the screens going out through a DeckLink
  card can be drawn by one of its outputs' own frames, once in each, rather than by the
  app's clock, which drifts against the card's. Automatic is as before. Tried against the
  outputs the app pretends for its tests, not yet on a card.
- [Show Performance Statistics On Screen](docs/features/screens.md#performance-statistics),
  ProPresenter's setting: each audience screen shows how often it was drawn, what by,
  its video delay and how late its sound is heard, as far as the app can see.
- Sound is heard about 63 ms sooner: Qt read the app's sound 16 KB at a time and kept it
  in a buffer of its own, and the mixer kept more in hand than it needed. On the tests'
  sound device it is now heard at least 79 ms after it is played, of which the system's
  sound service and the card's share is not known to the app.

## [0.113](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.113) · 2026-10-26

- [The app's channels](docs/features/audio.md#the-apps-channels), as ProPresenter's: up to
  sixteen of the app's own between what plays and the sound device. Settings → Audio has
  Channels and Channel Routing, a matrix of the app's channels against the device's, with
  M, S and T beside each, M and S over them all, and Mapping's Auto and Clear. The
  channels, their names, Mute and Solo are kept in ProPresenter's file of the workspace;
  where they go on a device is this computer's, for each device.
- An Audio tab in the [Media Inspector](docs/features/media.md#the-media-inspector), for a
  video with sound and a piece of the audio bin: Ramp In and Ramp Out, the media's
  channels against the app's, and each channel's volume and mute, in ProPresenter's
  fields; a change taken back leaves the file as it was.
- The app mixes its own sound, all but a video input's, on a thread of its own: sound is
  heard a little after its picture (about a tenth of a second on the tests' sound
  device), and the meter shows what goes to the device. Building it needs FFmpeg's
  libswresample as well.

## [0.112](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.112) · 2026-10-24

- A video playing on the output stutters less as slides change over it, as measured: a
  slide shown by a cut is drawn once, where a second frame a moment later took the video's
  turn; transitions, scrolling text and builds move by each screen's own refreshes, and
  in the refreshes a playing video leaves free, where Qt's own animations ran on a timer
  of sixty-two and a half a second; and the stage, switched off, no longer lays out the
  slides' words at every click. Over a video of thirty frames a second, video frames late
  at a cut went from 7 to 11 in thirty to 1 to 4, and scrolling words now move on by the
  same in every frame. Words with a shadow still cost the video a frame at a cut. See
  [How it is made](docs/how-it-is-made.md#built-for-modest-hardware).
- The benchmark follows each frame of a video to the screen, and how evenly scrolling
  words move.

## [0.111](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.111) · 2026-10-24

- [Screen Previews that move](docs/features/stage-layouts.md#screen-preview): a Screen
  Preview on a stage layout changes as its screen does, by the screen's transition, with
  scrolling text and a video input in a shape going in it; before, a change in it was a
  cut and it stood still.
- In the operator window, the same for the Screen Previews in a stage layout the preview
  shows, and the preview itself changes by its screen's transitions, each with a switch in
  Settings → Windows, on to begin with. On the laptop this is made on, the preview's
  transitions bring the output's slides and transitions some four or five thousandths of a
  second later; off, the preview changes by a cut.
- [A guide](docs/guide.md) behind a ? at the right of the toolbar: the features that are
  this app's own, what it does not do, its keys and where it keeps things.
- The keys page gains Ctrl+Alt+C, the Calendar.

## [0.110](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.110) · 2026-10-23

- [A slide disabled at a date](docs/features/disabled-slides.md#at-a-date), this app's own:
  Disable On… in a slide's menu sets a day and a time for the slide to be disabled by
  itself; until then it is marked with the day. The dates are kept in the workspace, as
  ProPresenter keeps no field it does not know in a presentation.
- The editor's drags, a slider in its inspector or a handle on the slide, take half the work
  they did: the list of elements is no longer made again at every step, and the elements
  that a step has not changed are not worked out again.
- The README's screenshot taken again.
- 0.109's notes, which said a slider in the editor had become slower, were wrong, and are
  corrected: it did the same work as in 0.108.

## [0.109](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.109) · 2026-10-23

- [The Calendar](docs/features/calendar.md), as ProPresenter's: events at a date and time,
  once or every week on chosen days, that take a playlist's presentation live on its first
  slide or run a macro by themselves, in a window of their own opened from the toolbar (to
  the right of Bible; the toolbar's buttons are a little narrower to make room).
- [Scrolling text](docs/features/scrolling-text.md), as ProPresenter's: a text box's words
  going left, right, up or down across it, once or over and over, feathered at its ends,
  set in the editor's Text tab. How fast each Speed is, among other things, is a guess.
- [Disabled slides](docs/features/disabled-slides.md): Disable Slide and Enable Slide in a
  slide's menu, kept as ProPresenter keeps them. A disabled slide is faded, passed over by
  Next, Previous and everything that goes on by itself, and shown by a click. Slides
  ProPresenter had disabled are now listed, where before they were left out.
- A screen set to a display goes to that display the first time it is turned on, and not
  to the laptop's screen.

## [0.108](https://github.com/greyshirtguy/Simple-Presenter/releases/tag/0.108) · 2026-10-22

- [Playback markers](docs/features/playback-markers.md) on the transport: small flags over
  its bar, their points on the top of the slider's handle, so that a click on the slider is
  the slider's as ever; dragged to move a marker (the live media left where it is), showing
  its time and name under the pointer, and right-clicked for the Media Inspector's menu,
  with Rename typed over the media's name. The Inspector draws the same flags, larger.
- A marker moved, renamed or given actions while its media plays, on the transport or in
  the Inspector, is what that media goes by from then on.
- The README's screenshot taken again.

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
