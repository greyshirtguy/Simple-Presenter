[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Devices

A **device** is something outside the app that a slide or a macro sends to, as in
ProPresenter's Devices: notes to a **MIDI** port, a Custom Control to a Ross switcher
over **RossTalk**, or a request to a web address (**HTTP Requests**, which is this
app's own and not ProPresenter's). A MIDI device's sources also work the show, by the
**MIDI Map**.

They are set up in **Settings → Devices**, as ProPresenter's page has them: a row for
each, with a green edge when it is connected, what it is, and **Connect**; a click on
the row opens what it is set to, and **+ Add a Device** adds one of the three kinds.

- **MIDI**: its **Sources**, the ports whose notes work the show, and its
  **Destinations**, the ports the notes its actions send go to, ticked from the MIDI
  ports there are (through ALSA, so a USB MIDI device, a program's port and network
  MIDI's are all there). A port it names that is not here is shown, and said, and the
  device goes on with the rest.
- **RossTalk**: the switcher's **Address** and **Port** (7788 is RossTalk's). It keeps
  a connection to it open.
- **HTTP Requests**: a **Base address** (`http://192.168.1.20:8080/api`), and
  **Headers** that every request has (an `Authorization`, say). There is nothing to
  connect.
- **Auto Reconnect**, for MIDI and RossTalk: a device that drops, or whose port goes,
  is connected again when it can be (RossTalk tries every few seconds); and one that was
  connected when the workspace was last left is connected again when it is next opened.

The devices are this computer's, kept for each workspace in this app's own settings
(`~/.config/SimplePresenter/Devices.json`). ProPresenter keeps its own list in the
workspace (its `Configuration/CommunicationDevices`), which is read and never written:
its MIDI and RossTalk devices that are not here yet are offered under the list, to be
added with ProPresenter's names and ids, so that the actions ProPresenter made for them
send through them. Their ports and addresses are this computer's to set.

**The actions.** Once there is a device, **Add Action → Communication** has it, by kind
and then by device where there are several of a kind, as ProPresenter's menu has them:

- **MIDI Note On…** and **MIDI Note Off…**: the note (0 to 127, with its name: 60 is
  C3), the velocity and the channel.
- **Custom Control…**: which, as the switcher numbers it. It is sent as `CC` and that.
- **HTTP Request…**: the method (GET, POST, PUT or DELETE), the path under the base
  address (or a whole address of its own), a body, and headers of its own, which win
  over the device's. A body with no Content-Type of its own is said to be JSON if it
  looks like it, and plain words otherwise.

A MIDI note and a Custom Control are written into the presentation or the macro exactly
as ProPresenter writes them, so that ProPresenter's own work here and this app's there.
An HTTP request, which ProPresenter does not have, is written in the general form
ProPresenter's communication actions have (a format and named values) and nothing else:
to ProPresenter it is an action for a device it does not have. What each sent, and how
it went (an HTTP request's answer and how long it took), is in the log.

**The MIDI Map**, from the button beside + on the Devices page: ProPresenter's map of
the notes that work the show, with its 33 commands and its notes to begin with (Clear
All is 0, Next Slide 12, and so on), under its headings. A note heard from a connected
device's sources, on any channel, does what its note is set to; for those under
**Select by Index** the note's velocity says which one, from 1: Select Playlist with a
velocity of 4 opens the 4th playlist, Toggle Prop On/Off with 2 the 2nd prop of the
collection selected. **Auto Fill** gives the notes one after another from the note
typed. Bail to Logo and the timelines are greyed: this app has neither. The last note
heard is shown at the top, with what it did. The map is this computer's, for every
workspace.

What is a GUESS, or not done:

- That a RossTalk command ends with a carriage return and a line feed, as RossTalk's
  published form has it (ProPresenter's help does not say); what ProPresenter's Auto
  Reconnect covers; what each of the MIDI Map's commands does in ProPresenter beyond
  its name (a playlist "selected" is opened here, a playlist item opened, Next Playlist
  Item takes the next presentation live at its first slide); and the order Auto Fill
  goes in.
- Not done: ProPresenter's other kinds of device (DMX, Global Cache, the Sony and GVG
  switchers, the scoreboards), RossTalk's other commands (Fade To Black, Key Cut, Key
  Trans, Trigger GPI: one ProPresenter made is sent as it is, but none can be made
  here), MIDI that ProPresenter's key mappings name, and a picture for a communication
  action (ProPresenter has one; the pack of its pictures here does not).
