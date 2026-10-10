#!/usr/bin/env python3
"""Runs the scripted tests: the whole app, worked by scripts, on workspaces of its own.

    tests/ui/run.py                 every test
    tests/ui/run.py props stage     those two
    tests/ui/run.py --list          what tests there are

Each test is a QML script (tests/ui/t/<name>.qml) that the app loads into its operator
window and runs: it clicks and types as someone would, looks at what the windows then
hold and show, and prints a line for each thing it checks. The app has to have been
built with the door such a script comes in by:

    cmake -B build-tests -G Ninja -DSIMPLEPRESENTER_TEST_HOOK=ON
    cmake --build build-tests

Nothing of a run touches the desktop it is started from, the real settings or the real
workspaces:

  - Every test gets a fresh copy of the workspace it runs on, fresh settings and a
    documents folder of its own, in a folder made for the run under the system's
    temporary folder. (The media of the copied workspaces is a link to the real media
    folders, which are only read.)
  - Most tests need the windows really drawn, with a graphics card, and a pointer and
    a keyboard that behave as real ones do. They get a desktop of their own: a GNOME
    Shell with no screen, started for the run on a session bus of its own and stopped
    after it. Nothing of it shows anywhere. The few tests that can do without are drawn
    into memory.
  - Sound is played into a private audio service with one sink that goes nowhere: the
    real one and the sound card are not touched, and nothing is heard.

The run's folder holds each test's output (logs/<name>.log) and the pictures it took
(frames/<name>/). It is removed afterwards if every test passed, and kept, with its path
printed, if any did not (or if --keep was asked for).

The workspaces the tests run on are in tests/ui/fixtures, which is not in the
repository: see tests/README.md.
"""
import argparse
import glob
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
FIXTURES = os.path.join(HERE, "fixtures")
PROTO = os.path.join(REPO, "third_party", "ProPresenter7-Proto", "autogen-proto")

# What the app and its libraries say that is of no interest to a test
NOISE = re.compile(r"VDPAU|vulkan|dbus|libva|vendor_id|^\s*$|Input #|Metadata|Duration|Stream #|handler_name|encoder|major_brand"
                   r"|minor_version|compatible_brands|creation_time|timecode|Side data|displaymatrix|cpb:")
# What a test says when a check fails, or what QML says when a script is wrong
FAILED = re.compile(r"FAILED|EXCEPTION|SCRIPT DID NOT LOAD")
QML_ERROR = re.compile(r"TypeError|ReferenceError|qrc:.*: |Internal error|is not a type|Binding loop|Unable to assign")
FINISHED = re.compile(r"ALL [0-9]+ CHECKS PASSED|CHECKS FAILED|ALL PASSED")


# ---- the files a test is given

def protoc(direction, message, proto, data):
    """Text to ProPresenter's file format ("encode") or back ("decode")."""
    done = subprocess.run(["protoc", "--%s=%s" % (direction, message), "-I", PROTO, proto], input=data, capture_output=True)
    if done.returncode != 0:
        raise RuntimeError("protoc could not %s a %s: %s" % (direction, message, done.stderr.decode()[:300]))
    return done.stdout


def copy_workspace(name, test_dir):
    target = os.path.join(test_dir, "WorkSpaces", name)
    shutil.copytree(os.path.join(FIXTURES, "workspaces", name), target, symlinks=True)
    return target


KEY_MAPPINGS = """\
application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER }
keymappings { keyboard { key_equivalent: "k" key_equivalent_modifier_flags: MODIFIERFLAGS_COMMAND_KEY } menu_item: "someMenuItem" }
keymappings { keyboard { key_equivalent: "g" key_equivalent_modifier_flags: MODIFIERFLAGS_SHIFT_KEY } group_identifier { parameter_name: "Chorus" } }
macos_keymappings { midi { channel: 1 pitch: 60 velocity: 100 } group_identifier { parameter_name: "Verse" } }
keymappings { keyboard { key_equivalent: "q" } group_identifier { parameter_name: "Tag" } }
"""
# ProPresenter's own list of groups, with its hotkeys: A and S for the verses, C for the chorus, none for the tag.
GROUPS = """\
groups { uuid { string: "11111111-2222-3333-4444-555555555555" } name: "Verse" color { blue: 1 alpha: 1 } hotKey { code: KEY_CODE_ANSI_A } }
groups { uuid { string: "22222222-2222-3333-4444-555555555555" } name: "Verse 2" color { blue: 1 alpha: 1 } hotKey { code: KEY_CODE_ANSI_S } application_group_name: "kept" }
groups { uuid { string: "33333333-2222-3333-4444-555555555555" } name: "Chorus" color { red: 1 alpha: 1 } hotKey { code: KEY_CODE_ANSI_C control_identifier: "kept too" } }
groups { uuid { string: "44444444-2222-3333-4444-555555555555" } name: "Tag" color { green: 1 alpha: 1 } hotKey { } }
"""
# Settings as an earlier version of the app left them, with the hotkeys of the groups in them
LEGACY_SETTINGS = r"""[General]
groups="[{\"name\":\"Intro\",\"color\":\"#fdd835\"},{\"name\":\"Verse\",\"color\":\"#1e88e5\",\"key\":\"V\"},{\"name\":\"Chorus\",\"color\":\"#d81b60\",\"key\":\"C\"},{\"name\":\"Bridge\",\"color\":\"#8e24aa\"},{\"name\":\"Tag\",\"color\":\"#f4511e\",\"key\":\"T\"}]"
"""


