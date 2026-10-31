[Simple Presenter](../README.md)

# TODO

What is not there yet. ProPresenter does a great deal that this app does not, and not
all of it is even on this list. It is a list of ideas, not of promises: see the warning at
the top of the [README](../README.md).

## Partly there

- [ ] **Text fills**: letters filled with a gradient, a picture, a blur or an invert of
      what is behind them, or cut out of their box, and boxes filled with a blur or an
      invert, are drawn and kept, and set in the Text and Shape tabs as ProPresenter's are
      (see [Text fills](features/text-fills.md)). Still to do: a cut-out of a box filled
      with a gradient or a picture; such letters with chords over them, or scrolling; and
      the shadow under them.
- [ ] **Export to PowerPoint**: slide transitions and builds as PowerPoint's, sound, and
      a media cue's Media Inspector settings, are not carried over yet (see [Export to
      PowerPoint](features/export-powerpoint.md#what-does-not-go-over)).
- [ ] **Screens of several outputs**: an audience screen of outputs combined into one
      picture, as ProPresenter's Combined arrangement is, is drawn: two side by side are
      offered (see [Screens](features/screens.md#combined-screens)), and more, from a file
      ProPresenter wrote, are drawn as they are. Still to do: edge blending, and a stage
      screen of several outputs.
- [ ] **File compatibility**: draw more of what a `.pro` file can hold, such as a video
      as an element's fill, gradients of more than two colours or that run in a
      circle, and feathered edges on shapes other than the three plain ones. Drawn so
      far: text with its fonts, colours, outline, shadow, capitals, underline and
      spacing, made smaller or larger to suit its box where it is set to be; shapes,
      from their outlines, whatever they are; fills that are a colour (including one
      that is only behind the lines of the text), a gradient from one colour to
      another, a picture, a Screen Preview of a screen, or a blur or an invert of what is
      behind; letters filled with more than a colour ([Text fills](features/text-fills.md));
      strokes, shadows and
      feathered edges; elements that show
      only when another has text, or while a timer runs; and text linked from another
      element, from a timer, or from the slide that is live.
- [ ] **Editor**: text boxes, shapes and pictures can be added, moved, resized, turned
      and removed, and their text and looks changed; slides can be added, copied,
      pasted and deleted; a song's chords can be put on; builds (see
      [Builds](features/builds.md)); a text box's scrolling (see
      [Scrolling text](features/scrolling-text.md)). Still to do: reordering
      slides, the rest of ProPresenter's shapes, picking several elements at once,
      lists, builds of a text a part at a time (ProPresenter's
      Delivery), and a
      theme for a new slide (everything that makes a new slide, which is the `+` over
      the editor's slides, New Slide in a slide's menu, media dropped between slides
      and the first slide of a new presentation, is to offer a theme's slide as well
      as a blank one).
- [ ] **Key mappings**: the hotkeys of groups are there, read from and written to the
      workspace's list of groups as ProPresenter has them, and Ctrl+S and Ctrl+E for
      show mode and edit mode. Still to do: a page of the settings for the app's other
      keys, and the rest of what ProPresenter's key mappings can go to (cues, macros,
      props, timers, clear groups, MIDI notes).
- [ ] **Playlists**: playlists and folders can be created, renamed, rearranged, removed
      and run, their rows added, reordered and removed, and playlists exported from
      ProPresenter brought in. Still to do: headers, media rows, and a continuous view
      of a whole playlist, one presentation under another.
