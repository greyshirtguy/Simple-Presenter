[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# The extended API, and MCP

[ProPresenter's API](api.md) operates a show, and can make and change some of what goes
with one, but it has nothing for a presentation itself: its slides, their words and what
is on them. This is the app's own addition to it, on the same port, for that and what
goes with it: **a program can make and change the presentations, playlists and themes
of the open workspace**, so that a whole service can be set up by a script: the songs
made, in a theme or not, put in a playlist under headings, and any of it changed later.

And the same is offered to an AI model, through **MCP** (the Model Context Protocol):
each thing the API can do is a tool a model can call, so that a person can ask for the
same in their own words ("make a presentation of these lyrics in our Lyrics theme and
put it after the welcome in Sunday's playlist").

It is the app's own and not ProPresenter's: nothing of it is under ProPresenter's
`/v1/`, and no program made for ProPresenter knows of it. Like the rest of the app it
is one person's hobby, shared as an example and not as a product: what is here may
change from one version to the next, and nobody is promised that it stays.

**Switching it on.** In **Settings → Network**, with **Enable Network** on, **Enable
Extended API and MCP** switches both on. They are off until then, and answer nothing.

**Who can use it.** There is no password, as with ProPresenter's API; but this part can
change and delete what is in the workspace, so it is held closer:

- Switched on, it answers only **programs on this computer**. **From Other Computers**
  lets the other computers of the network in: then anyone on the network can change the
  workspace through it, as anyone can operate the show through ProPresenter's API.
- A request from **a web page** is answered only if the page is one of this computer's
  own (or, with other computers let in, one served by a computer of the local network).
  A page from the internet, open in a browser here, is turned away; so is a page that
  does not say where it is from (one in a sandbox, or one opened from a file on the
  disk: a page that is to use the API has to be served by something); and so is a
  request that asks for this computer by a name of the internet's, and not by its
  number, by `localhost`, or by a name that only a local network gives (the computer's
  own, one word, or one ending `.local`, `.lan`, `.home.arpa` and the like).

**What it does to files.** Every change is made as the app's own windows make it: the
file is read whole, what the change is about is altered, and the whole is written back,
so that everything of ProPresenter's the app does not understand stays as it was. A
request about a presentation or a theme either does all it was asked or changes nothing;
one about playlists is looked at whole before any of it is done. (A presentation's new
name is the one thing done after the rest: it is looked at first, and can then only
fail as a file can.) Beyond that:

- a presentation or a theme that is deleted goes to the computer's **Trash**;
- before a file is changed, **a copy of it as it was** is kept with the editor's own
  copies (`~/.local/share/SimplePresenter/SimplePresenter/Edit Backups/`, "… before the
  API"), the newest twenty of each file: always where the file is not as the API itself
  last left it (somebody has changed it at the window, or in ProPresenter, since), and
  otherwise once in ten minutes, so that a run of changes keeps what was there before
  the run;
- a presentation that is **open in the editor** at the computer cannot be changed until
  the editor is closed: the request is refused, and says so;
- a presentation that is **on the output** when it is changed stays as it is there until
  a slide of it is next shown, as when one is edited at the window; and one that is
  deleted or renamed while it is on the output goes on being shown.

A change is made on the app's own thread, as one made at the window is: a video that is
playing may miss a frame as a file is written. It is for setting a service up, not for
the middle of one.

## MCP

MCP is how a program that lets an AI model use tools (Claude's apps and Claude Code, an
editor's assistant, a command line agent) is told what tools there are. The app offers
its tools two ways, which do the same:

- **By an address**, for a program that takes one: `http://127.0.0.1:50001/mcp` (the
  port is the API's). For Claude Code: `claude mcp add --transport http simplepresenter http://127.0.0.1:50001/mcp`.
- **By a command**, for a program that starts its MCP servers itself:
  `SimplePresenter --mcp`. Started so, the app opens no window: it only passes each
  tool's call to the app that is running the show, through the API, and is ended by the
  program that started it. In such a program's list of servers:

  ```json
  { "mcpServers": { "simplepresenter": { "command": "SimplePresenter", "args": ["--mcp"] } } }
  ```

  (`SimplePresenter --mcp --mcp-api http://192.168.1.20:50001` asks an app on another
  computer, which must have let other computers in.)

Either way the app has to be running, with both switches on. A model is told to begin
with `get_workspace`, and can ask for this page with `get_reference`.

It speaks MCP's revisions 2026-07-28 (in which every request stands by itself) and
2025-11-25, 2025-06-18, 2025-03-26 and 2024-11-05 (which begin with a handshake), and
offers tools and nothing else of MCP: no resources, prompts or notifications. A tool's
answer is the API's answer, as JSON text; a tool that could not do what it was asked
answers with the reason, marked as an error, for the model to put right.

**A model changes real files.** It is as able to delete a playlist as to make one, and
does what it understood, which is not always what was meant. Most programs ask before a
tool that changes something is called: leave that on. What is deleted is in the Trash,
and the copies kept are whole.

It has been tried with requests made by hand and by the app's own tests, which speak
both ways of MCP to it. It has not been tried with every program that speaks MCP.

## Overview: every operation

Paths are under `/sp/v1/`; a request's body and every answer are JSON. Names are in
lower case with underscores, colours are `"#rrggbb"` (or `"#rrggbbaa"` for one that is
partly see-through), and lengths are in a slide's own units (a slide is usually 1920 by
1080). `GET /sp/v1/openapi.json` (`get_openapi`) describes all of this in OpenAPI's form,
for tools that try an API or generate a client for one. Each operation is an MCP tool of
the name given, whose arguments are the parts of the path and what the body takes.

A connection is kept open for the next request, as HTTP does, and the app keeps sixty-four
at once for all of its API: a program that asks often should ask again on the connection
it has, or close it (`Connection: close`), and not leave one open for each request.

What a path names:

- **`{presentation}`**: its uuid, or its name. A name two libraries have is told apart
  with `?library=`.
- **`{slide}`**: its id, or its number from 0 among the presentation's slides in the
  presentation's own order (its groups as it keeps them, whatever arrangement is
  selected).
- **`{element}`**: its id, its name, or its number from 0, the furthest back.
- **`{group}`**, **`{arrangement}`**: its id or its name (a group also by its number).
- **`{theme}`**: its id, its place under the Themes folder (`Samples/Black Box`, the
  slash written `%2F` in a path), or its name where one theme has it.
- **`{playlist}`**: its id, or its name; **`{item}`**: a row's id, or its number from 0.
- **`{library}`**: its name.

| Tool | Request | What it does |
| --- | --- | --- |
| `get_workspace` | `GET /sp/v1/workspace` | The workspace in outline: libraries, playlists, themes, groups. The place to begin |
| `get_reference` | `GET /sp/v1/reference` | This page, or with `?topic=` one part of it |
| `get_openapi` | `GET /sp/v1/openapi.json` | The API as an OpenAPI document (not an MCP tool) |
| `list_libraries` | `GET /sp/v1/libraries` | The libraries (not an MCP tool: `get_workspace` has them) |
| `create_library` | `POST /sp/v1/libraries` | Makes an empty library |
| `list_presentations` | `GET /sp/v1/library/{library}` | A library's presentations: uuid and name |
| `search_presentations` | `GET /sp/v1/search` | Finds presentations by name or by words, `?q=` |
| `create_presentation` | `POST /sp/v1/library/{library}/presentations` | Makes a presentation, with its slides |
| `get_presentation` | `GET /sp/v1/presentation/{presentation}` | A presentation; `?detail=full` for every element |
| `update_presentation` | `PATCH /sp/v1/presentation/{presentation}` | Its name, destination, Slide Show, arrangement, key, copyright details |
| `delete_presentation` | `DELETE /sp/v1/presentation/{presentation}` | Moves it to the Trash |
| `duplicate_presentation` | `POST /sp/v1/presentation/{presentation}/duplicate` | Makes a copy of it |
| `apply_theme` | `POST /sp/v1/presentation/{presentation}/theme` | Dresses its slides in a theme |
| `format_text` | `POST /sp/v1/presentation/{presentation}/format` | Formats the text of many slides |
| `update_group` | `PATCH /sp/v1/presentation/{presentation}/group/{group}` | Renames, recolours or moves a group |
| `delete_group` | `DELETE /sp/v1/presentation/{presentation}/group/{group}` | Deletes a group with its slides |
| `set_arrangement` | `PUT /sp/v1/presentation/{presentation}/arrangement/{arrangement}` | Makes or changes an arrangement |
| `delete_arrangement` | `DELETE /sp/v1/presentation/{presentation}/arrangement/{arrangement}` | Deletes an arrangement |
| `update_timeline` | `PATCH /sp/v1/presentation/{presentation}/timeline` | Its timeline's length, loop and cues |
| `add_slides` | `POST /sp/v1/presentation/{presentation}/slides` | Adds slides |
| `get_slide` | `GET /sp/v1/presentation/{presentation}/slide/{slide}` | A slide with all its elements (a theme's: `GET /sp/v1/theme/{theme}/slide/{slide}`) |
| `update_slide` | `PATCH /sp/v1/presentation/{presentation}/slide/{slide}` | Its words, label, notes, media, group, place |
| `delete_slide` | `DELETE /sp/v1/presentation/{presentation}/slide/{slide}` | Deletes a slide |
| `duplicate_slide` | `POST /sp/v1/presentation/{presentation}/slide/{slide}/duplicate` | Copies a slide, here or into another presentation |
| `add_element` | `POST /sp/v1/presentation/{presentation}/slide/{slide}/elements` | Puts a text box, a shape or a picture on a slide (a theme's: `POST /sp/v1/theme/{theme}/slide/{slide}/elements`) |
| `update_element` | `PATCH /sp/v1/presentation/{presentation}/slide/{slide}/element/{element}` | Changes an element (a theme's: `PATCH /sp/v1/theme/{theme}/slide/{slide}/element/{element}`) |
| `delete_element` | `DELETE /sp/v1/presentation/{presentation}/slide/{slide}/element/{element}` | Takes an element off (a theme's: `DELETE /sp/v1/theme/{theme}/slide/{slide}/element/{element}`) |
| `add_action` | `POST /sp/v1/presentation/{presentation}/slide/{slide}/actions` | Gives a slide an action |
| `update_action` | `PATCH /sp/v1/presentation/{presentation}/slide/{slide}/action/{action}` | Changes one of a slide's actions |
| `delete_action` | `DELETE /sp/v1/presentation/{presentation}/slide/{slide}/action/{action}` | Takes an action off a slide |
| `list_themes` | `GET /sp/v1/themes` | The themes (not an MCP tool: `get_workspace` has them) |
| `get_theme` | `GET /sp/v1/theme/{theme}` | A theme, its slides and their elements |
| `create_theme` | `POST /sp/v1/themes` | Makes a theme |
| `delete_theme` | `DELETE /sp/v1/theme/{theme}` | Moves a theme to the Trash |
| `add_theme_slide` | `POST /sp/v1/theme/{theme}/slides` | Adds a slide to a theme |
| `update_theme_slide` | `PATCH /sp/v1/theme/{theme}/slide/{slide}` | A theme slide's name and background |
| `delete_theme_slide` | `DELETE /sp/v1/theme/{theme}/slide/{slide}` | Deletes a theme's slide |
| `list_playlists` | `GET /sp/v1/playlists` | The playlists and folders (not an MCP tool: `get_workspace` has them) |
| `get_playlist` | `GET /sp/v1/playlist/{playlist}` | A playlist's rows |
| `create_playlist` | `POST /sp/v1/playlists` | Makes a playlist, with its rows, or a folder |
| `update_playlist` | `PATCH /sp/v1/playlist/{playlist}` | Renames or moves a playlist |
| `delete_playlist` | `DELETE /sp/v1/playlist/{playlist}` | Removes a playlist (its presentations stay) |
| `set_playlist_items` | `PUT /sp/v1/playlist/{playlist}/items` | Gives a playlist these rows and no others |
| `add_playlist_items` | `POST /sp/v1/playlist/{playlist}/items` | Adds rows to a playlist |
| `update_playlist_item` | `PATCH /sp/v1/playlist/{playlist}/item/{item}` | A row's arrangement, a heading's words and colour, a row's place |
| `delete_playlist_item` | `DELETE /sp/v1/playlist/{playlist}/item/{item}` | Removes a row |
| `list_media` | `GET /sp/v1/media` | The media files under the workspace's Media folder |
| `list_action_targets` | `GET /sp/v1/action_targets` | Timers, macros, looks, props and the rest that a slide's action can name |

For example, a song made in a theme and put in a playlist, with `curl`:

```
curl -X POST http://127.0.0.1:50001/sp/v1/library/Songs/presentations -d '{
  "name": "Example Song", "theme": "Lyrics",
  "groups": [ { "name": "Verse 1", "slides": ["First line\nSecond line", "Third line\nFourth line"] },
              { "name": "Chorus",  "slides": ["The chorus\nof the song"] } ],
  "arrangements": [ { "name": "Sunday", "groups": ["Verse 1", "Chorus", "Verse 1", "Chorus"] } ],
  "arrangement": "Sunday" }'
curl -X PUT http://127.0.0.1:50001/sp/v1/playlist/Sunday/items -d '{
  "items": [ { "header": "Worship", "color": "#ff9300" }, { "presentation": "Example Song" } ] }'
```

## Presentations

A presentation is kept as ProPresenter keeps one: **groups** (Verse 1, Chorus), each a
name, a colour and its slides, in the order the slides run in; and **arrangements**,
each a name and the groups to go through, in which a group may come up several times.
With no arrangement selected, the slides run group by group.

`get_presentation` answers:

| | |
| --- | --- |
| `uuid`, `name`, `library`, `file` | what it is and where |
| `destination` | `"presentation"`, or `"announcements"` for one sent to the announcements layer |
| `slide_show` | ProPresenter's Slide Show: seconds each slide stays, 0 for none |
| `arrangement` | the name of the arrangement selected, `""` for none |
| `key`, `shown_key` | the key its chords are written in, and the one they are shown in |
| `copyright` | `{title, author, artist, publisher, year, song_number, album, display}` |
| `groups` | `[{id, name, color, slides: [ids]}]`, in order |
| `arrangements` | `[{id, name, groups: [group ids], group_names}]` |
| `slide_count`, `slides` | its slides in its own order: see Slides |
| `timeline` | `{duration, loop, cues: [{place, time, name, track, slide or action}]}` |

A change to a presentation as a whole is answered with the same without `slides`.

**Making one** (`create_presentation`) takes `name`, and its slides one of three ways:

- `groups`: `[{name, color, slides: [...]}]`. A group named as the workspace's own list
  of groups names one ("Verse 1", "Chorus") is that group, in its colour; `color` is
  for another.
- `slides`: `[...]`, for slides in no group.
- `lyrics`: a song's words as text, each part under a line that names it, parts parted
  by an empty line, the title first if there is one (as SongSelect's lyrics files are);
  cut into slides of `lines_per_slide` lines, 2 unless said.

and `theme` (with `theme_slide`) to dress every slide as it is made; `arrangements`
(`[{name, groups: [names]}]`) and `arrangement`; `destination`; `copyright`; `key`;
`width` and `height`. A name the library has already is given a number after it.

`update_presentation` changes `name` (the file is renamed, and the rows of playlists
that are it are brought along), `destination`, `slide_show`, `arrangement`, `key`,
`shown_key` and `copyright`. A new `key` (the key its chords are written in) is the key
it is shown in as well, as when one is chosen in the editor, unless `shown_key` says
another; the key it has already, sent again, changes neither. `apply_theme` and
`format_text` take `slides` (ids or numbers), or do every slide where `slides` is left
out; a list with nothing in it is refused. `format_text`'s `format` is what ProPresenter's Text button
sets: `family`, `style`, `size`, `size_by`, `color`, `capitalization`, `alignment`,
`vertical_alignment`, `stroke_on`, `stroke_color`, `stroke_width`, `stroke_width_by`,
`text_shadow_enabled`, `text_shadow_solid_color`, `text_shadow_opacity`,
`text_shadow_radius`, `text_shadow_angle`, `text_shadow_offset` (and the last four with
`_by` after them, to add to what each has).

`update_group` takes `name`, `color`, and `before` or `after` another group.
`set_arrangement` makes the arrangement named in the path if there is none, and takes
`groups` (the whole list, by id or name) and `name`. `update_timeline` takes `duration`,
`loop`, `remove` (places), `move` (`[{place, time}]`) and `add` (`[{time, slide}]` or
`[{time, action, name}]`).

## Slides

A slide is described the same way wherever one is made (`create_presentation`,
`add_slides`), all of it optional; a string alone is a slide of those words:

| | |
| --- | --- |
| `text` | its words, lines parted by `\n`, in one text box |
| `elements` | in place of `text`, for more than one box of words: a list as `add_element` takes |
| `label`, `label_color` | its label |
| `notes` | its notes |
| `enabled` | `false` for a slide Next and Previous pass over |
| `background` | a colour |
| `media` | the picture or video the slide brings: a file's path, or `{file, foreground, playback, loop_count, loop_seconds, settings}` |
| `go_to_next` | its Go To Next Timer: `{seconds, loop}` |
| `actions` | a list as `add_action` takes |

A slide made of `text` alone looks as its neighbours do: its text box is a copy of the
nearest slide's box of words. In a presentation with none yet it is a box over the whole
slide but for a margin, named "Lyrics", its words centred in white. With `theme`, the
theme decides.

A slide is answered as `{id, index, group, label, label_color, enabled, text, notes,
width, height, background, media, go_to_next, actions}`, and in full with `elements`.

`add_slides` puts them `after` or `before` a slide, at the end of a `group` (a name the
presentation has not makes a new group, last), or at the end. `update_slide` changes
`text` (the words of its text box, keeping how they are set and the chords over words
that stay), `label`, `label_color`, `notes`, `enabled`, `background`, `media` (`null`
takes it away), `go_to_next` (`null` for none); `group` begins a group at the slide, as
choosing a group in a slide's menu does in ProPresenter (it and the slides after it in
its group become that group); and `before`, `after` or `to_group` move it.

`media`'s `playback` is `"stop"`, `"loop"`, `"loop_count"` (with `loop_count`) or
`"loop_seconds"` (with `loop_seconds`). A file is named by its whole path on the
computer the app runs on, or by its path under the workspace's Media folder
(`list_media`): files are referred to where they are, and not copied.

## Elements

What is on a slide: text boxes, shapes, pictures and videos, which in ProPresenter's
files are one kind of thing. `get_slide` lists a slide's from the back to the front.
`add_element` takes `kind` (`"text"`, `"rectangle"`, `"rounded_rectangle"`, `"ellipse"`,
`"arrow"`, or `"media"` with `file`) and any of the properties below; a text box with
no `text` given says "Text". `update_element` takes any of them, and `order`. They may
be sent beside the rest or in a `properties` object. An element sent back as it was
shown changes nothing: only what differs is written. What can only be read (`id`,
`visible`, `shape` and the like) is passed over where it is sent back as it was, and
refused where it is sent as something else.

What says nothing while its switch is off is left out of an answer (a stroke's colour
and width, of an element whose stroke is off).

| Property | |
| --- | --- |
| `id`, `visible`, `shape` | (read only) its id; whether it shows; `rectangle`, `rounded_rectangle`, `ellipse`, `arrow`, `other` |
| `name` | what it is called. A theme matches text boxes by name, and a stage layout can ask for the text of boxes of one name ("Lyrics") |
| `x`, `y`, `width`, `height` | where it is and how large |
| `rotation` | degrees, clockwise |
| `opacity` | 0 to 1 |
| `locked`, `hidden` | |
| `order` | its place among the slide's elements, 0 the furthest back |
| `fill_on` | whether it is filled |
| `fill_kind` | `color`, `gradient`, `media`, `screen` (a picture of what a screen shows), `slide_image` (of the live slide), `blur` or `invert` (of what is behind it) |
| `fill_color` | a plain fill's colour; giving it makes the fill a plain colour |
| `fill_gradient_from`, `fill_gradient_to`, `fill_gradient_angle` | a gradient's first and last colours, and its angle |
| `fill_media_path`, `fill_media_scale`, `fill_media_settings` | a picture or video to fill it with; 0 fitted, 1 filling it, 2 stretched |
| `fill_blur_amount`, `fill_blur_saturation` | a blur's |
| `fill_screen_id`, `fill_screen_name`, `fill_slide_next`, `fill_input_index` | which screen; the next slide and not the live one; which video input |
| `fill_lines_only` | the fill only behind the lines of the text |
| `roundness` | a rounded rectangle's corners, as a part of its shorter side |
| `feather_on`, `feather_radius` | its edges fading out |
| `stroke_on`, `stroke_color`, `stroke_width` | its outline |
| `shadow_enabled`, `shadow_color`, `shadow_angle`, `shadow_offset`, `shadow_radius` | its shadow (the colour with how see-through it is); also `shadow_solid_color` and `shadow_opacity` to set those apart |
| `text_shadow_…` | the same, for its text's shadow |
| `vertical_alignment` | `top`, `middle`, `bottom` |
| `margin_left`, `margin_top`, `margin_right`, `margin_bottom` | the text's margins in the box |
| `text_scale` | 0 as it is; 2 made smaller if it does not fit; 3 made larger if there is room; 4 either |
| `chords_on`, `chord_notation`, `chord_color` | whether chords are drawn, how (0 letters, 1 numbers, 2 numerals, 3 Do Re Mi), and their colour |
| `link_kind` | where its text comes from, if not its own: `none`, `element`, `timer`, `slide_text`, `clock`, `video_countdown`, `stage_message` |
| `link_element_id`, `link_transform` | another element of the slide; 0 as it is, 1 on one line, 2 a word a line, 3 a letter a line |
| `link_timer_id`, `link_timer_name`, `link_timer_hours`, `link_timer_minutes`, `link_timer_seconds`, `link_timer_hundredths`, `link_color_triggers` | a timer (`list_action_targets`); each part 0 hidden, 1 one digit, 2 two, 3 and 4 the same but hidden while nothing; `[{time, color}]` |
| `link_slide_next`, `link_slide_source`, `link_slide_name`, `link_slide_preserve` | the live slide's text (or the next's): 0 its words, 1 its notes, 2 its elements of a name |
| `link_clock_date`, `link_clock_time`, `link_clock24` | the clock: 0 none, 1 short, 2 medium, 3 long, 4 full |
| `link_video_ignore_looping`, `link_flash`, `link_flash_color` | a video's time left; the stage message |
| `link_marker_destination`, `link_marker_which`, `link_marker_name`, `link_marker_time`, `link_marker_color` | a playback marker |
| `visibility_rules`, `visibility_criterion`, `visibility_conditions` | shown only when: 0 all, 1 any, 2 none of `[{kind: "element", element_id, has_text}]` |

A slide's **builds** and **transitions**, a text box's scrolling and the fills of its
letters are read and not yet changed here.

## Text

An element's text is answered three ways at once:

- `text`: its words, lines parted by `\n`;
- `format`: the format they begin in: `{family, font_name, size, bold, italic, color,
  alignment}`, and `underline`, `strikethrough`, `kerning`, `capitalization`,
  `stroke_color` and `stroke_width` where they are anything;
- `paragraphs`: all of it, `[{alignment, runs: [{text, …format}]}]`, where the text has
  more than one format in it.

and changed three ways:

- `text`: the words typed over. What the old and the new have in common at their start
  and their end keeps its look, and so do the chords over words that stay.
- `format`: a format for all of it; only what is named is changed, and it is put on
  every word: `{"color": "#ffffff"}` makes words that begin white and go on red all
  white. (A `format` with every property the element was read with is taken for that one
  sent back, and only what differs from it is put on the words: so an element of several
  looks that is read, changed elsewhere and sent back whole keeps them.)
- `paragraphs`: the text anew. A run takes what it does not say from the look the box
  had, so `[{"runs": [{"text": "Plain "}, {"text": "bold", "bold": true}]}]` is enough.

`alignment` is `left`, `center`, `right` or `justify`; `capitalization` is `none`,
`all_caps`, `small_caps`, `title_case` or `start_case`. A font is named by its family
(`"Helvetica Neue"`), with `bold` and `italic`, or by one face's own name (`font_name`,
`"HelveticaNeue-Bold"`); a font that is not installed on the app's computer is kept in
the file by name and drawn in another.

`chords` are `[{at, name}]`: the chord's name in the song's key, and the place in the
words it stands over, counted in characters from 0 (a line break is one).

## Actions

What else a slide's cue does when it is triggered. `get_presentation` lists a slide's,
each with its `id`, `kind` and a `title` in words; `add_action` takes `kind` and what
that kind takes, beside it or in `properties`; `update_action` takes all of it again.
`list_action_targets` has the ids and names of what they work.

| `kind` | Takes |
| --- | --- |
| `timer` | `action` (`start`, `stop`, `reset`, `reset_and_start`, `stop_and_reset`, `increment`), `timer_id`, `timer_name`, `amount` (seconds an increment adds); and to set the timer up too, `set: true` with `set_kind` (`countdown`, `countdownTo`, `elapsed`), `set_duration`, `set_time_of_day`, `set_start_time`, `set_end_time`, `set_has_end_time`, `set_overrun` |
| `clear` | `layer`: `all`, `audio`, `media`, `props`, `slide`, `audio_effects` or `announcements` |
| `clear_group` | `group_id`, `group_name` |
| `stage` | `assignments`: `[{screen_id, screen_name, layout_id, layout_name}]`, the layout's two empty to leave a screen as it is |
| `prop` | `prop_id`, `prop_name`, `collection_id`, `collection_name`, and `clear: true` to take it off |
| `look` | `look_id`, `look_name` |
| `macro` | `macro_id`, `macro_name`, `collection_id`, `collection_name` |
| `message` | `message_id`, `message_name`, `values`: `[{token_id, name, kind, value}]` |
| `audio` | `item` (the id of a row of the audio bin), `playback` (`stop` or `loop`), `effect` |
| `media` | `item` (the id of a row of the media bin) |
| `media_playlist`, `audio_playlist` | `playlist_id`, `playlist_name`, `retrigger` |
| `video_input` | `input_index` |
| `communication` | `device_id`, `device_name`, `command` and what it sends (see [devices](devices.md)) |

Actions of kinds the app does not run are listed as `other`, and are left as they are.

## Themes

A theme is a set of slides that say how slides should look ([Themes](themes.md)).
Dressing a slide in a theme's slide puts the slide's words into the theme's text boxes,
matched by name, then by size, then in order; so a theme's text box needs **words in
it** to stand for the words to come ("Lyrics"), and a name that the slides' boxes have
is the surest match.

`get_theme` answers `{id, name, place, slides: [{id, name, index, width, height,
background, elements}]}`. `create_theme` takes `name` and `slides` (`[{name,
background, width, height, elements}]`); with none it has one slide with a text box
over most of it. `add_theme_slide` takes the same for one slide, or `copy_of` another.
A theme's elements are changed as a presentation's are, by the same three operations at
the theme's paths (for an MCP tool, `theme` in place of `presentation`).

## Playlists

`get_playlist` answers `{id, name, items}`, each row `{id, index, kind, name}` and for

- a heading (`kind: "header"`): `color`;
- a presentation: `presentation` (its uuid), `library`, `arrangement` (the row's own
  choice, `""` for every group in order), `missing` (its file is not in the workspace);
- anything else ProPresenter can put in a playlist: `kind: "other"`, kept and not opened.

A row to be made is `{presentation, library, arrangement}` (the presentation by uuid or
name; without `arrangement`, the presentation's own choice) or `{header, color}`.
`set_playlist_items` lays out the whole playlist: the rows given, in that order, and no
others. A row that is there already keeps its id (the same heading, the same
presentation, or `{id}` for a row by its id, which is also how a row of kind `other` is
kept); what is not given is removed. `add_playlist_items` adds rows `after` a row or at
the end. A presentation removed from a playlist stays in its library.

## Media

`list_media` answers `{folder, folders, files: [{name, file, kind}]}` for the
workspace's Media folder or, with `?folder=`, a folder in it; `kind` is `image`, `video`
or `audio`. Nothing is uploaded or copied through the API: a slide or an element is
given a file that is already on the app's computer.

## Errors

A request that did not go through is answered with a status and `{"error": why}`:

| | |
| --- | --- |
| 400 | what was sent cannot be understood, or cannot be: a property that is not known (with those that are), a colour that is none, a media file that is not on the computer |
| 403 | who asks may not: another computer while only this one is let in, or a web page from elsewhere |
| 404 | something that was named is not there, in the path or in what was sent: a presentation, a slide, an element, an action, a group, a theme, a playlist, a row; or the extended API is switched off |
| 405 | the path is there, but not with that method (`allow` names those it has) |
| 409 | something is in the way: the presentation is open in the editor, or another presentation or library has the name |
| 500 | a file could not be read or written |

An MCP tool answers the same reason as text, marked as an error.

## What is a guess

ProPresenter has no such API to go by, so the whole of it is this app's own. Within it,
where a change is one ProPresenter makes too, it is made as the app makes it elsewhere,
which is as ProPresenter does where that is known; these were not looked at there:

- a heading with no colour is written with none (every heading seen in ProPresenter's
  files had been given one);
- a slide moved goes into the group of the slide it is put beside;
- a group removed is taken out of every arrangement;
- a group renamed to a name the workspace's list of groups has takes that group's
  colour and its id in the list, as one chosen from a slide's menu does;
- that ProPresenter opens a presentation or a theme made here: none has been tried
  there yet (as for every presentation the app makes: see [the README](../../README.md)).

## Not here yet

Uploading a media file; a library renamed or deleted; a presentation moved to another
library; a slide's builds and transitions, scrolling text and the fills of letters; the
props, stage layouts, masks, looks, macros, timers and messages themselves (ProPresenter's
API has requests for some of those, which this app [does not answer yet](api.md)); the
media bin and the audio bin; and operating the show, which is ProPresenter's API's.
