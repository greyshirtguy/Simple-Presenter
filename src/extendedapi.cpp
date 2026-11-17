#include "extendedapi.h"

#include <QHostAddress>
#include <QUrl>

namespace extendedapi {

QString prefix()
{
    return QStringLiteral("/sp/v1/");
}

QString mcpPath()
{
    return QStringLiteral("/mcp");
}

namespace {

using F = Field;

Field in(const QByteArray &name, const QByteArray &description)
{
    return {name, Field::Path, "string", true, description, {}, {}};
}

Field asked(const QByteArray &name, const QByteArray &type, const QByteArray &description, const QList<QByteArray> &choices = {})
{
    return {name, Field::Query, type, false, description, {}, choices};
}

Field given(const QByteArray &name, const QByteArray &type, const QByteArray &description, bool required = false,
            const QByteArray &items = {}, const QList<QByteArray> &choices = {})
{
    return {name, Field::Body, type, required, description, items, choices};
}

// What is said of the things most paths name, once
const QByteArray kPresentation = "The presentation: its uuid, or its name. A name that two libraries have is told apart by "
                                 "`library`.";
const QByteArray kLibraryOf = "The library the presentation is in, where its name alone would not say which is meant.";
const QByteArray kSlide = "The slide: its id, or its number among the presentation's slides in the order the presentation keeps "
                          "them (the order get_presentation lists them in), from 0.";
const QByteArray kThemeSlide = "The theme's slide: its id, its name, or its number from 0.";
const QByteArray kElement = "The element: its id, its name, or its number among the slide's elements from 0, which is the one "
                            "furthest back.";
const QByteArray kTheme = "The theme: its id, its place (the path under the workspace's Themes folder, as get_workspace gives "
                          "it), or its name where only one theme has it.";
const QByteArray kPlaylist = "The playlist or folder: its id, or its name (the first of that name, as get_workspace lists them).";
const QByteArray kSlideThings =
    "A slide is given as an object, all of it optional: `text` (its words, lines parted by \\n, which go in one text box), "
    "`label` and `label_color`, `notes`, `enabled`, `background` (a colour), `media` (a picture or video the slide brings: a "
    "file's path, or {file, foreground, playback, loop_count, loop_seconds}), `go_to_next` ({seconds, loop}), `elements` (a "
    "list of elements as add_element takes them, in place of `text`, for a slide with more on it than one box of words) and "
    "`actions` (a list of actions as add_action takes them). A string alone is a slide of those words.";

QList<Operation> table()
{
    QList<Operation> all;

    // ---- finding one's way

    all.append({"get_workspace", "GET", "workspace", {}, Operation::Read, "The workspace in outline",
                "What the open workspace holds, to find one's way: its name and folder, its libraries with how many "
                "presentations each has, its playlists and their folders as a tree (ids, names, and how many items), its "
                "themes with their slides' names, and the workspace's own groups (the names and colours ProPresenter offers "
                "for a song's parts). Ask this first.",
                {}});
    all.append({"get_reference", "GET", "reference", {}, Operation::Read, "The reference: every property by name",
                "The extended API's reference as text (Markdown): what a presentation, a slide and an element are made of, "
                "every property of an element that can be read and changed with what its values mean, how text is "
                "formatted, the kinds of action a slide can have, a playlist's items, and what is a guess. Ask for one "
                "`topic` to have only that part.",
                {asked("topic", "string", "One part of it; all of it without.",
                       {"overview", "presentations", "slides", "elements", "text", "actions", "themes", "playlists", "media", "errors"})}});
    all.append({"get_openapi", "GET", "openapi.json", {}, Operation::Read, "This API as an OpenAPI document",
                "Every operation here, with its path, what it takes and what it is for, in OpenAPI 3.1's form.", {}, false});

    // ---- libraries

    all.append({"list_libraries", "GET", "libraries", {}, Operation::Read, "The libraries",
                "The workspace's libraries, by name, with how many presentations each has.", {}, false});
    all.append({"create_library", "POST", "libraries", {}, Operation::Change, "Make a library",
                "Makes a new, empty library: a folder of that name under the workspace's Libraries.",
                {given("name", "string", "What it is called. No slash in it.", true)}});
    all.append({"list_presentations", "GET", "library/{library}", {}, Operation::Read, "A library's presentations",
                "The presentations of one library, in the order of the alphabet: each one's uuid and name.",
                {in("library", "The library, by its name.")}});
    all.append({"search_presentations", "GET", "search", {}, Operation::Read, "Find presentations by name or words",
                "Finds presentations in every library by their names and by the words on their slides, as the app's own "
                "search does: capitals, punctuation and accents make no difference. Those found by name come first; one "
                "found by its words comes with the first line that has them.",
                {{"q", Field::Query, "string", true, "What to look for: words of a name, or of a song.", {}, {}},
                 asked("limit", "integer", "The most to give back; 20 without.")}});

    // ---- presentations

    all.append({"create_presentation", "POST", "library/{library}/presentations", {}, Operation::Change, "Make a presentation",
                "Makes a new presentation in a library, with its slides. Give its slides in one of three ways: `groups`, a "
                "song's parts each with its slides; `slides`, slides in no group; or `lyrics`, a song's words as text, each "
                "part under a line that names it (Verse 1, Chorus, …), which is cut into slides of `lines_per_slide` lines. "
                "With none of them it has one slide with nothing on it. With `theme`, every slide is dressed in that theme "
                "as it is made. If the library has a presentation of that name already, a number is put after the name. "
                "Answers with the presentation as get_presentation gives it.",
                {in("library", "The library to make it in, by its name."),
                 given("name", "string", "What it is called.", true),
                 given("groups", "array",
                       "Its groups in order, each {name, color, slides}: `name` as ProPresenter names a song's parts (\"Verse "
                       "1\", \"Chorus\"), `color` only to give it another than the workspace has for that name, and `slides` "
                       "its slides. " + kSlideThings,
                       false, "object"),
                 given("slides", "array", "Its slides, in no group, where `groups` is not given: each as a slide of a group is.", false, "any"),
                 given("lyrics", "string",
                       "A song's words as plain text, where neither is given: each part begins with a line that names it "
                       "(\"Verse 1\", \"Chorus\", \"Bridge\"), parts are parted by an empty line, and the first line may be the "
                       "title. As SongSelect's lyrics files are."),
                 given("lines_per_slide", "integer", "How many lines of `lyrics` go on a slide; 2 without."),
                 given("theme", "string", "A theme to dress every slide in: its id, its place or its name."),
                 given("theme_slide", "string", "Which of the theme's slides; its first without."),
                 given("arrangements", "array",
                       "Arrangements to make, each {name, groups}: `groups` the names of the groups to go through, in order, "
                       "a group as often as it comes up.",
                       false, "object"),
                 given("arrangement", "string", "The name of the one of those arrangements the presentation is to be shown in."),
                 given("destination", "string", "Where its slides are sent: the presentation layer, or the announcements.", false,
                       {}, {"presentation", "announcements"}),
                 given("copyright", "object",
                       "A song's details for the copyright display, any of {title, author, artist, publisher, year, "
                       "song_number, display}."),
                 given("key", "string", "The key its chords are written in (\"G\", \"F#m\")."),
                 given("width", "number", "The width of its slides; 1920 without."),
                 given("height", "number", "Their height; 1080 without.")}});
    all.append({"get_presentation", "GET", "presentation/{presentation}", {}, Operation::Read, "A presentation",
                "A presentation: its uuid, name and library, its groups in order with their slides' ids, its arrangements, "
                "and each slide in the presentation's own order with its words, label, notes, the media it brings and its "
                "actions. With `detail` \"full\" each slide also has its elements, with every property; without, only what "
                "is needed to know what the slides say.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 asked("detail", "string", "How much of each slide; \"outline\" without.", {"outline", "full"})}});
    all.append({"update_presentation", "PATCH", "presentation/{presentation}", {}, Operation::Change, "Change a presentation's own settings",
                "Changes what is said of the presentation as a whole; only what is given is changed. Renaming it renames its "
                "file and brings the rows of playlists that are it along.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("name", "string", "Another name."),
                 given("destination", "string", "Where its slides are sent.", false, {}, {"presentation", "announcements"}),
                 given("slide_show", "number", "ProPresenter's Slide Show: the seconds each slide stays before the next comes by itself; 0 for none."),
                 given("arrangement", "string", "The arrangement it is shown in, by name or id; \"\" for none, which is every group in order."),
                 given("key", "string", "The key its chords are written in."),
                 given("shown_key", "string", "The key they are shown in."),
                 given("copyright", "object", "Changes to the song's details: any of {title, author, artist, publisher, year, song_number, display}.")}});
    all.append({"delete_presentation", "DELETE", "presentation/{presentation}", {}, Operation::Remove, "Take a presentation out of its library",
                "Moves the presentation's file to the computer's Trash, where it can be put back from. Rows of playlists "
                "that were it are left, and say it is missing.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf)}});
    all.append({"duplicate_presentation", "POST", "presentation/{presentation}/duplicate", {}, Operation::Change, "Make a copy of a presentation",
                "Makes a copy of the presentation, with everything in it, as a presentation of its own: for one that is to "
                "be like another to begin with. Answers with the copy.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("name", "string", "What the copy is called; the same name with a number after it without."),
                 given("to_library", "string", "The library the copy goes in, by name; the same one without.")}});
    all.append({"apply_theme", "POST", "presentation/{presentation}/theme", {}, Operation::Change, "Dress slides in a theme",
                "Dresses slides of a presentation in a theme's slide, for good: each slide's text boxes take the place, the "
                "size, the font and the colours of the theme's, keeping their words, and what else the theme's slide has "
                "comes with it. All its slides, or those named.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("theme", "string", kTheme, true),
                 given("theme_slide", "string", "Which of the theme's slides: its id, name or number; its first without."),
                 given("slides", "array", "The slides to dress, by id or number; all of them without.", false, "string")}});
    all.append({"format_text", "POST", "presentation/{presentation}/format", {}, Operation::Change, "Format the text of many slides",
                "Puts a format on every text box that has words, of all the presentation's slides or of those named, as "
                "ProPresenter's Text button does: only what `format` names is changed.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("format", "object",
                       "Any of: family, style (\"Regular\", \"Bold\", \"Italic\", \"Bold Italic\"), size, size_by (points added to "
                       "each size), color, capitalization (\"none\", \"all_caps\", \"small_caps\", \"title_case\", \"start_case\"), "
                       "alignment (\"left\", \"center\", \"right\", \"justify\"), vertical_alignment (\"top\", \"middle\", "
                       "\"bottom\"), stroke_on, stroke_color, stroke_width, text_shadow_enabled, text_shadow_solid_color, "
                       "text_shadow_opacity, text_shadow_radius, text_shadow_angle, text_shadow_offset.",
                       true),
                 given("slides", "array", "The slides, by id or number; all of them without.", false, "string")}});

    // ---- groups and arrangements

    all.append({"update_group", "PATCH", "presentation/{presentation}/group/{group}", {}, Operation::Change, "Rename, recolour or move a group",
                "Changes a group of a presentation: what it is called, its colour, or where it comes in the presentation's "
                "own order of groups. Its slides and the arrangements that have it are left as they are.",
                {in("presentation", kPresentation), in("group", "The group: its id, its name, or its number from 0."),
                 asked("library", "string", kLibraryOf),
                 given("name", "string", "Another name."),
                 given("color", "string", "Another colour, \"#rrggbb\"; \"\" for none of its own."),
                 given("before", "string", "The group it is to come just before."),
                 given("after", "string", "Or the group it is to come just after.")}});
    all.append({"delete_group", "DELETE", "presentation/{presentation}/group/{group}", {}, Operation::Remove, "Delete a group and its slides",
                "Takes a group out of a presentation with every slide in it, and out of every arrangement that had it.",
                {in("presentation", kPresentation), in("group", "The group: its id, its name, or its number from 0."),
                 asked("library", "string", kLibraryOf)}});
    all.append({"set_arrangement", "PUT", "presentation/{presentation}/arrangement/{arrangement}", {}, Operation::Change, "Make or change an arrangement",
                "Makes an arrangement of that name if the presentation has none, or changes the one it has: the groups it "
                "goes through, in order, a group as often as it is to come up; and its name.",
                {in("presentation", kPresentation), in("arrangement", "The arrangement: its id or its name. A name the presentation has not is a new arrangement's."),
                 asked("library", "string", kLibraryOf),
                 given("groups", "array", "The groups to go through, in order, each by its id or name. A new one without this has every group once.", false, "string"),
                 given("name", "string", "Another name for it.")}});
    all.append({"delete_arrangement", "DELETE", "presentation/{presentation}/arrangement/{arrangement}", {}, Operation::Remove, "Delete an arrangement",
                "Takes an arrangement out of a presentation. If it was the one the presentation is shown in, none is.",
                {in("presentation", kPresentation), in("arrangement", "The arrangement: its id or its name."),
                 asked("library", "string", kLibraryOf)}});
    all.append({"update_timeline", "PATCH", "presentation/{presentation}/timeline", {}, Operation::Change, "Change a presentation's timeline",
                "Changes ProPresenter's timeline of a presentation: its length, whether it goes round, and the cues on it. "
                "`remove` and `move` name cues by the place get_presentation gives each; they are done before `add`.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("duration", "number", "Its length in seconds."),
                 given("loop", "boolean", "Whether it starts again at its end."),
                 given("remove", "array", "Cues to take off, by their places.", false, "integer"),
                 given("move", "array", "Cues to move, each {place, time}.", false, "object"),
                 given("add", "array", "Cues to put on, each {time, slide} for a slide (its id or number) or {time, action, name} for an action as add_action takes one.", false, "object")}});

    // ---- slides

    all.append({"add_slides", "POST", "presentation/{presentation}/slides", {}, Operation::Change, "Add slides to a presentation",
                "Adds slides to a presentation: after a slide, before one, at the end of a group, or at the end of the "
                "presentation. With `theme` the new slides are dressed in it; without, a slide given only `text` has a text "
                "box like that of the nearest slide with words, so that it looks as the others do. Answers with the new slides.",
                {in("presentation", kPresentation), asked("library", "string", kLibraryOf),
                 given("slides", "array", "The slides to add, in order. " + kSlideThings, true, "any"),
                 given("after", "string", "The slide they go after, by id or number."),
                 given("before", "string", "Or the slide they go before."),
                 given("group", "string", "Or the group they go at the end of, by id or name. A name the presentation has not makes a new group of that name, last."),
                 given("theme", "string", "A theme to dress them in."),
                 given("theme_slide", "string", "Which of its slides; its first without.")}});
    all.append({"get_slide", "GET", "presentation/{presentation}/slide/{slide}", "theme/{theme}/slide/{slide}", Operation::Read, "A slide, with everything on it",
                "One slide in full: what get_presentation says of it, and its elements from the back to the front, each "
                "with every property (see get_reference, topic \"elements\"). A presentation's slide, or with `theme` a theme's.",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf)}});
    all.append({"update_slide", "PATCH", "presentation/{presentation}/slide/{slide}", {}, Operation::Change, "Change a slide",
                "Changes a slide of a presentation; only what is given is changed. `text` changes the words of its text box "
                "and keeps how they are set (their font, size and colour, and the chords over words that stay).",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf),
                 given("text", "string", "The words of its text box (the first that has words, or failing that the first empty one; a slide with no text box is given one), lines parted by \\n."),
                 given("label", "string", "Its label; \"\" takes the words off."),
                 given("label_color", "string", "The label's colour, \"#rrggbb\"; \"\" for none."),
                 given("notes", "string", "Its notes; \"\" for none."),
                 given("enabled", "boolean", "Whether Next and Previous stop at it (ProPresenter's Enable/Disable Slide)."),
                 given("background", "string", "Its background colour; \"\" for none."),
                 given("group", "any", "Begins a group at this slide, as choosing a group in a slide's menu does in ProPresenter: it and the slides after it in its group become the group named ({name, color}, or a name alone, whose colour is the workspace's for it)."),
                 given("media", "any", "The picture or video it brings: a file's path, or {file, foreground, playback (\"stop\", \"loop\", \"loop_count\", \"loop_seconds\"), loop_count, loop_seconds, settings}; null takes it away."),
                 given("go_to_next", "any", "Its Go To Next Timer, {seconds, loop}; null for none."),
                 given("before", "string", "Moves it to just before this slide (id or number)."),
                 given("after", "string", "Or to just after this one."),
                 given("to_group", "string", "Or to the end of this group (id or name).")}});
    all.append({"delete_slide", "DELETE", "presentation/{presentation}/slide/{slide}", {}, Operation::Remove, "Delete a slide",
                "Takes a slide out of a presentation, for good, and out of its group.",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf)}});
    all.append({"duplicate_slide", "POST", "presentation/{presentation}/slide/{slide}/duplicate", {}, Operation::Change, "Copy a slide",
                "Makes a copy of a slide, with everything on it and all its cue does, straight after it; or into another "
                "presentation, after a slide of that one or at its end. Answers with the copy.",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf),
                 given("to", "string", "The presentation the copy goes into, by uuid or name; the same one without."),
                 given("after", "string", "The slide of that presentation it goes after; without, after the slide copied, or at the end of another presentation.")}});

    // ---- elements (of a presentation's slide, or a theme's)

    all.append({"add_element", "POST", "presentation/{presentation}/slide/{slide}/elements", "theme/{theme}/slide/{slide}/elements",
                Operation::Change, "Put an element on a slide",
                "Adds an element to a slide, in front of what is there: a text box, a shape, or a picture or video. Any "
                "property of an element can be given with it (get_reference, topic \"elements\"), and `text` and `format` "
                "for its words. On a presentation's slide, or with `theme` in place of `presentation` on a theme's. Answers "
                "with the element.",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf),
                 given("kind", "string", "What it is; a text box without.", false, {},
                       {"text", "rectangle", "rounded_rectangle", "ellipse", "arrow", "media"}),
                 given("file", "string", "For kind \"media\": the picture or video file's path."),
                 given("text", "string", "Its words, lines parted by \\n."),
                 given("format", "object", "The format of its words: any of family, size, bold, italic, underline, color, alignment, capitalization, stroke_color, stroke_width, kerning."),
                 given("properties", "object",
                       "Its other properties by name, as get_slide shows an element: name, x, y, width, height, opacity, "
                       "fill_on, fill_color, stroke_on, stroke_color, stroke_width, shadow_enabled, vertical_alignment, "
                       "margin_left, text_scale, and the rest. (They may also be given beside `kind`, not inside this.)")}});
    all.append({"update_element", "PATCH", "presentation/{presentation}/slide/{slide}/element/{element}",
                "theme/{theme}/slide/{slide}/element/{element}", Operation::Change, "Change an element",
                "Changes an element of a slide; only what is given is changed. Send back any of the properties get_slide "
                "shows for it, changed. `text` changes its words and keeps how they are set; `format` formats all of them; "
                "`paragraphs` replaces them with text set run by run. On a presentation's slide, or with `theme` a theme's.",
                {in("presentation", kPresentation), in("slide", kSlide), in("element", kElement), asked("library", "string", kLibraryOf),
                 given("text", "string", "Its words, lines parted by \\n."),
                 given("format", "object", "A format for all its words: any of family, size, bold, italic, underline, strikethrough, color, alignment, capitalization, stroke_color, stroke_width, kerning."),
                 given("paragraphs", "array", "Its text anew, each paragraph {alignment, runs: [{text, …format}]}.", false, "object"),
                 given("chords", "array", "The chords over its words, each {at, name}: `at` the place in the words, counted in characters from 0.", false, "object"),
                 given("order", "integer", "Its place among the slide's elements, 0 the furthest back."),
                 given("properties", "object", "Its other properties by name (see get_reference, topic \"elements\"). They may also be given beside these, not inside this.")}});
    all.append({"delete_element", "DELETE", "presentation/{presentation}/slide/{slide}/element/{element}",
                "theme/{theme}/slide/{slide}/element/{element}", Operation::Remove, "Take an element off a slide",
                "Takes an element off a slide. On a presentation's slide, or with `theme` a theme's.",
                {in("presentation", kPresentation), in("slide", kSlide), in("element", kElement), asked("library", "string", kLibraryOf)}});

    // ---- a slide's actions

    all.append({"add_action", "POST", "presentation/{presentation}/slide/{slide}/actions", {}, Operation::Change, "Give a slide an action",
                "Gives a slide another thing to do when it is triggered, besides showing itself: work a timer, clear a "
                "layer or a clear group, give the stage a layout, put a prop on or take it off, make a look live, run a "
                "macro, show a message, play audio or media of the bins. What each `kind` takes is in get_reference, topic "
                "\"actions\"; list_action_targets has the ids to name things by. Answers with the slide's actions.",
                {in("presentation", kPresentation), in("slide", kSlide), asked("library", "string", kLibraryOf),
                 given("kind", "string", "What it does.", true, {},
                       {"timer", "clear", "clear_group", "stage", "prop", "look", "macro", "message", "audio", "media",
                        "media_playlist", "audio_playlist", "video_input", "communication"}),
                 given("properties", "object", "What that kind takes (they may also be given beside `kind`).")}});
    all.append({"update_action", "PATCH", "presentation/{presentation}/slide/{slide}/action/{action}", {}, Operation::Change, "Change one of a slide's actions",
                "Changes an action a slide has, by the action's id: give all that its kind takes, as for add_action.",
                {in("presentation", kPresentation), in("slide", kSlide), in("action", "The action's id, as get_presentation gives it."),
                 asked("library", "string", kLibraryOf),
                 given("kind", "string", "What it does.", true),
                 given("properties", "object", "What that kind takes (they may also be given beside `kind`).")}});
    all.append({"delete_action", "DELETE", "presentation/{presentation}/slide/{slide}/action/{action}", {}, Operation::Remove, "Take an action off a slide",
                "Takes one of a slide's actions away, by the action's id.",
                {in("presentation", kPresentation), in("slide", kSlide), in("action", "The action's id."),
                 asked("library", "string", kLibraryOf)}});

    // ---- themes

    all.append({"list_themes", "GET", "themes", {}, Operation::Read, "The themes",
                "The workspace's themes, each with its id, name, place and its slides' ids and names.", {}, false});
    all.append({"get_theme", "GET", "theme/{theme}", {}, Operation::Read, "A theme, with its slides",
                "A theme: its slides, each with its name, size, background and elements in full, which is how slides "
                "dressed in it will look.",
                {in("theme", kTheme)}});
    all.append({"create_theme", "POST", "themes", {}, Operation::Change, "Make a theme",
                "Makes a new theme. With `slides`, each {name, background, elements}; without, it has one slide with a "
                "text box over the whole of it, to be changed with update_element. A slide dressed in a theme puts its "
                "words in the theme's text boxes that have words in them (\"Lyrics\"), so give each text box some. Answers "
                "with the theme.",
                {given("name", "string", "What it is called.", true),
                 given("slides", "array", "Its slides, each {name, background, width, height, elements: [as add_element takes]}.", false, "object")}});
    all.append({"delete_theme", "DELETE", "theme/{theme}", {}, Operation::Remove, "Delete a theme",
                "Moves a theme's folder to the computer's Trash. Slides already dressed in it stay as they are.",
                {in("theme", kTheme)}});
    all.append({"add_theme_slide", "POST", "theme/{theme}/slides", {}, Operation::Change, "Add a slide to a theme",
                "Adds a slide to a theme: with nothing but a text box on it, or a copy of another of its slides, or as "
                "described. Answers with the slide.",
                {in("theme", kTheme),
                 given("name", "string", "What it is called."),
                 given("copy_of", "string", "A slide of the theme to make it a copy of, by id, name or number."),
                 given("background", "string", "Its background colour."),
                 given("elements", "array", "Its elements, as add_element takes them.", false, "object")}});
    all.append({"update_theme_slide", "PATCH", "theme/{theme}/slide/{slide}", {}, Operation::Change, "Rename a theme's slide, or give it a background",
                "Changes a theme's slide itself: its name and its background colour. What is on it is changed with "
                "update_element.",
                {in("theme", kTheme), in("slide", kThemeSlide),
                 given("name", "string", "Another name."),
                 given("background", "string", "Its background colour; \"\" for none.")}});
    all.append({"delete_theme_slide", "DELETE", "theme/{theme}/slide/{slide}", {}, Operation::Remove, "Delete a theme's slide",
                "Takes a slide out of a theme. A theme keeps at least one.",
                {in("theme", kTheme), in("slide", kThemeSlide)}});

    // ---- playlists

    all.append({"list_playlists", "GET", "playlists", {}, Operation::Read, "The playlists",
                "The playlists and their folders, in the order they are listed in, each with its id, name, depth and the "
                "folder it is in.", {}, false});
    all.append({"get_playlist", "GET", "playlist/{playlist}", {}, Operation::Read, "A playlist's items",
                "A playlist: its items in order, each a heading (its name and colour) or a presentation (its uuid, name "
                "and library, the arrangement the row shows it in, and whether its file is missing).",
                {in("playlist", kPlaylist)}});
    all.append({"create_playlist", "POST", "playlists", {}, Operation::Change, "Make a playlist or a folder of playlists",
                "Makes a new playlist, with its items if they are given, or a folder to keep playlists in. Answers with it.",
                {given("name", "string", "What it is called.", true),
                 given("folder", "boolean", "Make a folder of playlists and not a playlist."),
                 given("parent", "string", "The folder to make it in, by id or name; at the top without."),
                 given("items", "array",
                       "A new playlist's items, in order: each {presentation, library, arrangement} for a presentation (by "
                       "uuid or name; the arrangement by name, the presentation's own choice without) or {header, color} "
                       "for a heading.",
                       false, "object")}});
    all.append({"update_playlist", "PATCH", "playlist/{playlist}", {}, Operation::Change, "Rename or move a playlist",
                "Gives a playlist or a folder another name, or moves it: before or after another, or into a folder.",
                {in("playlist", kPlaylist),
                 given("name", "string", "Another name."),
                 given("before", "string", "The playlist or folder it is to come just before."),
                 given("after", "string", "Or the one it is to come just after."),
                 given("into", "string", "Or the folder it is to go into.")}});
    all.append({"delete_playlist", "DELETE", "playlist/{playlist}", {}, Operation::Remove, "Delete a playlist",
                "Removes a playlist, or a folder with every playlist in it. The presentations its rows named are not "
                "touched: they stay in their libraries.",
                {in("playlist", kPlaylist)}});
    all.append({"set_playlist_items", "PUT", "playlist/{playlist}/items", {}, Operation::Change, "Give a playlist these items and no others",
                "Makes a playlist hold exactly the items given, in that order: the way to lay out a whole service at once. "
                "An item that is there already (the same heading, or the same presentation) keeps its row; the rest are "
                "made, and rows not given are removed.",
                {in("playlist", kPlaylist),
                 given("items", "array",
                       "Its items, in order: each {presentation, library, arrangement} for a presentation (by uuid or "
                       "name) or {header, color} for a heading.",
                       true, "object")}});
    all.append({"add_playlist_items", "POST", "playlist/{playlist}/items", {}, Operation::Change, "Add items to a playlist",
                "Adds items to a playlist, at its end or under one of its rows.",
                {in("playlist", kPlaylist),
                 given("items", "array", "The items to add, in order: each {presentation, library, arrangement} or {header, color}.", true, "object"),
                 given("after", "string", "The row they go under: its id, or its number from 0; at the end without.")}});
    all.append({"update_playlist_item", "PATCH", "playlist/{playlist}/item/{item}", {}, Operation::Change, "Change or move a playlist's row",
                "Changes one row of a playlist: a presentation's arrangement, a heading's words or colour, or where the row is.",
                {in("playlist", kPlaylist), in("item", "The row: its id, or its number from 0."),
                 given("arrangement", "string", "For a presentation: the arrangement the row shows it in, by name; \"\" for every group in order."),
                 given("name", "string", "For a heading: its words."),
                 given("color", "string", "For a heading: its colour, \"#rrggbb\"."),
                 given("before", "string", "Moves it to just before this row."),
                 given("after", "string", "Or to just after this one.")}});
    all.append({"delete_playlist_item", "DELETE", "playlist/{playlist}/item/{item}", {}, Operation::Remove, "Remove a row from a playlist",
                "Takes a row out of a playlist. A presentation it named stays in its library.",
                {in("playlist", kPlaylist), in("item", "The row: its id, or its number from 0.")}});

    // ---- what a slide can bring and work

    all.append({"list_media", "GET", "media", {}, Operation::Read, "The workspace's media files",
                "The pictures, videos and sound files under the workspace's Media folder, each with its path, to give a "
                "slide as its media or an element as its picture. A folder's own files, and the folders in it.",
                {asked("folder", "string", "A folder under Media to look in; Media itself without.")}});
    all.append({"list_action_targets", "GET", "action_targets", {}, Operation::Read, "What a slide's actions can work",
                "The things of the workspace that a slide's action can name, each with its id and name: timers, macros, "
                "looks, props, messages, stage screens and layouts, clear groups, and the playlists of the media bin and "
                "of the audio bin with their rows.",
                {}});
    return all;
}