def prepare_keys(workspace, test_dir, with_groups):
    """Key mappings that hold things other than hotkeys of groups, ProPresenter's list of groups (or none), and
    settings from before the hotkeys were kept in the workspace."""
    configuration = os.path.join(workspace, "Configuration")
    with open(os.path.join(configuration, "KeyMappings"), "wb") as out:
        out.write(protoc("encode", "rv.data.KeyMappingDocument", "keymapping.proto", KEY_MAPPINGS.encode()))
    shutil.copy(os.path.join(configuration, "KeyMappings"), os.path.join(test_dir, "KeyMappings.before"))
    if with_groups:
        with open(os.path.join(configuration, "Groups"), "wb") as out:
            out.write(protoc("encode", "rv.data.ProGroupsDocument", "groups.proto", GROUPS.encode()))
    settings = os.path.join(test_dir, "config", "SimplePresenter")
    os.makedirs(settings, exist_ok=True)
    with open(os.path.join(settings, "SimplePresenter.conf"), "w") as out:
        out.write(LEGACY_SETTINGS)


def prepare_actions(workspace, test_dir):
    """A presentation whose slides each do something to a timer the way a cue made in ProPresenter does (a timer
    action beside the slide's own), and a timers file of three timers for them to act on. Built from one slide
    of a presentation of the workspace, with fresh ids."""
    library = os.path.join(workspace, "Libraries", "Demo")
    with open(os.path.join(library, "Move Of God.pro"), "rb") as source:
        text = protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()
    blocks = re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)
    template = next(b for b in blocks if "ACTION_TYPE_PRESENTATION_SLIDE" in b and "rtf_data" in b and "ACTION_TYPE_MEDIA" not in b)
    header = re.search(r"^application_info \{\n.*?^\}\n", text, re.S | re.M).group(0)

    def new():
        return str(uuid.uuid4()).upper()

    one, two, elapsed = "11111111-AAAA-4AAA-8AAA-111111111111", "22222222-BBBB-4BBB-8BBB-222222222222", "33333333-CCCC-4CCC-8CCC-333333333333"
    nobody = "99999999-DDDD-4DDD-8DDD-999999999999"

    def timer(action, timer_id="", name="", duration=None, amount=None):
        s = ('  actions {\n    uuid {\n      string: "%s"\n    }\n    isEnabled: true\n    type: ACTION_TYPE_TIMER\n    timer {\n'
             '      action_type: %s\n      timer_identification {\n' % (new(), action))
        if timer_id:
            s += '        parameter_uuid {\n          string: "%s"\n        }\n' % timer_id
        if name:
            s += '        parameter_name: "%s"\n' % name
        s += "      }\n"
        if duration is not None:
            s += "      timer_configuration {\n        countdown {\n          duration: %d\n        }\n      }\n" % duration
        if amount is not None:
            s += "      increment_amount: %d\n" % amount
        return s + "    }\n  }\n"

    cases = [
        [timer("ACTION_RESET_AND_START", one, "Two")],                  # 1 the id says One, the name says Two
        [timer("ACTION_START", nobody, "Two")],                         # 2 no timer has the id; the name is Two's
        [timer("ACTION_RESET_AND_START", nobody, "Nobody")],            # 3 neither is any timer's
        [timer("ACTION_STOP", one)],                                    # 4
        [timer("ACTION_RESET", one)],                                   # 5
        [timer("ACTION_RESET_AND_START", nobody, "Two", duration=60)],  # 6 as ProPresenter wrote the user's own
        [timer("ACTION_STOP_AND_RESET", two)],                          # 7
        [timer("ACTION_INCREMENT", one, amount=30)],                    # 8
        [timer("ACTION_START", one), timer("ACTION_START", "", "Elapsed")],  # 9 two actions on one cue
        [timer("ACTION_RESET_AND_START", two, "One")],                  # 10 the id says Two, the name says One
        [],                                                             # 11 a slide that does nothing to any timer
    ]
    cues, ids = [], []
    for actions in cases:
        cue = re.sub(r'string: "[0-9A-F-]{36}"', lambda m: 'string: "%s"' % new(), template)
        ids.append(re.search(r'string: "([0-9A-F-]{36})"', cue).group(1))
        assert cue.endswith("}\n")
        cues.append(cue[:-2] + "".join(actions) + "}\n")
    group = ('cue_groups {\n  group {\n    uuid {\n      string: "%s"\n    }\n    name: "Actions"\n    hotKey {\n    }\n  }\n' % new()
             + "".join('  cue_identifiers {\n    string: "%s"\n  }\n' % i for i in ids) + "}\n")
    made = header + 'uuid {\n  string: "%s"\n}\nname: "Timer Actions"\n' % new() + "".join(cues) + group
    with open(os.path.join(library, "Timer Actions.pro"), "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", made.encode()))
    timers = "".join('timers {\n  uuid {\n    string: "%s"\n  }\n  name: "%s"\n  configuration {\n%s  }\n}\n' % (i, n, c) for i, n, c in [
        (one, "One", "    countdown {\n      duration: 100\n    }\n"), (two, "Two", "    countdown {\n      duration: 200\n    }\n"),
        (elapsed, "Elapsed", "    elapsed_time {\n    }\n    allows_overrun: true\n")])
    os.makedirs(os.path.join(workspace, "Configuration"), exist_ok=True)
    with open(os.path.join(workspace, "Configuration", "Timers"), "wb") as out:
        out.write(protoc("encode", "rv.data.TimersDocument", "timers.proto", timers.encode()))


