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
is no password. It is off until switched on, and while it is off no port is open and it
costs nothing.

What is there, by ProPresenter's paths under `/v1/`:

| | |
| --- | --- |
| **Status** | the layers that are on, the slide live and the next with their words and notes, the screens, and the audience and stage screens switched on and off |
| **Presentations** | the one live and the one open in the window, by UUID, with their groups, slides and arrangements; where the show is in them; any slide of them, by its number or its group, next and previous; a presentation of a library or a playlist triggered or opened in the window |
| **Libraries and playlists** | the libraries, by name; the playlists, as a tree; a playlist's items; the playlist and item live and open; their items triggered, and next and previous |
| **Clearing** | each layer, and the workspace's clear groups |
| **Looks, props, messages, macros** | listed, and the live look; a look made live; a prop on and off; a message shown, with the words of its text tokens, and hidden; a macro run |
| **Timers** | listed, with their times now; started, stopped, reset, and given more time |
| **The stage** | its screens and layouts, which layout each screen has, and changing it; the stage message shown and hidden |
| **Media and audio** | the media playlists and the audio bin, their items played, next and previous; the video playing on the media layer played, paused, and moved through; the time left of it |
| **The rest** | the workspace's global groups and masks; and, for a client that asks for everything at once, capture (never capturing) and video inputs (none) |

**Streams.** As with ProPresenter, any of those that report something can be asked for
with `?chunked=true`, and the answer then stays open and sends the thing again each
time it changes; and `POST /v1/status/updates` with a list of paths streams them all
on one connection, the computer's clock every second among them. Each piece is the
JSON followed by an empty line (CR LF CR LF), so that a client can tell where one ends
when the network splits it.

**Not there yet**: whatever makes, changes or deletes things in the workspace's files
(looks, messages, timers, props, playlists, macros, clear groups); the pictures of
slides, media and props; the chord chart; and what waits on things the app does not do
yet (capture, video inputs, a timeline, pausing or moving through the audio bin's
music). Those answer 404, as ProPresenter answers what it does not know. Nor is
ProPresenter's other form of the API, over TCP/IP, nor its server-sent events.

**Where it is not quite ProPresenter's**:

- ProPresenter's UUIDs for its libraries are not in the workspace's folder, so each
  library is given one made from its name. ProPresenter's own documentation says a
  library is better asked for by name.
- A slide that is switched off in ProPresenter is left out here, so in a presentation
  that has one, a slide's number can differ from ProPresenter's.
- A presentation triggered from a library or a playlist goes live without the window
  opening it, as a GUESS at what ProPresenter does (its "focus" paths are for that);
  stepping through the playlist open in the window opens each item there.
- Next and previous do what the arrow keys do here, which stay in the presentation.
- The JSON's keys come in alphabetical order, and a whole number has no ".0".

It has been tried with requests made by hand and by the app's own tests; not yet with
Companion itself. The log says when it listened, who streamed what, and each request
that operated the show, with who sent it.