QStringList parts(const QString &path)
{
    return path.split(u'/', Qt::SkipEmptyParts);
}

// How a pattern fits a path: none if it does not; otherwise which of its parts were
// words, first first (see httpapi's own, which this is the twin of)
bool fits(const QStringList &pattern, const QStringList &path, QVariantMap *params, QList<bool> *words)
{
    if (pattern.size() != path.size())
        return false;
    QVariantMap found;
    QList<bool> made;
    for (qsizetype i = 0; i < pattern.size(); ++i) {
        const QString &part = pattern[i];
        if (part.startsWith(u'{') && part.endsWith(u'}')) {
            found.insert(part.mid(1, part.size() - 2), path[i]);
            made.append(false);
        } else if (part == path[i]) {
            made.append(true);
        } else {
            return false;
        }
    }
    *params = found;
    *words = made;
    return true;
}

// The parts after the prefix of a path as it came, each with its %-escapes undone; none
// (and false) for a path not under the prefix
bool under(const QString &rawPath, QStringList *rest)
{
    if (!rawPath.startsWith(prefix()))
        return false;
    rest->clear();
    for (const QString &part : parts(rawPath.mid(prefix().size())))
        rest->append(QUrl::fromPercentEncoding(part.toLatin1()));
    return true;
}

}