# ---- what is looked at in a workspace's files once a test is over, said as checks

def said(ok, what, detail=""):
    return "  ok      %s" % what if ok else "  FAILED  %s%s" % (what, "   [%s]" % detail if detail else "")


def after_acts(workspace, test_dir):
    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.MacrosDocument", "macros.proto", source.read()).decode()

    now = decoded(os.path.join(workspace, "Configuration", "Macros"))
    was = decoded(os.path.join(FIXTURES, "workspaces", "Act", "Configuration", "Macros"))
    looks = now.count("ACTION_TYPE_AUDIENCE_LOOK"), was.count("ACTION_TYPE_AUDIENCE_LOOK")
    macros = len(re.findall(r"^macros \{", now, re.M)), len(re.findall(r"^macros \{", was, re.M))
    return [
        said(looks[0] == looks[1] and looks[1] > 0, "in the macros file: the actions of kinds not done here are all still there", "%d and %d" % looks),
        said(macros[0] == macros[1] + 1, "in the macros file: one macro more than there was", "%d and %d" % macros),
        said('name: "House Lights"' in now, "in the macros file: the new macro by its name"),
        said(now.count("macro_id") == macros[0], "in the macros file: every macro listed in a collection, once"),
    ]


def after_looks(workspace, test_dir):
    with open(os.path.join(workspace, "Configuration", "Workspace"), "rb") as source:
        text = protoc("decode", "rv.data.ProPresenterWorkspace", "proworkspace.proto", source.read()).decode()
    live = re.search(r"^live_audience_look \{\n(.*?)^\}", text, re.S | re.M)
    name = re.search(r'^  name: "([^"]*)"', live.group(1), re.M).group(1) if live else ""
    presets = len(re.findall(r"^audience_looks \{", text, re.M))
    return [
        said(name == "Notes L3rd", "in the workspace's file: the look made live as the app was closed is the live look", name),
        said(presets == 7 and "original_look_uuid" in (live.group(1) if live else ""),
             "in the workspace's file: the saved looks as they were, and the live look saying which it came from", str(presets)),
    ]


def groups_of(workspace):
    with open(os.path.join(workspace, "Configuration", "Groups"), "rb") as source:
        return protoc("decode", "rv.data.ProGroupsDocument", "groups.proto", source.read()).decode()


