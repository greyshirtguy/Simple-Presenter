[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# The API

ProPresenter offers its own API over the network, which Bitfocus Companion's ProPresenter
module, the remotes made for it and anyone's own scripts follow and operate the show
through. Switched on in **Settings → Network** (**Enable Network**), this app answers
the same API, by the same paths, in the same JSON, at this computer's address and the
**Port** set there (50001 to begin with); the settings show the addresses. A client
already set up for a ProPresenter is pointed at this computer instead. The **Network
Name** is what `/version` says the computer is called (its own name until another is
set), and `/version` says this is SimplePresenter, on Linux: it does not pretend to be
ProPresenter.

**Anyone on the network can operate the show through it**, as with ProPresenter: there
is no password. It listens on every network the computer is on, not only the one the
show's devices are on, so a laptop on a hotel's Wi-Fi offers it to the hotel; and, as
ProPresenter does, it answers any web page open in a browser on the network, not only
programs. It is off until switched on, and while it is off no port is open and it costs
nothing.

What is there, by ProPresenter's paths under `/v1/`:

| | |
| --- | --- |
| **Status** | the layers that are on, the slide live and the next with their words and notes, the screens, and the audience and stage screens switched on and off |
| **Presentations** | the one live and the one open in the window, by UUID, with their groups, slides and arrangements; where the show is in them; any slide of them, by its number or its group, next and previous; a presentation of a library or a playlist triggered or opened in the window |
| **Libraries and playlists** | the libraries, by name; the playlists, as a tree; a playlist's items; the playlist and item live and open; their items triggered, and next and previous |
| **Clearing** | each layer, and the workspace's clear groups |
| **Looks, props, messages, macros** | listed, and the live look; a look made live; a prop on and off; a message shown, with the words of its text tokens, and hidden; a macro run |
| **Timers** | listed, with their times now; started, stopped, reset, and given more time; and set up by PUT (the kind, its times, and whether it may overrun), by itself or along with starting, stopping or resetting it, which is what Companion's Timer Set sends |
| **The stage** | its screens and layouts, which layout each screen has, and changing it; the stage message shown and hidden |
| **Media and audio** | the media playlists and the audio bin, their items played, next and previous; the video playing on the media layer played, paused, and moved through; the time left of it |
| **Video inputs** | listed, and one put on the [video input](video-inputs.md) layer; whether one is on it, and clearing it |
| **Timelines** | a presentation's [timeline](timeline.md) played, paused and rewound, and whether it is running and where its playhead is: the live presentation's, the one open, one by its UUID, one by its place in a playlist, and the announcements' |
| **Pictures of slides** | any slide of a presentation, by its UUID, or of an item of a playlist, as a JPEG or a PNG of the size asked for (see below) |
| **The rest** | the workspace's global groups and masks; and, for a client that asks for everything at once, capture (never capturing) |

**Streams.** As with ProPresenter, any of those that report something can be asked for
with `?chunked=true`, and the answer then stays open and sends the thing again each
time it changes; and `POST /v1/status/updates` with a list of paths streams them all
on one connection, the computer's clock every second among them. Each piece is the
JSON followed by an empty line (CR LF CR LF), so that a client can tell where one ends
when the network splits it.

**Not there yet**: whatever makes, changes or deletes things in the workspace's files
(looks, messages, props, playlists, macros, clear groups; of the timers, only setting
one up is here); the pictures of media, props, masks, stage layouts and themes; the
chord chart; and what waits on things the app does not do
yet (capture, pausing or moving through the audio bin's music). Those answer 404, as ProPresenter answers what it does not know. Nor is
ProPresenter's other form of the API, over TCP/IP, nor its server-sent events.

**Where it is not quite ProPresenter's**:

- ProPresenter's UUIDs for its libraries are not in the workspace's folder, so each
  library is given one made from its name. ProPresenter's own documentation says a
  library is better asked for by name.
- A slide that is switched off in ProPresenter is left out here, so in a presentation
  that has one, a slide's number can differ from ProPresenter's.
- A presentation triggered from a library or a playlist goes live without the window
  opening it, as a GUESS at what ProPresenter does (its "focus" paths are for that);
  stepping through the playlist open in the window opens each item there. A slide
  triggered in the presentation open in the window has the window's slides follow it,
  as they follow one taken live there.
- Next and previous do what the arrow keys do here, which stay in the presentation.
- The JSON's keys come in alphabetical order, and a whole number has no ".0".

## ProPresenter Remote

**ProPresenter Remote**, Renewed Vision's app for a phone or a tablet, works with this
app as it does with ProPresenter, through the API. Under the API's settings in
**Settings → Network**, **Enable ProPresenter Remote** switches it on, with a
**Controller** password, which lets a Remote operate the show, and an **Observer**
password, which lets one only follow it; either can be switched off. The API must be on
as well (**Enable Network**).

While it is on, the app is advertised on the network as ProPresenter is, under the
Network Name, so that the Remote lists it by itself; the settings say whether it is.
That is done by Avahi, which answers for the whole computer on the network and is on
every Ubuntu desktop: without it, the Remote has to be given the computer's address.
The Remote then talks to the app over a WebSocket at `/v1/ws` on the API's port, each
message encrypted with its password (AES-256, as ProPresenter does it), and asks for
what it shows and does by the API's own paths: so what is there for the Remote is what
is there above. A Remote whose password is wrong is answered nothing, as ProPresenter
answers it.

The Remote shows each slide by a picture of it, which it fetches from the API by HTTP:
see the next part.

What is a guess, ProPresenter's not having been seen doing it: the names and order of
the settings, and the controller on and the observer off to begin with; with both
passwords on, the first message (ProPresenter's `/version`) sent once under each, the
controller's first; an observer's request for what operates the show answered "403
Forbidden"; and a Remote whose password is changed, or switched off, let go of. How the
Remote's messages are encrypted and what it sends were seen of ProPresenter 22 and its
Remote on 2026-11-13, and the encryption is as Renewed Vision describes it.

The passwords are kept in this computer's settings for the app, as ProPresenter keeps
its own in its preferences, and are not in the workspace.

## Pictures of slides

`GET /v1/presentation/{uuid}/thumbnail/{index}`, and the same of an item of a playlist
(`/v1/playlist/{id}/{index}/thumbnail/{cue}`, and of the active playlist's), give a
picture of a slide, as ProPresenter's do: the slide as the screens show it, with its
media behind it, and a dark checkerboard where it is transparent, as ProPresenter draws
one. The cue is counted in the arrangement the presentation is shown with, a playlist's
item bringing its own. `quality` is the length of the picture's longer side, 400 unless
asked (here between 16 and 1920); `thumbnail_type=png` asks for a PNG rather than a
JPEG. A cue that is not there is 404.

A picture is drawn by the same drawing that puts the slide on the screens, so that it
cannot differ from them; but **not by the app itself**. Drawing a slide takes the app's
own thread, which a video under the slides needs at every frame: a Remote leafing
through a presentation asks for a picture of every slide in it at once, which would make
a video stutter. So the pictures are drawn by a second process of the app's, started the
first time one is asked for and closed when none has been for five minutes. It asks
Linux to give it the processor and the disk only when nothing else wants them; and as
that still leaves it sharing what Linux does not share out (the processor's cache, each
core's second thread, the graphics chip), the app stops it outright whenever something
is taken live, until the change and its transition are over, and lets it go on after.
The pictures wait for the show; the app only passes the requests to it and the pictures
back. Each picture is kept, in the app's cache folder
(`slide-pictures`, under 200 MB), until its presentation or its media changes, and
asked for again is sent from there.

What is a guess: ProPresenter's checkerboard is 8-pixel squares at 400 wide, which here
grow with the picture; ProPresenter's bounds to `quality` are not known.

It has been tried with requests made by hand and by the app's own tests, and with
Companion, whose Timer Set (a PUT with the timer's new length) is what brought the
timers' PUT here. The log says when it listened, who streamed what, and each request
that operated the show, with who sent it.