const QList<Operation> &operations()
{
    static const QList<Operation> all = table();
    return all;
}

const Operation *operation(const QByteArray &name)
{
    for (const Operation &each : operations()) {
        if (each.name == name)
            return &each;
    }
    return nullptr;
}

Match match(const QByteArray &method, const QString &rawPath)
{
    QStringList asked;
    if (!under(rawPath, &asked))
        return {};
    Match made;
    QList<bool> best;
    for (const Operation &each : operations()) {
        if (each.method != method)
            continue;
        for (const bool theme : {false, true}) {
            const QByteArray &pattern = theme ? each.themePattern : each.pattern;
            if (pattern.isEmpty())
                continue;
            QVariantMap params;
            QList<bool> words;
            if (!fits(parts(QString::fromLatin1(pattern)), asked, &params, &words))
                continue;
            if (!made.operation || words > best) {
                made.operation = &each;
                made.params = params;
                made.theme = theme;
                best = words;
            }
        }
    }
    return made;
}

bool own(const QString &path)
{
    return path.startsWith(prefix()) || path == mcpPath();
}

QList<QByteArray> methodsOf(const QString &rawPath)
{
    QStringList asked;
    QList<QByteArray> methods;
    if (!under(rawPath, &asked))
        return methods;
    for (const Operation &each : operations()) {
        for (const QByteArray &pattern : {each.pattern, each.themePattern}) {
            QVariantMap params;
            QList<bool> words;
            if (!pattern.isEmpty() && fits(parts(QString::fromLatin1(pattern)), asked, &params, &words) && !methods.contains(each.method))
                methods.append(each.method);
        }
    }
    return methods;
}