def after_keys(workspace, test_dir):
    groups = " ".join(groups_of(workspace).split())
    with open(os.path.join(workspace, "Configuration", "KeyMappings"), "rb") as now, open(os.path.join(test_dir, "KeyMappings.before"), "rb") as was:
        same = now.read() == was.read()
    bridge = re.search(r'name: "Bridge" color \{ red: 0\.55\S* green: 0\.14\S* blue: 0\.66\S* alpha: 1 \} hotKey \{ \} \}', groups)
    return [
        said('groups { uuid { string: "11111111-2222-3333-4444-555555555555" } name: "Verse" color { blue: 1 alpha: 1 } hotKey { code: KEY_CODE_ANSI_A } }' in groups,
             "in the list of groups: the verse as it was", groups),
        said('name: "Verse 2" color { blue: 1 alpha: 1 } hotKey { code: KEY_CODE_ANSI_S } application_group_name: "kept" }' in groups,
             "in the list of groups: a group the settings do not have, as it was, with what else it holds", groups),
        said('groups { uuid { string: "33333333-2222-3333-4444-555555555555" } name: "Chorus" color { red: 1 alpha: 1 } hotKey { control_identifier: "kept too" } }' in groups,
             "in the list of groups: the chorus, its hotkey taken by another group, with the rest of it as it was", groups),
        said(bridge is not None, "in the list of groups: the bridge, added with the colour it has in the settings and no hotkey now", groups),
        said(same, "the key mappings file is byte for byte as it was"),
    ]


def after_keys2(workspace, test_dir):
    groups = groups_of(workspace)
    names = re.findall(r'name: "([^"]*)"', groups)
    codes = re.findall(r"code: (\w+)", groups)
    ids = len(re.findall(r'string: "[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}"', groups))
    return [
        said(names == ["Intro", "Verse", "Chorus", "Bridge", "Tag"], "the list made is of every group of the settings, in their order", " ".join(names)),
        said(codes == ["KEY_CODE_ANSI_V", "KEY_CODE_ANSI_C", "KEY_CODE_ANSI_T"], "with their hotkeys as ProPresenter writes them", " ".join(codes)),
        said(ids == 5, "and an id each", str(ids)),
    ]


def after_screens(workspace, test_dir):
    with open(os.path.join(workspace, "Configuration", "Workspace"), "rb") as source:
        text = protoc("decode", "rv.data.ProPresenterWorkspace", "proworkspace.proto", source.read()).decode()
    screens = re.findall(r"^pro_screens \{\n  name: \"([^\"]*)\"\n  screen_type: (\w+)", text, re.M)
    return [
        said(len(screens) == 16, "in the workspace's file: sixteen screens", str(len(screens))),
        said(screens[:2] == [("Sanctuary", "SCREEN_TYPE_AUDIENCE"), ("Stage", "SCREEN_TYPE_STAGE")],
             "in the workspace's file: the two it started with, the first by its new name", str(screens[:2])),
        said(text.count("type: TYPE_CUSTOM") == 16 and text.count("placeholder:") == 16,
             "in the workspace's file: each written as ProPresenter writes a screen connected to nothing"),
    ]


# ---- the tests: the workspace each runs on, how it is drawn, and what else it needs

SHELL, MEMORY = "shell", "memory"
# NDI's installer as NDI gives it out, where a copy has been kept beside the repository's own files (it is not in
# the repository): what the test of fetching NDI's library fetches, in place of the real download.
_KEPT = os.path.join(REPO, "third_party", "ndi-sdk", "download", "Install_NDI_SDK_v6_Linux.tar.gz")
NDI_INSTALLER = "file://" + (_KEPT if os.path.exists(_KEPT) else "/nonexistent/Install_NDI_SDK_v6_Linux.tar.gz")


PLAIN_SONG = """{title: Plain Song}
{artist: Nobody At All}
{key: G}

{start_of_verse}
[G]One two three [D]four
Five six [Em]seven eight
[C]Nine ten e[G/B]leven twelve
Thirteen
{end_of_verse}

{start_of_chorus}
[C]Sing it [G]out
[D]Sing it loud
{end_of_chorus}

{comment: Outro}
[G] [D/F#] [Em]
"""


def prepare_chords(workspace, test_dir):
    """Songs that have chords, which Multitracks wrote and ProPresenter kept (copies of the user's own, kept with the
    other fixtures and not in the repository), in the workspace's first library; and a ChordPro file to import."""
    libraries = os.path.join(workspace, "Libraries")
    library = os.path.join(libraries, sorted(name for name in os.listdir(libraries) if os.path.isdir(os.path.join(libraries, name)))[0])
    songs = os.path.join(FIXTURES, "songs")
    for name in sorted(os.listdir(songs)):
        shutil.copy(os.path.join(songs, name), library)
    with open(os.path.join(workspace, "Plain Song.cho"), "w") as out:
        out.write(PLAIN_SONG)