- [ ] **Media inspector**: it is there (see
      [The Media Inspector](features/media.md#the-media-inspector)), with the file, the thumbnail, the
      fitting, turning, flipping and cropping, the way a video plays on, its trim and
      ProPresenter's effects. Still to come: how
      loud (for now a rule: a background video is silent, a foreground one has its
      sound), its alignment, delay and play rate, a transition of its own, effect
      presets, the two effects drawn from what is behind; and the inspector for
      several pieces of media at once.
- [ ] **Show controls**: the timers, the props, the stage layouts and the macros are
      there, for as many stage screens as are wanted. Still to come: more of what a
      stage layout can show (pictures of the slides and of the output); a prop's own
      transition and clearing itself after a time.
- [ ] **Messages**: they are there (see [Messages](features/messages.md)). Still to come: making
      an action that shows one, and what such an action gives the message's tokens; a
      message that goes when its timers have run out; setting how a timer in a message
      is written; and messages seen on the network.
- [ ] **Helping yourself with an AI**: the advice, and what an agent is told, are there
      ([Helping yourself, with an AI](ai-help.md)). Still to do, so
      that an agent or anyone else can check its own work: a workspace that can be
      published, made of nothing anyone owns, for the scripted tests to run on; a test
      that a whole workspace survives being read and written back unchanged; one
      command that runs every check there is; and the checks run by themselves on every
      pull request.
- [ ] **Clear groups**: they are there, for every layer a group can name. Still to
      come: giving a group a picture of one's own; and putting the groups in another
      order by dragging.
- [ ] **Announcements**: they are there (see [Announcements](features/announcements.md)). Still to
      come: a key for their clear button; and a transition of their own.
- [ ] **Go To Next timers and the Slide Show**: they are there (see
      [Go To Next timers and the Slide Show](features/go-to-next-and-slide-show.md)).
      Still to come: a slide that goes on when its video ends; the other things
      ProPresenter lets a slide lead to (a random slide, a named one); and something
      on the slide that shows the time running down.
- [ ] **Masks**: they are there (see [Masks](features/masks.md)). Still to come: a mask of
      another size than 1920 by 1080, to match the screen it is for; copying one; and
      a look's fade for a mask that comes or goes.
- [ ] **Actions**: a slide and a macro can be given actions for timers, clearing, the
      stage, props, macros, looks, audio, playlists, messages and video inputs, and
      those are run; so is communication with a MIDI, RossTalk or HTTP device (see
      [Devices](features/devices.md)). Still to come: the rest of ProPresenter's kinds
      (capture and more, which are kept in the files and shown, and not done), an
      action's delay, and putting a slide's or a macro's actions in another order.
- [ ] **Bibles**: they are there (see [Bibles](features/bibles.md)), from XML files and from
      api.bible. Still to come: searching a Bible by a word; a second translation
      beside the first; other sources online (the ESV's own, among them); and the
      flow's weights tried on real passages.
- [ ] **Blackmagic DeckLink**: a screen can go out through a card's output, keyed or
      not (see [Blackmagic DeckLink](features/decklink.md)), and has been tried on one
      card; keying not yet. Still to come: the video coming in, as a video input (see
      Video inputs, above); sound, in and out, the first two channels to begin with;
      and more formats.
- [ ] **Devices**: MIDI, RossTalk's Custom Control and HTTP requests are there (see
      [Devices](features/devices.md)). Still to come: ProPresenter's other kinds of device
      (DMX, Global Cache, the Sony and GVG switchers, the scoreboards), RossTalk's other
      commands made here, and MIDI notes in the key mappings.
- [ ] **The API**: ProPresenter's HTTP API answers what the app has (see
      [The API](features/api.md)). Still to come: what makes, changes or deletes things in the
      workspace's files through it (looks, messages, timers, props, playlists, macros,
      clear groups), the pictures of slides and of media, the chord chart, and what
      waits on features the app does not have yet.
- [ ] **Video inputs**: they are there, from the devices Linux has for video (see
      [Video inputs](features/video-inputs.md)). Still to come: a DeckLink card's
      inputs, and NDI sources, as inputs; ProPresenter's audio Delay, channel levels
      and routing for an input's sound; and the settings ProPresenter has for a
      card's inputs.
- [ ] **Playback markers**: they are there (see [Playback markers](features/playback-markers.md)).
      Still to come: Cut, Copy and Paste of markers; dragging an action, a show control
      or a file onto a marker, or under the list for a marker of its own; and the link
      that colours a shape by a marker.
- [ ] **Copyright display**: it is there (see [Copyright display](features/copyright.md)).
      Still to come: ProPresenter's CCLI reporting, and its "Show House of Worship
      Integrations" switch.
- [ ] **Calendar**: it is there (see [Calendar](features/calendar.md)). Still to come:
      ProPresenter's Capture, and leaving out one time of a repeating event.
- [ ] **The audio bin**: it is there (see [Audio](features/audio.md)). Still to come: shuffling a
      playlist; a hot key for a piece; where a piece starts and stops, and how loud it
      is, which ProPresenter's inspector sets and which are kept in the file and, but
      for how loud, not gone by; and going round a number of times or for a length of
      time, which is played and cannot be set.
- [ ] **The app's channels**: they are there, and where media's channels and theirs go
      (see [The app's channels](features/audio.md#the-apps-channels)). Still to come: a video
      input's sound through them, with ProPresenter's routing for an input; sound out
      through a DeckLink card and over NDI, which ProPresenter routes separately; the
      level, delay and tone of each of the device's channels, which ProPresenter's
      matrix has; and making up for the delay the mixing adds by itself (a screen's
      [video delay](features/screens.md#video-delay) holds the picture back, set by hand).

## To try, and see what happens

Bigger things, nearly all of which ProPresenter does and this does not. No order, and
no promise that any of them will turn out to be possible this way: that is the
experiment.

- [ ] **Clear to logo**
- [ ] **Timer markers** (an idea of my own)
- [ ] **More to import**: PowerPoint, to begin with
- [ ] **Easy View**
- [ ] **Table View**
- [ ] **Timecode**
- [ ] **Streaming and recording** (RTP), perhaps