QString pathOf(const Operation &operation, const QVariantMap &values, bool theme)
{
    const QByteArray &pattern = theme && !operation.themePattern.isEmpty() ? operation.themePattern : operation.pattern;
    QStringList made;
    for (const QString &part : parts(QString::fromLatin1(pattern))) {
        if (!part.startsWith(u'{')) {
            made.append(part);
            continue;
        }
        const QString value = values.value(part.mid(1, part.size() - 2)).toString();
        if (value.isEmpty())
            return {};
        made.append(QString::fromLatin1(QUrl::toPercentEncoding(value)));
    }
    return prefix() + made.join(u'/');
}

namespace {

QVariantMap schemaOf(const Field &field)
{
    QVariantMap schema;
    if (field.type != "any")
        schema.insert(QStringLiteral("type"), QString::fromLatin1(field.type));
    schema.insert(QStringLiteral("description"), QString::fromUtf8(field.description));
    if (field.type == "array" && !field.items.isEmpty() && field.items != "any")
        schema.insert(QStringLiteral("items"), QVariantMap {{QStringLiteral("type"), QString::fromLatin1(field.items)}});
    if (!field.choices.isEmpty()) {
        QVariantList choices;
        for (const QByteArray &choice : field.choices)
            choices.append(QString::fromLatin1(choice));
        schema.insert(QStringLiteral("enum"), choices);
    }
    return schema;
}

// A field's name as an MCP tool is given it: the operations on a theme's slide take
// `theme` where a presentation's take `presentation`, and a tool takes either
bool takesTheme(const Operation &operation)
{
    return !operation.themePattern.isEmpty();
}

}