def after_chords(workspace, test_dir):
    """What the chord editors left in the song's file, and the import in the song it made: how the stretch of characters
    each chord belongs to is written, which only the file shows (the app reads back where a chord starts and no more); and
    that giving a blank slide chords changed that slide's one text box and nothing else."""
    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def song(name):
        # (Not the first library by name any more: the test made one called Hymns.)
        found = sorted(glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", name)))
        return found[0] if found else ""

    chord = re.compile(r'custom_attributes \{\s*(?:range \{\s*(?:start: (\d+)\s*)?(?:end: (\d+)\s*)?\}\s*)?chord: "((?:[^"\\]|\\.)*)"\s*\}')

    def boxes(text):
        """Every text of a decoded presentation, in the file's order: where it is, and its chords as (start, end, name)."""
        found = []
        for opening in re.finditer(r"^( *)text \{$", text, re.M):
            closing = re.compile(r"^" + opening.group(1) + r"\}$", re.M).search(text, opening.end())
            block = text[opening.start():closing.end()]
            found.append({"from": opening.start(), "to": closing.end(), "is": block,
                          "chords": [(int(m.group(1) or 0), int(m.group(2) or 0), m.group(3)) for m in chord.finditer(block)]})
        return found

    def around(text, found):
        """The file with its texts taken out: everything that a change to a text box must leave alone."""
        kept, at = [], 0
        for box in found:
            kept.append(text[at:box["from"]])
            at = box["to"]
        return "(a text)".join(kept + [text[at:]])

    def spans(box):
        return " ".join("%d-%d" % (start, end) for start, end, _ in box["chords"])

    def names(box):
        return " ".join(name for _, _, name in box["chords"])

    def font(box):
        found = re.search(r'font \{\s*name: "([^"]*)"\s*size: ([0-9.]+)', box["is"])
        return found.groups() if found else ("", "")

    # A numbered character as the app writes it in RTF, as protoc then shows it inside a string. (Made from a
    # backslash and the pieces, so that no escape is written here.)
    slash = chr(92) * 2
    def numbered(code):
        return slash + "uc0" + slash + "u%d " % code

    now, was = decoded(song("Hark2.pro")), decoded(os.path.join(FIXTURES, "songs", "Hark2.pro"))
    boxes_now, boxes_was = boxes(now), boxes(was)
    same_count = len(boxes_now) == len(boxes_was)
    changed = [i for i in range(len(boxes_now))] if not same_count else [i for i in range(len(boxes_now)) if boxes_now[i]["is"] != boxes_was[i]["is"]]
    blank = [i for i in changed if same_count and not boxes_was[i]["chords"]]
    intro = [i for i in changed if same_count and boxes_was[i]["chords"]]
    lines = [said(same_count and len(blank) == 1 and len(intro) == 1,
                  "in the song's file: two texts are other than they were, the blank slide's and the intro's", "%d of %d" % (len(changed), len(boxes_now))),
             said(same_count and around(now, boxes_now) == around(was, boxes_was),
                  "in the song's file: everything outside those two is as ProPresenter left it, byte for byte once decoded")]
    if len(blank) == 1 and len(intro) == 1:
        box, before = boxes_now[blank[0]], boxes_was[blank[0]]
        rtf = re.search(r'rtf_data: "((?:[^"\\]|\\.)*)"', box["is"])
        stand_ins = numbered(0x200B) + numbered(0x2001) + numbered(0x200B)
        lines += [
            said(names(box) == "E B/D#" and spans(box) == "0-1 2-3",
                 "in the song's file: the blank slide's two chords, each written over its own stand-in and no more", names(box) + " over " + spans(box)),
            said(rtf is not None and rtf.group(1).rstrip("}").endswith(stand_ins) and rtf.group(1).count(slash + "uc0") == 3,
                 "in the song's file: its text is the two stand-ins with the wide space between them, and nothing else"),
            said(font(box) == font(before) and font(box)[0] != "" and font(box)[0] in (rtf.group(1) if rtf else ""),
                 "in the song's file: in the font and size the text box had for what is typed into it", "%s %s, was %s %s" % (font(box) + font(before))),
        ]
        box, before = boxes_now[intro[0]], boxes_was[intro[0]]
        count = len(before["chords"])
        lines += [
            said(spans(before) == " ".join("%d-%d" % (2 * i, 2 * i + 1) for i in range(count)),
                 "in the song's file: Multitracks wrote the intro's chords one character long, each over its own stand-in", spans(before)),
            said(names(box) == names(before) + " A" and spans(box) == " ".join("%d-%d" % (2 * i, 2 * i + 1) for i in range(count + 1)),
                 "in the song's file: and so are they written here, with one more on the end", names(box) + " over " + spans(box)),
        ]
    made = boxes(decoded(song("Plain Song.pro"))) if song("Plain Song.pro") else []
    worded = [box for box in made if names(box) == "G D Em"]
    alone = [box for box in made if names(box) == "G D/F# Em"]
    lines += [
        said(len(worded) == 1 and spans(worded[0]) == "0-14 14-18 28-39",
             "in the imported song's file: a chord over words is written up to the next chord or the end of its line", " | ".join(spans(box) for box in worded)),
        said(len(alone) == 1 and spans(alone[0]) == "0-1 2-3 4-5",
             "in the imported song's file: and a chord with no words over its own stand-in only", " | ".join(spans(box) for box in alone)),
    ]
    return lines


def test(workspace, drawn=SHELL, timeout=170, env=None, prepare=None, after=None, also=(), monitors=1):
    return {"workspace": workspace, "drawn": drawn, "timeout": timeout, "env": env or {}, "prepare": prepare, "after": after, "also": also,
            "monitors": monitors}


TESTS = {
    "media": test("Demo"),
    "transport": test("Demo"),
    "timers": test("Demo"),
    "link": test("ProPresenter MR"),
    "rapid": test("Demo"),
    "chrome": test("Demo"),
    "switch": test("Demo", MEMORY, 60, also=("ProPresenter MR",)),
    "smoke": test("Demo", MEMORY, 120, {"SP_TEST_WINDOWS": "0"}),
    "edit": test("Demo", MEMORY, 120, {"SP_TEST_WINDOWS": "0", "SP_TEST_WIDTH": "1200", "SP_TEST_HEIGHT": "660"}),
    "props": test("Demo"),
    "stage": test("ProPresenter MR"),
    "actions": test("Demo", SHELL, 120, prepare=prepare_actions),
    "log": test("Demo"),
    "sound": test("Demo"),
    "drop": test("Demo"),
    "simple": test("Demo"),
    "shapes": test("Shapes", SHELL, 120),
    "slides": test("Shapes", SHELL, 120),
    "keys": test("Shapes", SHELL, 120, {"SP_TEST_REMEMBER": "1"}, lambda w, d: prepare_keys(w, d, True), after_keys),
    "keys2": test("Shapes", SHELL, 60, {"SP_TEST_REMEMBER": "1"}, lambda w, d: prepare_keys(w, d, False), after_keys2),
    "acts": test("Act", after=after_acts),
    "refine": test("Act"),
    "search": test("Demo"),
    "looks": test("Act", after=after_looks),
    "themes": test("Act"),
    "chords": test("ProPresenter MR", prepare=prepare_chords, after=after_chords),
    # (On a desktop of its own, with two displays to send screens to.)
    "screens": test("Demo", after=after_screens, monitors=2),
    # (With no library of NDI's to be found, and its installer fetched from a copy on this computer if there is one.)
    "ndisetup": test("Demo", env={"NDI_RUNTIME_DIR_V6": "", "SIMPLEPRESENTER_NDI_SDK_URL": NDI_INSTALLER}),
}


# ---- the desktop the tests are drawn on, and its audio

class Desktop:
    """A GNOME Shell with no screen, on a session bus of its own, and a private audio service beside it."""

    def __init__(self, run_dir, monitors=1):
        self.root = os.path.join(run_dir, "desktop" if monitors == 1 else "desktop%d" % monitors)
        self.monitors = monitors
        self.runtime = os.path.join(self.root, "r")
        self.display = "wayland-9"
        self.started = []

    def answering(self):
        probe = socket.socket(socket.AF_UNIX)
        try:
            probe.connect(os.path.join(self.runtime, self.display))
            return True
        except OSError:
            return False
        finally:
            probe.close()

    def spawn(self, name, command, env):
        log = open(os.path.join(self.root, name + ".log"), "wb")
        self.started.append(subprocess.Popen(command, env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                             start_new_session=True, cwd=self.root))

    def start(self):
        for part in ("r", "home", "config", "data", "cache", "state"):
            os.makedirs(os.path.join(self.root, part))
        os.chmod(self.runtime, 0o700)
        env = {"PATH": os.environ["PATH"], "HOME": os.path.join(self.root, "home"), "XDG_RUNTIME_DIR": self.runtime,
               "XDG_CONFIG_HOME": os.path.join(self.root, "config"), "XDG_DATA_HOME": os.path.join(self.root, "data"),
               "XDG_CACHE_HOME": os.path.join(self.root, "cache"), "XDG_STATE_HOME": os.path.join(self.root, "state"), "LANG": "C.UTF-8"}
        shell = dict(env, XDG_SESSION_TYPE="wayland", XDG_CURRENT_DESKTOP="GNOME")
        # The first display is an ordinary 1920 by 1080 unless SP_TEST_MONITOR asks for another size, which is for
        # taking pictures of the app at twice its size (with QT_SCALE_FACTOR=2 the windows need the room).
        monitors = ["--virtual-monitor", os.environ.get("SP_TEST_MONITOR", "1920x1080")] + ["--virtual-monitor", "1280x720"] * (self.monitors - 1)
        self.spawn("shell", ["dbus-run-session", "--", "gnome-shell", "--headless"] + monitors + ["--wayland-display", self.display, "--no-x11"], shell)
        for _ in range(60):
            if self.answering():
                break
            time.sleep(0.5)
        else:
            raise RuntimeError("the test desktop did not start: see %s" % os.path.join(self.root, "shell.log"))
        # Audio: PipeWire with a sink that plays to nowhere, and its session manager with every kind of hardware left out.
        shutil.copytree(os.path.join(HERE, "audio", "pipewire"), os.path.join(self.root, "config", "pipewire"))
        audio = dict(env, DBUS_SESSION_BUS_ADDRESS="unix:path=/nonexistent")
        self.spawn("pipewire", ["pipewire"], audio)
        for _ in range(40):
            if os.path.exists(os.path.join(self.runtime, "pipewire-0")):
                break
            time.sleep(0.25)
        self.spawn("wireplumber", ["wireplumber", "-p", "policy"], audio)
        self.spawn("pipewire-pulse", ["pipewire-pulse"], audio)
        time.sleep(2)

    def env(self):
        return {"XDG_RUNTIME_DIR": self.runtime, "WAYLAND_DISPLAY": self.display, "QT_QPA_PLATFORM": "wayland",
                "DBUS_SESSION_BUS_ADDRESS": "unix:path=/nonexistent"}

    def stop(self):
        for process in reversed(self.started):
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        deadline = time.time() + 5
        for process in self.started:
            try:
                process.wait(max(0.1, deadline - time.time()))
            except subprocess.TimeoutExpired:
                pass
        # What the desktop's helpers mounted in its runtime folder (the portal's documents, the file system
        # of network places) is let go of, or the folder cannot be cleared away afterwards.
        for mounted in ("doc", "gvfs"):
            for tool in ("fusermount3", "fusermount"):
                if shutil.which(tool):
                    subprocess.run([tool, "-u", "-z", os.path.join(self.runtime, mounted)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    break
        # Whatever the desktop started for itself is known by the runtime folder it was given, which is this run's alone.
        for _ in range(2):
            left = self.left()
            for pid in left:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if not left:
                break
            time.sleep(0.5)

    def left(self):
        mine = ("XDG_RUNTIME_DIR=" + self.runtime).encode()
        found = []
        for entry in os.listdir("/proc"):
            if not entry.isdigit() or int(entry) == os.getpid():
                continue
            try:
                with open("/proc/%s/environ" % entry, "rb") as environ:
                    if mine in environ.read().split(b"\0"):
                        found.append(int(entry))
            except OSError:
                pass
        return found


# ---- running one

def make_script(name, run_dir):
    """The test's script as the app is given it: with the shared helpers in, and the places of this run's files."""
    scripts = os.path.join(run_dir, "t")
    os.makedirs(scripts, exist_ok=True)
    if not os.path.exists(os.path.join(scripts, "lib.js")):
        shutil.copy(os.path.join(HERE, "t", "lib.js"), scripts)
    with open(os.path.join(HERE, "t", name + ".qml")) as source:
        body = source.read()
    if "//COMMON" in body:
        with open(os.path.join(HERE, "t", "common.txt")) as common:
            body = body.replace("//COMMON", common.read())
    body = body.replace("@MADE@", os.path.join(FIXTURES, "made")).replace("@WORKSPACES@", os.path.join(run_dir, name, "WorkSpaces"))
    path = os.path.join(scripts, name + ".qml")
    with open(path, "w") as out:
        out.write(body)
    return path


def run_test(name, binary, run_dir, desktop, cache):
    recipe = TESTS[name]
    if recipe["monitors"] != 1:
        # A desktop of its own, for as long as the test takes.
        own = Desktop(run_dir, recipe["monitors"])
        own.start()
        try:
            return run_on(name, binary, run_dir, own, cache)
        finally:
            own.stop()
    return run_on(name, binary, run_dir, desktop, cache)


def run_on(name, binary, run_dir, desktop, cache):
    recipe = TESTS[name]
    test_dir = os.path.join(run_dir, name)
    frames = os.path.join(run_dir, "frames", name)
    for folder in (os.path.join(test_dir, "config"), os.path.join(test_dir, "data"), os.path.join(test_dir, "docs"), frames):
        os.makedirs(folder)
    with open(os.path.join(test_dir, "config", "user-dirs.dirs"), "w") as dirs:
        dirs.write('XDG_DOCUMENTS_DIR="%s"\n' % os.path.join(test_dir, "docs"))
    workspace = copy_workspace(recipe["workspace"], test_dir)
    for other in recipe["also"]:
        copy_workspace(other, test_dir)
    if recipe["prepare"]:
        recipe["prepare"](workspace, test_dir)
    script = make_script(name, run_dir)

    env = dict(os.environ)
    env.update({"XDG_CONFIG_HOME": os.path.join(test_dir, "config"), "XDG_DATA_HOME": os.path.join(test_dir, "data"), "XDG_CACHE_HOME": cache})
    if recipe["drawn"] == SHELL:
        env.pop("DISPLAY", None)
        env.update(desktop.env())
    else:
        env.update({"QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"})
    env.update(recipe["env"])
    command = [binary, "--workspace", workspace, "--script", script, "--frames", frames]
    try:
        done = subprocess.run(command, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=recipe["timeout"])
        output = done.stdout.decode(errors="replace")
    except subprocess.TimeoutExpired as late:
        output = (late.stdout or b"").decode(errors="replace") + "\nTIMED OUT after %d seconds\n" % recipe["timeout"]
    lines = [line for line in output.splitlines() if not NOISE.search(line)]
    if recipe["after"]:
        try:
            lines += recipe["after"](workspace, test_dir)
        except Exception as error:  # a file the test should have left is not there, say
            lines.append("  FAILED  what the test left in the workspace could not be read   [%s]" % error)
    os.makedirs(os.path.join(run_dir, "logs"), exist_ok=True)
    with open(os.path.join(run_dir, "logs", name + ".log"), "w") as log:
        log.write("\n".join(lines) + "\n")

    ok = sum(1 for line in lines if line.startswith("  ok "))
    failed = [line for line in lines if FAILED.search(line)]
    errors = [line for line in lines if QML_ERROR.search(line)]
    finished = any(FINISHED.search(line) for line in lines)
    print("%-10s %3d ok, %d failed, %d QML errors%s" % (name, ok, len(failed), len(errors), "" if finished else "   DID NOT FINISH"), flush=True)
    for line in (failed + errors)[:8]:
        print(line[:300], flush=True)
    return not failed and not errors and finished


def main():
    parser = argparse.ArgumentParser(description="Runs the scripted tests of the app (see the top of this file).")
    parser.add_argument("tests", nargs="*", help="which tests to run; all of them if none is named")
    parser.add_argument("--binary", default=os.path.join(REPO, "build-tests", "SimplePresenter"),
                        help="the app, built with -DSIMPLEPRESENTER_TEST_HOOK=ON (default: build-tests/SimplePresenter)")
    parser.add_argument("--keep", action="store_true", help="keep the run's folder even if every test passes")
    parser.add_argument("--list", action="store_true", help="list the tests and stop")
    args = parser.parse_args()
    if args.list:
        print(" ".join(TESTS))
        return 0
    names = args.tests or list(TESTS)
    unknown = [name for name in names if name not in TESTS]
    if unknown:
        print("no such test: %s (there are: %s)" % (" ".join(unknown), " ".join(TESTS)), file=sys.stderr)
        return 2
    if not os.path.exists(args.binary):
        print("there is no app at %s: build it with -DSIMPLEPRESENTER_TEST_HOOK=ON, or say where it is with --binary" % args.binary, file=sys.stderr)
        return 2
    if not os.path.isdir(os.path.join(FIXTURES, "workspaces")):
        print("the workspaces the tests run on are not here (%s): see tests/README.md" % FIXTURES, file=sys.stderr)
        return 2

    run = tempfile.TemporaryDirectory(prefix="simplepresenter-tests-", ignore_cleanup_errors=True)
    run_dir = run.name
    cache = os.path.join(run_dir, "cache")
    os.makedirs(cache)
    desktop = None
    passed = False
    try:
        if any(TESTS[name]["drawn"] == SHELL and TESTS[name]["monitors"] == 1 for name in names):
            desktop = Desktop(run_dir)
            desktop.start()
        results = [run_test(name, os.path.abspath(args.binary), run_dir, desktop, cache) for name in names]
        passed = all(results)
    finally:
        if desktop:
            desktop.stop()
        if passed and not args.keep:
            run.cleanup()
        else:
            # (Not to be removed when this program ends.)
            run._finalizer.detach()
            print("the run's logs and pictures are in %s" % run_dir)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
