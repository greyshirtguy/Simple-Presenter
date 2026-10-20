[Simple Presenter](../README.md)

# TODO

What is not there yet. ProPresenter does a great deal that this app does not, and not
all of it is even on this list. It is a list of ideas, not of promises: see the warning at
the top of the [README](../README.md).

## Partly there

- [ ] **File compatibility**: draw more of what a `.pro` file can hold, such as a video
      as an element's fill, gradients of more than two colours or that run in a
      circle, and feathered edges on shapes other than the three plain ones. Drawn so
      far: text with its fonts, colours, outline, shadow, capitals, underline and
      spacing, made smaller or larger to suit its box where it is set to be; shapes,
      from their outlines, whatever they are; fills that are a colour (including one
      that is only behind the lines of the text), a gradient from one colour to
      another, a picture, or a Screen Preview of a screen; strokes, shadows and
      feathered edges; elements that show
      only when another has text, or while a timer runs; and text linked from another
      element, from a timer, or from the slide that is live.
- [ ] **Editor**: text boxes, shapes and pictures can be added, moved, resized, turned
      and removed, and their text and looks changed; slides can be added, copied,
      pasted and deleted; a song's chords can be put on; builds (see
      [Builds](features/builds.md)). Still to do: reordering
      slides, the rest of ProPresenter's shapes, picking several elements at once,
      lists and scrolling text, builds of a text a part at a time (ProPresenter's
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
      presets, the two effects drawn from what is behind, the audio tab and playback
      markers; for the audio bin's music as well as for media; and the inspector for
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
- [ ] **Clear groups**: they are there, for the layers this app has (the slide, the
      media, the props, the messages, the announcements, the audio and the audio
      effects). Still to come: the other layer a group can name (video inputs), which
      also wants a clear button of its own when the layer comes; giving a group a
      picture of one's own; and putting the groups in another order by dragging.
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
      stage, props, macros, looks and audio, and those are run; so is one that shows a
      message, which cannot be made here yet, and communication with a MIDI, RossTalk
      or HTTP device (see [Devices](features/devices.md)). Still to come: the rest
      of ProPresenter's kinds (capture and more, which
      are kept in the files and shown, and not done), an action's delay, putting a
      slide's or a macro's actions in another order, and the clear layer the app
      does not have yet (video inputs).
- [ ] **Bibles**: they are there (see [Bibles](features/bibles.md)), from XML files and from
      api.bible. Still to come: searching a Bible by a word; a second translation
      beside the first; other sources online (the ESV's own, among them); and the
      flow's weights tried on real passages.
- [ ] **Blackmagic DeckLink**: a screen can go out through a card's output, keyed or
      not (see [Blackmagic DeckLink](features/decklink.md)), and has been tried on one
      card; keying not yet. Still to come: the video coming in (see Video input,
      below); sound, in and out, the first two channels to begin with; and more
      formats.
- [ ] **Devices**: MIDI, RossTalk's Custom Control and HTTP requests are there (see
      [Devices](features/devices.md)). Still to come: ProPresenter's other kinds of device
      (DMX, Global Cache, the Sony and GVG switchers, the scoreboards), RossTalk's other
      commands made here, and MIDI notes in the key mappings.
- [ ] **The API**: ProPresenter's HTTP API answers what the app has (see
      [The API](features/api.md)). Still to come: what makes, changes or deletes things in the
      workspace's files through it (looks, messages, timers, props, playlists, macros,
      clear groups), the pictures of slides and of media, the chord chart, and what
      waits on features the app does not have yet.
- [ ] **The audio bin**: it is there (see [Audio](features/audio.md)). Still to come: shuffling a
      playlist; a hot key for a piece; where a piece starts and stops, how loud it
      is, and its own fades, which ProPresenter's inspector sets and which are kept
      in the file and, but for how loud, not gone by; going round a number of times
      or for a length of time, which is played and cannot be set; and a transport for
      the music (the one under the previews is the video's).

## To try, and see what happens

Bigger things, nearly all of which ProPresenter does and this does not. No order, and
no promise that any of them will turn out to be possible this way: that is the
experiment.

- [ ] **A simple help file**: the app's own features explained in one place, in the app
- [ ] **Video input**: from a Blackmagic card's input first, on a video input layer
      under the media, with its clear button and its place in the clear groups
- [ ] **Clear to logo**
- [ ] **Playback markers**
- [ ] **Timer markers** (an idea of my own)
- [ ] **Text formatter**
- [ ] **CCLI**: a song's details edited, and the ways of showing them
- [ ] **More to import**: PowerPoint, to begin with
- [ ] **Easy View**
- [ ] **Table View**
- [ ] **Timecode**
- [ ] **Streaming and recording** (RTP), perhaps