QVariantMap inputSchema(const Operation &operation)
{
    QVariantMap properties;
    QVariantList required;
    for (const Field &field : operation.fields) {
        QVariantMap schema = schemaOf(field);
        if (takesTheme(operation) && field.name == "presentation")
            schema.insert(QStringLiteral("description"),
                          QString::fromUtf8(field.description) + QStringLiteral(" (Leave out, and give `theme`, for a theme's slide.)"));
        properties.insert(QString::fromLatin1(field.name), schema);
        // (Where a theme will do in its place, neither is asked for outright: the
        // operation says which it lacked.)
        if (field.required && !(takesTheme(operation) && field.name == "presentation"))
            required.append(QString::fromLatin1(field.name));
    }
    if (takesTheme(operation)) {
        properties.insert(QStringLiteral("theme"),
                          QVariantMap {{QStringLiteral("type"), QStringLiteral("string")},
                                       {QStringLiteral("description"),
                                        QStringLiteral("For a theme's slide, in place of `presentation`: ") + QString::fromUtf8(kTheme)}});
    }
    QVariantMap schema {{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("properties"), properties}};
    if (!required.isEmpty())
        schema.insert(QStringLiteral("required"), required);
    return schema;
}

namespace {

// The parts the operations are listed in on the page of documentation, and in the
// reference (whose headings these are, but for the first, which is what is asked of the
// workspace as a whole)
struct Section
{
    const char *name;
    const char *about;
};
const QList<Section> &sectionList()
{
    static const QList<Section> all {
        {"Workspace", "The open workspace as a whole: what is in it, its libraries, finding a presentation, and this API's own description of itself."},
        {"Presentations", "A presentation: made, read, changed, copied and deleted; its groups, its arrangements and its timeline; a theme put on "
                          "its slides, and their text formatted."},
        {"Slides", "A presentation's slides: added, read, changed, moved, copied and deleted."},
        {"Elements", "What is on a slide: its text boxes, shapes, pictures and videos."},
        {"Actions", "What a slide does besides showing itself."},
        {"Themes", "The themes, their slides, and what is on those."},
        {"Playlists", "The playlists of presentations, their folders and their rows."},
        {"Media", "The media files a slide can be given."},
    };
    return all;
}

}

QStringList sections()
{
    QStringList names;
    for (const Section &section : sectionList())
        names.append(QString::fromLatin1(section.name));
    return names;
}

QString sectionOf(const Operation &operation)
{
    const QByteArray &name = operation.name;
    if (name.contains("playlist"))
        return QStringLiteral("Playlists");
    if (name.contains("theme") && name != "apply_theme")
        return QStringLiteral("Themes");
    if (name.contains("element"))
        return QStringLiteral("Elements");
    if (name.contains("action"))
        return QStringLiteral("Actions");
    if (name.contains("slide"))
        return QStringLiteral("Slides");
    if (name == "list_media")
        return QStringLiteral("Media");
    // (What is asked of a library or of all of them is the workspace's; what is asked
    // of one presentation, by its path, is the presentation's)
    if (operation.pattern.startsWith("presentation/") || name == "create_presentation")
        return QStringLiteral("Presentations");
    return QStringLiteral("Workspace");
}

namespace {

// Whether an operation makes something new, and so answers 201 where the others answer 200
bool makes(const Operation &operation)
{
    return operation.method == "POST"
        && (operation.name.startsWith("create_") || operation.name.startsWith("add_") || operation.name.startsWith("duplicate_"));
}

}

QVariantMap openApi(const QString &version)
{
    QVariantMap paths;
    const auto add = [&paths](const Operation &operation, const QByteArray &pattern, bool theme) {
        const QString path = prefix() + QString::fromLatin1(pattern);
        QVariantMap methods = paths.value(path).toMap();
        QVariantList parameters;
        QVariantMap properties;
        QVariantList required;
        for (const Field &field : operation.fields) {
            // (A theme's path has `theme` where a presentation's has `presentation`,
            // and no library to tell presentations apart by.)
            QByteArray name = field.name;
            QByteArray description = field.description;
            if (theme && name == "presentation") {
                name = "theme";
                description = kTheme;
            } else if (theme && name == "slide") {
                description = kThemeSlide;
            } else if (theme && name == "library") {
                continue;
            }
            if (field.where == Field::Body) {
                properties.insert(QString::fromLatin1(name), schemaOf(field));
                if (field.required)
                    required.append(QString::fromLatin1(name));
                continue;
            }
            QVariantMap schema = schemaOf(field);
            schema.remove(QStringLiteral("description"));
            parameters.append(QVariantMap {{QStringLiteral("name"), QString::fromLatin1(name)},
                                           {QStringLiteral("in"), field.where == Field::Path ? QStringLiteral("path") : QStringLiteral("query")},
                                           {QStringLiteral("required"), field.where == Field::Path || field.required},
                                           {QStringLiteral("description"), QString::fromUtf8(description)},
                                           {QStringLiteral("schema"), schema}});
        }
        QVariantMap made {{QStringLiteral("operationId"), QString::fromLatin1(operation.name) + (theme ? QStringLiteral("_of_theme") : QString())},
                          // (Under its part of the page: a theme's slide and what is on it under the themes)
                          {QStringLiteral("tags"), QStringList {theme ? QStringLiteral("Themes") : sectionOf(operation)}},
                          {QStringLiteral("summary"), QString::fromUtf8(operation.title)},
                          {QStringLiteral("description"), QString::fromUtf8(operation.description)},
                          {QStringLiteral("responses"),
                           QVariantMap {{makes(operation) ? QStringLiteral("201") : QStringLiteral("200"),
                                         QVariantMap {{QStringLiteral("description"),
                                                       makes(operation) ? QStringLiteral("Made: what was made, as JSON.")
                                                                        : QStringLiteral("Done: what was asked for, or what was changed, as JSON.")}}},
                                        {QStringLiteral("default"),
                                         QVariantMap {{QStringLiteral("description"),
                                                       QStringLiteral("Not done: 400 for what cannot be understood, 404 for what is not there, 409 for what is in the way, 403 for who may not ask. The body is {\"error\": why}.")}}}}}};
        if (!parameters.isEmpty())
            made.insert(QStringLiteral("parameters"), parameters);
        if (!properties.isEmpty()) {
            QVariantMap schema {{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("properties"), properties}};
            if (!required.isEmpty())
                schema.insert(QStringLiteral("required"), required);
            made.insert(QStringLiteral("requestBody"),
                        QVariantMap {{QStringLiteral("required"), !required.isEmpty()},
                                     {QStringLiteral("content"),
                                      QVariantMap {{QStringLiteral("application/json"), QVariantMap {{QStringLiteral("schema"), schema}}}}}});
        }
        methods.insert(QString::fromLatin1(operation.method).toLower(), made);
        paths.insert(path, methods);
    };
    for (const Operation &operation : operations()) {
        add(operation, operation.pattern, false);
        if (!operation.themePattern.isEmpty())
            add(operation, operation.themePattern, true);
    }
    QVariantList described;
    for (const Section &section : sectionList())
        described.append(QVariantMap {{QStringLiteral("name"), QString::fromLatin1(section.name)},
                                      {QStringLiteral("description"), QString::fromUtf8(section.about)}});
    // (The opening words are Markdown, which the page of documentation draws: its
    // links are to the app's other page and to the reference, on whatever served this.)
    return {{QStringLiteral("openapi"), QStringLiteral("3.1.0")},
            {QStringLiteral("info"),
             QVariantMap {{QStringLiteral("title"), QStringLiteral("Simple Presenter: the extended API")},
                          {QStringLiteral("version"), version},
                          {QStringLiteral("description"),
                           QStringLiteral("Simple Presenter's own addition to ProPresenter's API, for making and changing the "
                                          "presentations, playlists and themes of the open workspace. It is one person's hobby "
                                          "project, shared as an example and not as a product: nothing here is promised to stay "
                                          "as it is.\n\n"
                                          "Tried from a page, a request **changes the workspace's files**: what is deleted goes to the "
                                          "Trash, and a copy of each file is kept before it is changed.\n\n"
                                          "`GET /sp/v1/reference` has every property by name. ProPresenter's own API, "
                                          "as far as the app answers it, has [a page of its own](/v1/doc/index.html).")}}},
            // (Asked of whatever served this: an address with nothing before its first
            // slash is that computer's own)
            {QStringLiteral("servers"), QVariantList {QVariantMap {{QStringLiteral("url"), QStringLiteral("/")},
                                                                   {QStringLiteral("description"), QStringLiteral("The app that served this")}}}},
            {QStringLiteral("tags"), described},
            {QStringLiteral("paths"), paths}};
}

// ---- who may ask

namespace {

// The host of an Origin ("http://host:port") or of a Host ("host:port"), without its
// port and without an IPv6 address's brackets; "" if there is none to be read
QString hostOf(const QByteArray &text, bool isOrigin)
{
    QString host = QString::fromLatin1(text).trimmed();
    if (isOrigin) {
        const QUrl url(host, QUrl::StrictMode);
        return url.isValid() ? url.host() : QString();
    }
    if (host.startsWith(u'[')) {
        const qsizetype end = host.indexOf(u']');
        return end < 0 ? QString() : host.mid(1, end - 1);
    }
    // (One colon is a port's; more are an IPv6 address written bare)
    if (host.count(u':') == 1)
        host = host.left(host.indexOf(u':'));
    return host;
}

// The endings a name has that only a local network can give it: no name of the
// internet's can end so (they are set aside for this), so a page out there cannot make
// one of its own point here
const QStringList &localEndings()
{
    static const QStringList endings {QStringLiteral(".local"), QStringLiteral(".lan"), QStringLiteral(".home"), QStringLiteral(".home.arpa"),
                                      QStringLiteral(".internal"), QStringLiteral(".localdomain"), QStringLiteral(".localhost")};
    return endings;
}

bool hasLocalEnding(const QString &host)
{
    for (const QString &ending : localEndings()) {
        if (host.endsWith(ending, Qt::CaseInsensitive) && host.size() > ending.size())
            return true;
    }
    return false;
}

bool isLoopbackName(const QString &host)
{
    return host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 || host.endsWith(QLatin1String(".localhost"), Qt::CaseInsensitive);
}

// Whether a name is this computer's own: the name it has, as it is, or its first part
// alone, or that with a local network's ending straight after it ("presenter",
// "presenter.local"). The whole of the name is looked at, and not its first part
// alone: "presenter.example.com" is somebody else's.
bool isMachine(const QString &host, const QString &machineName)
{
    if (machineName.isEmpty())
        return false;
    if (host.compare(machineName, Qt::CaseInsensitive) == 0)
        return true;
    const QString first = machineName.section(u'.', 0, 0);
    if (host.compare(first, Qt::CaseInsensitive) == 0)
        return true;
    return host.startsWith(first + u'.', Qt::CaseInsensitive) && localEndings().contains(host.mid(first.size()).toLower());
}

// Whether a name is one a local network gives a computer, and not one of the
// internet's: one word ("presenter", which only a network's own search finds), or one
// with a local ending
bool isLocalName(const QString &host)
{
    return !host.isEmpty() && (!host.contains(u'.') || hasLocalEnding(host));
}

// Whether an address is one a local network gives out, and not one of the internet's
bool isPrivate(const QHostAddress &address)
{
    return address.isLoopback() || address.isLinkLocal() || address.isUniqueLocalUnicast() || address.isPrivateUse();
}

}

bool originAllowed(const QByteArray &origin, const Access &access)
{
    // No Origin: not a web page (a program, a script, an MCP client).
    const QByteArray asked = origin.trimmed();
    if (asked.isEmpty())
        return true;
    // "null" is what a browser says where it will not say (a page in a sandbox, a form
    // sent from a page that keeps where it is from to itself): from anywhere, then, and
    // not let in. (A page opened from a file on this computer says it too, and is
    // turned away with them: a page that is to use the API is served by something.)
    if (asked == "null")
        return false;
    const QString host = hostOf(asked, true);
    if (host.isEmpty())
        return false;
    QHostAddress address;
    if (address.setAddress(host)) {
        // A page of this computer's own, by a number; or with the network let in, one
        // served by a computer of the local network
        if (address.isLoopback() || access.ownAddresses.contains(address.toString()))
            return true;
        return access.network && isPrivate(address);
    }
    if (isLoopbackName(host) || isMachine(host, access.machineName))
        return true;
    return access.network && isLocalName(host);
}

bool hostAllowed(const QByteArray &host, const QString &machineName)
{
    // (HTTP/1.0 sends none; anything else that sends none is not a browser.)
    if (host.trimmed().isEmpty())
        return true;
    const QString name = hostOf(host, false);
    if (name.isEmpty())
        return false;
    QHostAddress address;
    if (address.setAddress(name))
        return true;
    return isLoopbackName(name) || isMachine(name, machineName) || isLocalName(name);
}

bool off(const Access &access)
{
    return !access.enabled;
}

QString refusal(const Asker &asker, const Access &access)
{
    if (!access.enabled)
        return QStringLiteral("The extended API is switched off: it is switched on in Simple Presenter's Settings, under Network.");
    if (!asker.local && !access.network)
        return QStringLiteral("The extended API answers only this computer: other computers are let in by Settings, Network, From Other Computers.");
    if (!hostAllowed(asker.host, access.machineName))
        return QStringLiteral("The extended API is asked for by this computer's address or its own name, not by another name.");
    if (!originAllowed(asker.origin, access))
        return QStringLiteral("The extended API does not answer a web page from elsewhere.");
    return {};
}

}
