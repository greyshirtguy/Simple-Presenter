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
import hashlib
import json
import os
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse
import uuid
import zlib

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


# Labels kept ready as ProPresenter writes them: two of words alone, one with a colour.
LABELS = """\
labels { text: "Blank Slide" }
labels { text: "Instrumental" }
labels { text: "Offering" color { green: 0.5 alpha: 1 } }
"""


def prepare_slidemenu(workspace, test_dir):
    with open(os.path.join(workspace, "Configuration", "Labels"), "wb") as out:
        out.write(protoc("encode", "rv.data.ProLabelsDocument", "labels.proto", LABELS.encode()))


def after_slidemenu(workspace, test_dir):
    """Slides put in other groups from their menu: what the song's file holds. The test kept the file as it was before
    (song-before) and after the first change (song-split); which song and which groups is read from those."""
    def decoded(path, message="rv.data.Presentation", proto="presentation.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def groups(text):
        """Each group of a decoded presentation, in order: its own id, its name, the id of the list's group, its slides."""
        found = []
        for block in re.findall(r"^cue_groups \{\n(.*?)^\}", text, re.S | re.M):
            group = re.search(r"^  group \{\n(.*?)^  \}", block, re.S | re.M).group(1)
            name = re.search(r'^    name: "((?:[^"\\]|\\.)*)"', group, re.M)
            listed = re.search(r'application_group_identifier \{\s*string: "([^"]+)"', group)
            found.append({"id": re.search(r'^    uuid \{\s*string: "([^"]+)"', group, re.M).group(1), "name": name.group(1) if name else "",
                          "list": listed.group(1) if listed else "", "hotkey": "hotKey {" in group, "color": "color {" in group,
                          "cues": re.findall(r'cue_identifiers \{\s*string: "([^"]+)"', block)})
        return found

    def arrangements(text):
        return [re.findall(r'group_identifiers \{\s*string: "([^"]+)"', block) for block in re.findall(r"^arrangements \{\n(.*?)^\}", text, re.S | re.M)]

    def rest(text):
        """A decoded presentation without its groups and its arrangements."""
        return re.sub(r"^(cue_groups|arrangements) \{\n.*?^\}\n", "", text, flags=re.S | re.M)

    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))
    before, split = (decoded(os.path.join(frames, name)) for name in ("song-before", "song-split"))
    # Two files were changed: the song whose groups were, and the one a slide of which was labelled, each known by
    # being, in the fixture, what the test kept of it before.
    def fixture(path):
        with open(os.path.join(FIXTURES, "workspaces", "Act", "Libraries", "MainHall", os.path.basename(path)), "rb") as source:
            return source.read()

    def kept(name):
        with open(os.path.join(frames, name), "rb") as source:
            return source.read()

    def now_bytes(path):
        with open(path, "rb") as source:
            return source.read()

    everything = sorted(glob.glob(os.path.join(glob.escape(workspace), "Libraries", "MainHall", "*.pro")))
    differing_files = [path for path in everything if now_bytes(path) != fixture(path)]
    changed = [path for path in differing_files if fixture(path) == kept("song-before")]
    labelled = [path for path in differing_files if fixture(path) == kept("label-before")]
    after = decoded(changed[0]) if len(changed) == 1 else ""
    listed = {name: uuid for uuid, name in re.findall(r'^groups \{\s*uuid \{\s*string: "([^"]+)"\s*\}\s*name: "([^"]*)"',
                                                    decoded(os.path.join(workspace, "Configuration", "Groups"), "rv.data.ProGroupsDocument", "groups.proto"), re.M)}

    was, cut, now = groups(before), groups(split), groups(after)
    new = [group for group in cut if group["id"] not in [g["id"] for g in was]]
    old = [g for g in was if new and g["cues"][-len(new[0]["cues"]):] == new[0]["cues"] and len(g["cues"]) > len(new[0]["cues"])] if new else []
    lines = [
        said(len(changed) == 1 and len(labelled) == 1 and len(differing_files) == 2 and bool(after),
             "two presentations' files were changed, the one whose groups were and the one with the labelled slide, and no other", "%d files" % len(differing_files)),
        said(len(cut) == len(was) + 1 and len(new) == 1 and len(old) == 1 and cut.index(new[0]) == was.index(old[0]) + 1
             and [g for g in cut if g["id"] == old[0]["id"]][0]["cues"] == old[0]["cues"][:-len(new[0]["cues"])],
             "in the song's file: the group was split, its later slides in a new group straight after it and its earlier ones where they were",
             "%d groups, then %d" % (len(was), len(cut))),
    ]
    if len(new) == 1 and len(old) == 1:
        made = new[0]
        lines += [
            said(made["name"] in listed and made["list"] == listed[made["name"]] and made["hotkey"] and made["color"] and re.fullmatch(r"[0-9A-F-]{36}", made["id"]) is not None,
                 "in the song's file: the new group is written as ProPresenter writes one given from its list: an id of its own, the name, a colour, an empty hotkey, "
                 "and the id of the list's group", "%s, from the list's %s" % (made["name"], made["list"])),
            said(all(now_one == [x for group in was_one for x in ([group, made["id"]] if group == old[0]["id"] else [group])]
                     for was_one, now_one in zip(arrangements(before), arrangements(split))) and len(arrangements(before)) > 0
                 and any(old[0]["id"] in one for one in arrangements(before)),
                 "in the song's file: every arrangement has the new group after the old one wherever that comes up, and is otherwise as it was",
                 "%d arrangements" % len(arrangements(before))),
            said(rest(before) == rest(split), "in the song's file: nothing but its groups and its arrangements is other than it was"),
        ]
    whole = [(a, b) for a, b in zip(cut, now) if a != b]
    lines += [
        said(len(now) == len(cut) and len(whole) == 1 and whole[0][0]["id"] == whole[0][1]["id"] and whole[0][0]["cues"] == whole[0][1]["cues"]
             and whole[0][1]["name"] in listed and whole[0][1]["list"] == listed[whole[0][1]["name"]] and whole[0][1]["name"] != whole[0][0]["name"],
             "in the song's file: the group changed from its first slide is the group it was, by its id and its slides, with the new name and the list's id for it",
             "%d groups differ" % len(whole)),
        said(arrangements(split) == arrangements(after) and rest(split) == rest(after),
             "in the song's file: and nothing else is other than it was, the arrangements among it"),
    ]

    # The labelled slide: one label more than the file had, written as ProPresenter writes one with a colour.
    if len(labelled) == 1:
        was_text, now_text = decoded(os.path.join(frames, "label-before")), decoded(labelled[0])
        added = [line.strip() for line in now_text.splitlines()]
        gone = was_text.splitlines()
        extra = len(now_text.splitlines()) - len(gone)
        block = re.search(r'^ *label \{\n *text: "Offering"\n *color \{\n *green: 0\.50196081\d*\n *alpha: 1\n *\}\n *\}\n', now_text, re.M)
        without = now_text.replace(block.group(0), "", 1) if block else ""
        lines += [
            said(block is not None and now_text.count('text: "Offering"') == 1,
                 "in the labelled slide's file: the label is on the slide's action, its words and its colour (a green, with nothing written for the red and the blue it has none of)"),
            said(bool(block) and without == was_text, "in the labelled slide's file: and nothing else is other than it was", "%d lines more" % extra),
        ]
    labels = decoded(os.path.join(workspace, "Configuration", "Labels"), "rv.data.ProLabelsDocument", "labels.proto")
    lines.append(said(re.fullmatch(r'labels \{ text: "Blank" \} labels \{ text: "Offering" color \{ green: 0\.5 alpha: 1 \} \} '
                                   r'labels \{ text: "Welcome" color \{ red: 0\.8980\d* green: 0\.2235\d* blue: 0\.2078\d* alpha: 1 \} \}',
                                   re.sub(r"\s+", " ", labels).strip()) is not None,
                      "in the workspace's list of labels: the one changed, the one that was left (as it was written), and the new one with its colour; the one removed is gone",
                      re.sub(r"\s+", " ", labels).strip()[:260]))
    return lines


# Clear groups as ProPresenter writes them: its own, as the one file of its own that was looked at has it (the id
# apart, which is made up); one of two layers, with a picture and a tint; one that is kept out of sight; and one with a
# picture of the user's own, which is put in when the file is made (%s).
CLEAR_GROUPS = """\
application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER application_version { major_version: 20 } }
groups {
  uuid { string: "C1EA0000-0000-4000-8000-000000000001" }
  name: "Clear All"
  layer_targets { target_layer: CLEAR_TARGET_LAYER_AUDIO }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_AUDIO_EFFECTS }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_MESSAGES }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_PROP }
  layer_targets { content_destination: CONTENT_DESTINATION_ANNOUNCEMENTS }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_SLIDE }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_BACKGROUND }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_LIVE_VIDEO }
  image_type: ImageTypeAll
  icon_tint_color { red: 1 green: 1 blue: 1 alpha: 1 }
  timeline_targets: CONTENT_DESTINATION_GLOBAL
  timeline_targets: CONTENT_DESTINATION_ANNOUNCEMENTS
  clear_presentation_next_slide: true
}
groups {
  uuid { string: "C1EA0000-0000-4000-8000-000000000002" }
  name: "Words"
  layer_targets { target_layer: CLEAR_TARGET_LAYER_MESSAGES }
  layer_targets { target_layer: CLEAR_TARGET_LAYER_SLIDE }
  image_type: ImageTypeBell
  is_icon_tinted: true
  icon_tint_color { red: 0.11764706 green: 0.53333336 blue: 0.8980392 alpha: 1 }
}
groups {
  uuid { string: "C1EA0000-0000-4000-8000-000000000003" }
  name: "Out Of Sight"
  layer_targets { target_layer: CLEAR_TARGET_LAYER_PROP }
  is_hidden_in_preview: true
  image_type: ImageTypeStar
}
groups {
  uuid { string: "C1EA0000-0000-4000-8000-000000000004" }
  name: "Own"
  layer_targets { target_layer: CLEAR_TARGET_LAYER_BACKGROUND }
  image_data: "%s"
}
"""


def tiny_png(rgb, size=8, height=None):
    """A small picture of one colour, as the bytes of a PNG file: square, or `size` wide and `height` high."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    height = height or size
    row = b"\x00" + bytes(rgb) * size
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(row * height)) + chunk(b"IEND", b""))


def prepare_cleargroups(workspace, test_dir):
    picture = "".join("\\%03o" % byte for byte in tiny_png((255, 0, 255)))
    path = os.path.join(workspace, "Configuration", "ClearGroups")
    with open(path, "wb") as out:
        out.write(protoc("encode", "rv.data.ClearGroupsDocument", "clearGroups.proto", (CLEAR_GROUPS % picture).encode()))
    shutil.copy(path, os.path.join(test_dir, "cleargroups-before"))


def after_cleargroups(workspace, test_dir):
    """Clear groups changed in the window they are set up in, and a group's button dragged onto a slide: what the
    workspace's file of them and the song's file hold. The file of the groups is set against what the test was given
    (prepare_cleargroups), group by group; the song against what the test kept of it before the drag (song-before)."""
    def decoded(path, message="rv.data.ClearGroupsDocument", proto="clearGroups.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def groups(text):
        """The groups of a decoded file, each as its lines of the text, by its name."""
        return {re.search(r'^  name: "([^"]*)"', block, re.M).group(1): block for block in re.findall(r"^groups \{\n.*?^\}\n", text, re.S | re.M)}

    def without_tint(block):
        return re.sub(r"^  icon_tint_color \{\n.*?^  \}\n", "", block, flags=re.S | re.M)

    was = decoded(os.path.join(test_dir, "cleargroups-before"))
    now = decoded(os.path.join(workspace, "Configuration", "ClearGroups"))
    before, after = groups(was), groups(now)
    order = re.findall(r'^  name: "([^"]*)"', now, re.M)
    made = after.get("Offering", "")
    words = after.get("Words", "")
    tint = re.search(r"^  icon_tint_color \{\n    red: (\S+)\n    green: (\S+)\n    blue: (\S+)\n    alpha: 1\n  \}\n", words, re.M)
    new_group = re.fullmatch(r'groups \{\n  uuid \{\n    string: "([0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12})"\n  \}\n  name: "Offering"\n'
                             r'  layer_targets \{\n    target_layer: CLEAR_TARGET_LAYER_PROP\n  \}\n  image_type: ImageTypeOne\n'
                             r'  icon_tint_color \{\n    red: 1\n    green: 1\n    blue: 1\n    alpha: 1\n  \}\n\}\n', made)
    lines = [
        said(order == ["Clear All", "Words", "Out Of Sight", "Offering", "Own"],
             "in the clear groups file: the groups in the order they were left in, the new one moved up one, and the one deleted not among them", ", ".join(order)),
        said(was[:was.index("groups {")] == now[:now.index("groups {")] and "PLATFORM_MACOS" in now[:now.index("groups {")],
             "in the clear groups file: what the file says of where it came from is as it was"),
        said(before["Clear All"] == after.get("Clear All") and "clear_presentation_next_slide: true" in before["Clear All"] and before["Clear All"].count("timeline_targets") == 2,
             "in the clear groups file: ProPresenter's own group is as it was, to the letter, with what this app has nothing for still in it"),
        said(before["Own"] == after.get("Own") and "image_data:" in before["Own"],
             "in the clear groups file: so is the group with a picture of the user's own, the picture with it"),
        said(before["Out Of Sight"] == after.get("Out Of Sight") and "is_hidden_in_preview: true" in before["Out Of Sight"],
             "in the clear groups file: and the group kept out of sight, which was shown and hidden again"),
        said(without_tint(words) == without_tint(before["Words"]).replace("ImageTypeBell", "ImageTypeStar") and "is_icon_tinted: true" in words,
             "in the clear groups file: the group that was changed has the star for its picture, its tint switched on as it was, the layers it had "
             "(one was added and taken out again), and nothing else other than it was"),
        said(tint is not None and [round(float(part) * 255) for part in tint.groups()] == [0x43, 0xa0, 0x47],
             "in the clear groups file: and its tint is the colour that was chosen", tint.group(0).replace("\n", " ") if tint else "no tint"),
        said(new_group is not None, "in the clear groups file: the new group has an id of its own, its name, the one layer it was given, the first of the digits "
             "for its picture, and the white tint, not switched on, that ProPresenter's own has", made.replace("\n", " ")[:300]),
    ]

    # The song: one action more than it had, as ProPresenter writes one that clears a group, naming the group by its id.
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))
    song = os.path.join(workspace, "Libraries", "MainHall", "When Wind Meets Fire.pro")
    song_was = decoded(os.path.join(frames, "song-before"), "rv.data.Presentation", "presentation.proto")
    song_now = decoded(song, "rv.data.Presentation", "presentation.proto")
    action = re.search(r'^( *)actions \{\n\1  uuid \{\n\1    string: "[0-9A-F-]{36}"\n\1  \}\n\1  isEnabled: true\n\1  type: ACTION_TYPE_CLEAR_GROUP\n'
                       r'\1  clear_group \{\n\1    identification \{\n\1      parameter_uuid \{\n\1        string: "%s"\n\1      \}\n'
                       r'\1      parameter_name: "Offering"\n\1    \}\n\1  \}\n\1\}\n' % (new_group.group(1) if new_group else "no such group"), song_now, re.M)
    lines += [
        said(action is not None and song_now.count("ACTION_TYPE_CLEAR_GROUP") == song_was.count("ACTION_TYPE_CLEAR_GROUP") + 1,
             "in the song's file: the slide has the action that clears the group, written as ProPresenter writes one: enabled, of that kind, "
             "and naming the group by its id in the workspace's list and its name"),
        said(action is not None and song_now.replace(action.group(0), "", 1) == song_was, "in the song's file: and nothing else is other than it was",
             "%d lines, then %d" % (len(song_was.splitlines()), len(song_now.splitlines()))),
    ]
    return lines


def write_settings(test_dir, lines):
    """The app's own settings, as it would have left them: what a setting that is acted on as the app starts is read from."""
    folder = os.path.join(test_dir, "config", "SimplePresenter")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "SimplePresenter.conf"), "w") as conf:
        conf.write("[General]\n" + "".join(line + "\n" for line in lines))


def kept_settings(test_dir, name):
    with open(os.path.join(test_dir, "config", "SimplePresenter", "SimplePresenter.conf")) as conf:
        return re.findall(r"^%s=(.*)$" % re.escape(name), conf.read(), re.M)


def prepare_softdecode(workspace, test_dir):
    """How video is decoded is settled as the app starts, from what was saved before, so the test has to find the
    setting saved already: here, that the processor alone is to decode."""
    write_settings(test_dir, ["videoDecoding=processor"])


def after_softdecode(workspace, test_dir):
    saved = kept_settings(test_dir, "videoDecoding")
    return [said(saved == ["auto"], "in the app's settings: what was chosen last is what is kept, for the next start", ", ".join(saved) or "nothing kept")]


def nouveau_system(test_dir):
    """A system made up in the test's folder, for the guard on hardware video decoding to look at in place of the real
    one: laid out as /dev, /sys and /lib/firmware are, with one graphics chip, run by nouveau, and none of the
    firmware its video decoder needs. (The app is told of it, and of the chip's number, in its environment: see the
    recipes. Only what the guard looks at is made up; video is still decoded by what this computer has.)"""
    system = os.path.join(test_dir, "system")
    driver = os.path.join(system, "sys", "bus", "pci", "drivers", "nouveau")
    device = os.path.join(system, "sys", "class", "drm", "renderD128", "device")
    for folder in (driver, device, os.path.join(system, "dev", "dri"), os.path.join(system, "lib", "firmware", "nouveau")):
        os.makedirs(folder)
    os.symlink(driver, os.path.join(device, "driver"))
    with open(os.path.join(system, "dev", "dri", "renderD128"), "w"):
        pass


def prepare_decodeguard(workspace, test_dir):
    nouveau_system(test_dir)


def after_decodeguard(workspace, test_dir):
    return [
        said(kept_settings(test_dir, "decodingNoticeSeen") == ["true"], "in the app's settings: that the notice has been shown is kept, so that it is shown once",
             ", ".join(kept_settings(test_dir, "decodingNoticeSeen")) or "nothing kept"),
        said(kept_settings(test_dir, "videoDecoding") == ["hardware"], "in the app's settings: and the choice to have the graphics chip tried all the same",
             ", ".join(kept_settings(test_dir, "videoDecoding")) or "nothing kept"),
    ]


def prepare_decodeguard2(workspace, test_dir):
    """The same computer at its next start: the notice has been shown, and the graphics chip was chosen all the same."""
    nouveau_system(test_dir)
    write_settings(test_dir, ["decodingNoticeSeen=true", "videoDecoding=hardware"])


def after_chords3(workspace, test_dir):
    """What Revert kept: before it put the song back as it was opened, the song as it then was, as a copy among the
    copies the editor keeps of a file before it is changed (which the app has no window for: they are files in a folder
    of its own)."""
    # (Which the test says in the app's own log of the run, as a sum of the file's bytes.)
    left = []
    for path in glob.glob(os.path.join(glob.escape(test_dir), "**", "Logs", "*"), recursive=True):
        with open(path, errors="replace") as log:
            left += re.findall(r"the song was left as ([0-9a-f]{40})", log.read())
    copies = sorted(glob.glob(os.path.join(glob.escape(test_dir), "data", "**", "Edit Backups", "**", "Hark2 *.pro"), recursive=True))
    sums = []
    for copy in copies:
        with open(copy, "rb") as source:
            sums.append(hashlib.sha1(source.read()).hexdigest())
    with open(os.path.join(FIXTURES, "songs", "Hark2.pro"), "rb") as source:
        as_it_came = hashlib.sha1(source.read()).hexdigest()
    return [said(len(left) == 1 and left[0] in sums, "among the editor's copies: the song as it was just before it was reverted, byte for byte",
                 "%d copies" % len(copies)),
            said(as_it_came in sums, "among the editor's copies: and the song as it was before its first change")]


def prepare_library(workspace, test_dir):
    """The rows of the playlists as ProPresenter writes them on a Mac: the address of the file there, the system named,
    and the path from the workspace's folder. The rows the fixture has were added on this computer and give the file's
    address here, in the workspace the fixture was copied from: a row whose file the test has deleted or renamed would
    then be found there, in somebody's real workspace, which a test is to have nothing to do with."""
    path = os.path.join(workspace, "Playlists", "Library")
    with open(path, "rb") as source:
        text = protoc("decode", "rv.data.PlaylistDocument", "propresenter.proto", source.read()).decode()
    text, rows = re.subn(r'^( *)absolute_string: "file://[^"]*/(Libraries/[^"/]+/[^"/]+\.pro)"\n(?! *platform:)',
                         r'\1absolute_string: "file:///Users/someone/Documents/ProPresenter/\2"\n\1platform: PLATFORM_MACOS\n', text, flags=re.M)
    if rows == 0:
        raise RuntimeError("the fixture's playlists have no rows that are presentations")
    with open(path, "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", text.encode()))
    shutil.copy(path, os.path.join(test_dir, "playlists-before"))


def after_library(workspace, test_dir):
    """A presentation renamed, from a library and from a playlist, and one deleted: what the files then hold. A renamed
    file is the file it was but for the name inside it; the playlists are what they were but for the name and the
    file of the rows that are those presentations; and the deleted one is in the Trash, whole. (It is taken out of the
    Trash again here: the Trash it went to is the system's for the disk the test runs on, not a folder of the test's.)"""
    def decoded(path, message="rv.data.Presentation", proto="presentation.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def differing(was, now):
        was, now = was.splitlines(), now.splitlines()
        return None if len(was) != len(now) else [(a.strip(), b.strip()) for a, b in zip(was, now) if a != b]

    original = os.path.join(FIXTURES, "workspaces", "Demo")
    library = os.path.join(workspace, "Libraries", "Demo")
    lines = []

    renamed = os.path.join(library, "Build My Life Again.pro")
    changed = differing(decoded(os.path.join(original, "Libraries", "Demo", "Build My Life.pro")), decoded(renamed)) if os.path.exists(renamed) else None
    lines.append(said(changed == [('name: "Build My Life"', 'name: "Build My Life Again"')],
                      "in the renamed presentation's file: the name inside is the new one, and nothing else is other than it was",
                      "no such file, or another number of lines" if changed is None else "%d lines differ: %s" % (len(changed), changed[:3])))

    second = sorted(name for name in os.listdir(library) if name.endswith(" - Two.pro"))
    first_name = second[0][:-len(" - Two.pro")] if second else ""
    changed = (differing(decoded(os.path.join(original, "Libraries", "Demo", first_name + ".pro")), decoded(os.path.join(library, second[0])))
               if len(second) == 1 and os.path.exists(os.path.join(original, "Libraries", "Demo", first_name + ".pro")) else None)
    lines.append(said(changed == [('name: "%s"' % first_name, 'name: "%s - Two"' % first_name)],
                      "in the file of the one renamed from a playlist: the same, with the slash that was typed a dash",
                      "no such file, or another number of lines" if changed is None else "%d lines differ: %s" % (len(changed), changed[:3])))

    # The playlists, against what the test was given (prepare_library): every row that is one of the two presentations,
    # which may be more than two rows, and nothing else.
    lists = [decoded(path, "rv.data.PlaylistDocument", "propresenter.proto")
             for path in (os.path.join(test_dir, "playlists-before"), os.path.join(workspace, "Playlists", "Library"))]
    changed = differing(*lists)
    kinds = sorted(set(a.split(":")[0] for a, b in changed)) if changed else []
    new_names = ("Build My Life Again", first_name + " - Two")
    names = [b for a, b in changed if a.startswith("name:")] if changed else []
    addresses = [b for a, b in changed if a.startswith("absolute_string:")] if changed else []
    paths = [b for a, b in changed if a.startswith("path:")] if changed else []
    mac = "file:///Users/someone/Documents/ProPresenter/Libraries/Demo/"
    lines += [
        said(changed is not None and len(changed) > 0 and all(a.split(":")[0] == b.split(":")[0] and a.split(":")[0] in ("name", "absolute_string", "path")
                                                              for a, b in changed),
             "in the playlists file: nothing is other than it was but the names of rows and the files they refer to",
             "another number of lines" if changed is None else "%d lines differ, of the kinds %s" % (len(changed), ", ".join(kinds))),
        said(len(names) >= 2 and sorted(set(names)) == sorted('name: "%s"' % name for name in new_names),
             "in the playlists file: every row that is one of the renamed presentations is called what that is called now",
             "; ".join(names)),
        said(len(addresses) == len(names) and sorted(set(addresses)) == sorted('absolute_string: "%s%s.pro"' % (mac, urllib.parse.quote(name)) for name in new_names),
             "in the playlists file: each gives the file's new name in its address, written as an address is, in the folder on the Mac that it gave before",
             "; ".join(sorted(set(addresses)))[:260]),
        said(len(paths) == len(names) and sorted(set(paths)) == sorted('path: "Libraries/Demo/%s.pro"' % name for name in new_names),
             "in the playlists file: and in its path from the workspace's folder", "; ".join(sorted(set(paths)))[:200]),
        said(lists[0].count("platform: PLATFORM_MACOS") == lists[1].count("platform: PLATFORM_MACOS") and lists[0].count("platform: PLATFORM_MACOS") >= len(names)
             and lists[0].count("root: ROOT_SHOW") == lists[1].count("root: ROOT_SHOW"),
             "in the playlists file: what ProPresenter writes beside an address (the system it was on, and where the path starts from) is all still there",
             "%d and %d" % (lists[0].count("platform: PLATFORM_MACOS"), lists[1].count("platform: PLATFORM_MACOS"))),
    ]

    # The deleted one: named in the app's log by where it went.
    went = ""
    for log in glob.glob(os.path.join(glob.escape(test_dir), "docs", "SimplePresenter", "Logs", "*.log")):
        with open(log, encoding="utf-8", errors="replace") as text:
            for line in text:
                found = re.search(r"  trash +(.*) moved to the Trash, as (.*)$", line.rstrip("\n"))
                if found:
                    went = found.group(2)
                    gone_from = found.group(1)
    victim = os.path.join(original, "Libraries", "Demo", os.path.basename(gone_from)) if went else ""
    whole = False
    if went and os.path.isfile(went) and os.path.isfile(victim):
        with open(went, "rb") as a, open(victim, "rb") as b:
            whole = a.read() == b.read()
    lines += [
        said(bool(went) and not os.path.exists(gone_from) and gone_from.startswith(library + os.sep), "the deleted presentation's file is out of its library", went),
        said(whole and os.path.basename(os.path.dirname(went)) == "files" and "Trash" in os.path.basename(os.path.dirname(os.path.dirname(went))),
             "and in the Trash, byte for byte the file it was", went),
    ]
    # Taken out of the Trash again: that one file, known to be the test's own copy by its bytes, and the note the
    # Trash keeps beside it, known by naming where the file came from.
    if whole and os.path.basename(os.path.dirname(went)) == "files":
        note = os.path.join(os.path.dirname(os.path.dirname(went)), "info", os.path.basename(went) + ".trashinfo")
        os.remove(went)
        if os.path.isfile(note):
            with open(note, encoding="utf-8", errors="replace") as text:
                names_it = urllib.parse.quote(gone_from) in text.read()
            if names_it:
                os.remove(note)
    return lines


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
        # The one more is written where the line stands in the text box's RTF: Multitracks' own RTF to the byte, with
        # a wide space and a stand-in put in after the line's last character, before the bracket that ends it. And
        # nothing else of the text box is written, but that what it says of a range of its words (that they are in
        # capitals) has its range go with the words: one that ended with the line, two characters longer now, still does.
        rtf_now, rtf_was = (re.search(r'rtf_data: "((?:[^"\\]|\\.)*)"', each["is"]) for each in (box, before))
        added = rtf_now.group(1)[len(rtf_was.group(1)) - 1:-1] if rtf_now and rtf_was else ""
        ranged = re.compile(r"custom_attributes \{\s*(?:range \{\s*(?:start: (\d+)\s*)?(?:end: (\d+)\s*)?\}\s*)?(?!chord:)((?:[^{}]|\{[^{}]*\})*?)\s*\}")

        def beside(each):
            """A text box without its words, its chords and the ranges of what else it says of some of its words:
            what a change to those must leave alone."""
            return re.sub(r"\s+", " ", ranged.sub(lambda m: "(of a range) " + m.group(3), chord.sub("", re.sub(r'rtf_data: "((?:[^"\\]|\\.)*)"', "(the words)", each["is"]))))

        def ranges(each):
            return [(int(m.group(1) or 0), int(m.group(2) or 0)) for m in ranged.finditer(chord.sub("", each["is"]))]

        # (The intro is the text box's one line: 2 * count - 1 characters before, two more now.)
        length = 2 * count - 1
        lines += [
            said(rtf_now is not None and rtf_was is not None and rtf_was.group(1).endswith("}")
                 and rtf_now.group(1).startswith(rtf_was.group(1)[:-1]) and rtf_now.group(1).endswith("}")
                 and added in (numbered(0x2001) + numbered(0x200B), numbered(0x2001) + " "),
                 "in the song's file: the intro's RTF is Multitracks' own to the byte, with the one stand-in more put in after the line's last",
                 "put in: %s" % added),
            said(beside(box) == beside(before),
                 "in the song's file: and nothing else of that text box is written: what it keeps beside its words is as it was"),
            said(len(ranges(before)) > 0 and ranges(box) == [(a if a < length else a + 2, b if b < length else b + 2) for a, b in ranges(before)]
                 and any(b == length for _, b in ranges(before)),
                 "in the song's file: but that what it says of all its words (that they are in capitals) is said of all of them still",
                 "%s, was %s" % (ranges(box), ranges(before))),
        ]
    # The presentation that was made new, and the one made after it by the same name
    new, second = (decoded(song(name)) if song(name) else "" for name in ("Sunday Notes.pro", "Sunday Notes 2.pro"))
    top = sorted(set(re.findall(r"^([a-z_A-Z]+)(?: \{|:)", new, re.M)))
    group = re.search(r"^cue_groups \{\n(.*?)^\}", new, re.S | re.M)
    cue_ids = re.findall(r'^cues \{\n  uuid \{\n    string: "([^"]+)"', new, re.M)
    lines += [
        said(top == ["application_info", "background", "ccli", "chord_chart", "cue_groups", "cues", "name", "timeline", "uuid"]
             and 'name: "Sunday Notes"' in new and re.search(r"^timeline \{\n  duration: 300\n\}", new, re.M) is not None,
             "in the new presentation's file: what every presentation of ProPresenter's starts with, and its name", " ".join(top)),
        said(len(cue_ids) == 1 and new.count("\ncues {") + new.startswith("cues {") == 1 and "elements {" not in new and "base_slide {" in new,
             "in the new presentation's file: one slide, with nothing on it", "%d cues" % len(cue_ids)),
        said(group is not None and len(re.findall(r"^cue_groups \{", new, re.M)) == 1 and "name:" not in group.group(1) and "hotKey {" in group.group(1)
             and len(cue_ids) == 1 and ('string: "%s"' % cue_ids[0]) in group.group(1).split("cue_identifiers {")[-1],
             "in the new presentation's file: the slide is in one group with an id and no name, as ProPresenter keeps a slide that is in none"),
        said(re.search(r"^\s*\d+(?::| \{)", new, re.M) is None,
             "in the new presentation's file: nothing the descriptions of ProPresenter's files have no name for"),
        said('name: "Sunday Notes 2"' in second and re.search(r'^uuid \{\n  string: "([^"]+)"', second, re.M) is not None
             and re.search(r'^uuid \{\n  string: "([^"]+)"', second, re.M).group(1) != re.search(r'^uuid \{\n  string: "([^"]+)"', new, re.M).group(1),
             "in the second one's file: its own name and an id of its own"),
    ]
    imported = decoded(song("Plain Song.pro")) if song("Plain Song.pro") else ""
    lines.append(said(re.search(r"^timeline \{\n  duration: 300\n\}", imported, re.M) is not None and re.search(r"^ccli \{", imported, re.M) is not None,
                      "in the imported song's file: the same beginning as a new presentation's"))
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


# ---- the audio bin

# A row of the audio bin's file as ProPresenter writes one (version 21.4 on a Mac: the one file of its own that was
# looked at), field for field, with made-up names. Which outputs each channel goes to was the user's own doing in
# that file, and is here as something a change must leave alone.
AUDIO_ROW = """\
        items {
          uuid { string: "%(id)s-ITEM" }
          name: "%(name)s"
          cue {
            uuid { string: "%(id)s-CUE" }
            name: "%(name)s"
            %(target)s
            completion_action_type: COMPLETION_ACTION_TYPE_LAST
            hot_key { }
            actions {
              uuid { string: "%(id)s-ACTION" }
              isEnabled: true
              duration: %(seconds)s
              type: ACTION_TYPE_MEDIA
              media {
                element {
                  uuid { string: "%(id)s-MEDIA" }
                  url { absolute_string: "%(address)s"
                        platform: PLATFORM_MACOS local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } }
                  metadata { artist: "unknown" format: "wavfloat" color_format: COLOR_FORMAT_SDR }
                  audio {
                    audio { volume: %(volume)s audio_channels { outputs { channel_index: 2 } } audio_channels { index: 1 outputs { channel_index: 3 } }
                            is_custom_mapping: true }
                    transport { play_rate: 1 out_point: %(seconds)s should_fade_in: true should_fade_out: true end_point: %(seconds)s
                                %(loop)s times_to_loop: 1 retrigger: RETRIGGER_SETTING_AUTOMATIC }
                    file { local_url { absolute_string: "%(address)s"
                                       platform: PLATFORM_MACOS local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } } }
                  }
                }
                audio { %(type)s }
              }
            }
            isEnabled: true
          }
        }
"""

AUDIO_BIN = """\
application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER application_version { major_version: 21 minor_version: 4 } }
type: TYPE_AUDIO
root_node {
  uuid { string: "0A0A0A0A-0000-0000-0000-000000000000" }
  name: "PLAYLIST"
  expanded: true
  playlists {
    playlists {
      uuid { string: "0A0A0A0A-0000-0000-0000-00000000F01D" }
      name: "Services"
      expanded: true
      playlists {
        playlists {
          uuid { string: "0A0A0A0A-0000-0000-0000-000000000001" }
          name: "Before the Service"
          items {
%(before)s
          }
        }
        playlists {
          uuid { string: "0A0A0A0A-0000-0000-0000-000000000002" }
          name: "Effects"
          items {
%(effects)s
          }
        }
      }
    }
    playlists {
      uuid { string: "0A0A0A0A-0000-0000-0000-000000000003" }
      name: "Nothing Yet"
      items { }
    }
  }
}
"""

# The sound files the test plays: what each is called, how long it is, how high its note is, and how loud it is on
# the left and on the right, where 1 is as loud as a file can say.
AUDIO_TONES = {
    "Walk In.wav": (3.0, 440, 0.5, 0.5),
    "Second Tune.wav": (3.0, 330, 0.25, 0.25),
    "Last Tune.wav": (2.0, 392, 0.5, 0.5),
    "Round.wav": (1.2, 523, 0.5, 0.5),
    "Bell.wav": (2.0, 880, 0.5, 0.0),
    "Thunder.wav": (4.0, 110, 0.5, 0.5),
}


def write_tone(path, seconds, pitch, left, right, test_dir):
    """A sound file of one steady note: sixteen bits, two channels, 44,100 samples a second. It is written only if
    where it would really be is inside the test's own folder: see prepare_audio."""
    import math
    import wave
    if not os.path.realpath(path).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("a test's sound file was about to be written outside the test's folder: %s" % os.path.realpath(path))
    rate = 44100
    frames = bytearray()
    for n in range(int(seconds * rate)):
        wave_at = math.sin(2 * math.pi * pitch * n / rate)
        frames += struct.pack("<hh", int(32767 * left * wave_at), int(32767 * right * wave_at))
    with wave.open(path, "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(bytes(frames))


def audio_row(key, name, file, seconds, sounds, target=True, loop=False, effect=False, volume=1):
    return AUDIO_ROW % {"id": "0B0B0B0B-0000-0000-0000-%012d" % key, "name": name, "file": file,
                        "address": "file://" + urllib.parse.quote(os.path.join(sounds, file)), "seconds": seconds, "volume": volume,
                        "target": "completion_target_type: COMPLETION_TARGET_TYPE_NEXT" if target else "",
                        "loop": "playback_behavior: PLAYBACK_BEHAVIOR_LOOP" if loop else "",
                        "type": "audio_type: MEDIA_ACTION_AUDIO_TYPE_SOUND" if effect else ""}


def prepare_audio(workspace, test_dir):
    """An audio bin as ProPresenter might have left one: a folder of two playlists and a playlist with nothing in it,
    one of their sound files not there; and two more sound files, for the test to add.

    NOTHING IS WRITTEN UNDER THE WORKSPACE'S Media. In a copy of a fixture that folder is a link to the Media of the
    workspace the fixture was made from, which is a real one: a file written "into the copy" there is written into
    somebody's real media folder. (It was, once, the first time this was run.) So the sound files are in the test's
    own folder, and each row of the file names its sound file by its whole address there; the place beside it that a
    row has for where the file is in the workspace is as ProPresenter writes it, and names a file that is not there,
    which is how a workspace that was copied without its media looks. write_tone refuses any other place."""
    sounds = os.path.join(test_dir, "sounds")
    os.makedirs(sounds)
    for name, (seconds, pitch, left, right) in AUDIO_TONES.items():
        write_tone(os.path.join(sounds, name), seconds, pitch, left, right, test_dir)
    write_tone(os.path.join(sounds, "Extra Tune.wav"), 2.5, 660, 0.5, 0.5, test_dir)
    write_tone(os.path.join(sounds, "Another.wav"), 1.0, 700, 0.5, 0.5, test_dir)
    with open(os.path.join(sounds, "notes.txt"), "w") as out:
        out.write("not a sound file\n")
    before = (audio_row(1, "Walk In", "Walk In.wav", 3, sounds) + audio_row(2, "Second Tune", "Second Tune.wav", 3, sounds)
              + audio_row(3, "Lost Tune", "Lost Tune.wav", 5, sounds) + audio_row(4, "Last Tune", "Last Tune.wav", 2, sounds)
              + audio_row(5, "Round", "Round.wav", 1.2, sounds, target=False, loop=True))
    effects = (audio_row(6, "Bell", "Bell.wav", 2, sounds, target=False, effect=True)
               + audio_row(7, "Thunder", "Thunder.wav", 4, sounds, target=False, effect=True, volume=0.5))
    path = os.path.join(workspace, "Playlists", "Audio")
    with open(path, "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (AUDIO_BIN % {"before": before, "effects": effects}).encode()))
    shutil.copy(path, os.path.join(test_dir, "audio-before"))


def after_audio(workspace, test_dir):
    """What the audio bin's file holds once the test has worked on it, set against the file the test was given
    (prepare_audio): the rows that were left alone, the one that was changed, the one that was added and the playlist
    that was made. And what a piece dragged onto a slide and onto the macros left in the song's file (set against
    song-before, which the test kept) and in the workspace's file of macros (against the fixture's)."""
    import difflib

    def decoded(path, message="rv.data.PlaylistDocument", proto="propresenter.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def rows(text):
        """Each row of a decoded playlists file, by its name: its lines of the text."""
        found = {}
        for block in re.finditer(r"^( +)items \{\n\1  uuid \{\n.*?^\1\}\n", text, re.S | re.M):
            found[re.search(r'^ +name: "([^"]*)"', block.group(0), re.M).group(1)] = block.group(0)
        return found

    def added_and_removed(was, now):
        """The lines one text has that the other has not, as two lists, each in order."""
        added, removed = [], []
        for line in difflib.ndiff(was.split("\n"), now.split("\n")):
            if line.startswith("+ "):
                added.append(line[2:])
            elif line.startswith("- "):
                removed.append(line[2:])
        return added, removed

    was = decoded(os.path.join(test_dir, "audio-before"))
    now = decoded(os.path.join(workspace, "Playlists", "Audio"))
    before, after = rows(was), rows(now)
    untouched = ["Walk In", "Second Tune", "Lost Tune", "Round", "Bell", "Thunder"]
    last_added, last_removed = added_and_removed(before.get("Last Tune", ""), after.get("Last Tune", ""))
    extra = after.get("Extra Tune", "")
    names = re.findall(r'^ +name: "([^"]*)"\n +(?:expanded: true\n +)?(?:playlists|items) \{', now, re.M)
    sounds = os.path.join(test_dir, "sounds")
    lines = [
        said(list(after) == ["Walk In", "Second Tune", "Lost Tune", "Last Tune", "Round", "Extra Tune", "Bell", "Thunder"],
             "in the audio bin's file: the rows ProPresenter wrote, in their order, and the one that was added after them in its playlist", ", ".join(after)),
        said(all(name in before and before[name] == after.get(name) for name in untouched),
             "in the audio bin's file: every row that was not changed is as ProPresenter wrote it, to the letter (one of them was made a sound effect and then music again)",
             ", ".join(name for name in untouched if before.get(name) != after.get(name))),
        said(last_added == [] and [line.strip() for line in last_removed] == ["completion_target_type: COMPLETION_TARGET_TYPE_NEXT"],
             "in the audio bin's file: the row that was set to stop has lost the line that said the next follows it, and nothing else",
             "added %s, removed %s" % (last_added, last_removed)),
        said(all(part in extra for part in ("completion_target_type: COMPLETION_TARGET_TYPE_NEXT", "completion_action_type: COMPLETION_ACTION_TYPE_LAST",
                                            "hot_key {", "isEnabled: true", "duration: 2.5", "type: ACTION_TYPE_MEDIA", "volume: 1", "play_rate: 1",
                                            "out_point: 2.5", "end_point: 2.5", "should_fade_in: true", "should_fade_out: true", "times_to_loop: 1",
                                            "retrigger: RETRIGGER_SETTING_AUTOMATIC", 'format: "WAV"'))
             and extra.count("file://" + urllib.parse.quote(os.path.join(sounds, "Extra Tune.wav"))) == 2,
             "in the audio bin's file: the row that was added is laid out as ProPresenter's are, and names its file where it is on this computer"),
        said("audio_channels" not in extra and "is_custom_mapping" not in extra and "audio_type" not in extra and "playback_behavior" not in extra,
             "in the audio bin's file: with nothing said of which outputs its channels go to, and nothing of what an action leaves unsaid"),
        said(names == ["PLAYLIST", "Services", "Before the Service", "Effects", "Sunday", "Nothing Yet"] and now.count("type: TYPE_AUDIO") == 1
             and re.search(r'name: "Sunday"\n +items \{\n +\}', now) is not None,
             "in the audio bin's file: the playlist that was made is in the folder of the one that was picked, with nothing in it", ", ".join(names)),
        said(now.split("root_node {")[0] == was.split("root_node {")[0],
             "in the audio bin's file: what it says of itself is as it was"),
    ]

    # The song a sound effect was dragged onto
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))
    song_was = decoded(os.path.join(frames, "song-before"), "rv.data.Presentation", "presentation.proto")
    song = glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", "Awake My Soul.pro"))
    song_now = decoded(song[0], "rv.data.Presentation", "presentation.proto") if song else ""
    added, removed = added_and_removed(song_was, song_now)
    block = "\n".join(added)
    bell = before.get("Bell", "")
    lines += [
        # (One action's worth of lines and no more: one line that opens an action, and as many brackets closed as opened.)
        said(removed == [] and len(added) > 20 and sum(1 for line in added if line.strip() == "actions {") == 1
             and sum(line.count("{") for line in added) == sum(line.count("}") for line in added),
             "in the song's file: an action has been added to it, and nothing else is other than it was", "%d lines added, %d removed" % (len(added), len(removed))),
        said(all(part in block for part in ("type: ACTION_TYPE_MEDIA", "audio_type: MEDIA_ACTION_AUDIO_TYPE_SOUND", "is_custom_mapping: true", "channel_index: 3",
                                            'artist: "unknown"', "duration: 2", "should_fade_in: true", 'name: "Bell"'))
             and block.count("file://" + urllib.parse.quote(os.path.join(sounds, "Bell.wav"))) == 2,
             "in the song's file: it is the row's own action as ProPresenter wrote it, with what this app knows nothing of, and called what the row is called"),
        said("0B0B0B0B-0000-0000-0000-000000000006-ACTION" in bell and "0B0B0B0B-0000-0000-0000-000000000006-ACTION" not in block
             and re.search(r'uuid \{\n +string: "[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}"', block) is not None,
             "in the song's file: with an id of its own, in the form ProPresenter uses"),
        said("completion_target_type" not in block, "in the song's file: and nothing of what follows it, which is a row's to say and not an action's"),
    ]

    # The macros: one made for a piece, and given another
    def macros(text):
        return {re.search(r'^  name: "([^"]*)"', block, re.M).group(1): block for block in re.findall(r"^macros \{\n.*?^\}\n", text, re.S | re.M)}

    macros_was = macros(decoded(os.path.join(FIXTURES, "workspaces", "Act", "Configuration", "Macros"), "rv.data.MacrosDocument", "macros.proto"))
    macros_now = macros(decoded(os.path.join(workspace, "Configuration", "Macros"), "rv.data.MacrosDocument", "macros.proto"))
    made = macros_now.get("Thunder", "")
    actions = re.findall(r"^  actions \{\n.*?^  \}\n", made, re.S | re.M)
    lines += [
        said(set(macros_now) == set(macros_was) | {"Thunder"} and all(macros_now[name] == macros_was[name] for name in macros_was),
             "in the macros' file: one macro more, and the others to the letter as they were", ", ".join(sorted(set(macros_now) ^ set(macros_was)))),
        said(len(actions) == 2 and all("type: ACTION_TYPE_MEDIA" in action and "audio_type: MEDIA_ACTION_AUDIO_TYPE_SOUND" in action for action in actions)
             and "Thunder.wav" in actions[0] and "volume: 0.5" in actions[0] and "Bell.wav" in actions[1],
             "in the macros' file: its two actions are the two pieces, each as its row has it", "%d actions" % len(actions)),
        said(len(actions) == 2 and "playback_behavior: PLAYBACK_BEHAVIOR_LOOP" in actions[1] and "playback_behavior" not in actions[0]
             and "playback_behavior" not in bell,
             "in the macros' file: the one that was set to go round says so in its media, and its row in the bin is as it was"),
    ]
    return lines


# A slide's notes as ProPresenter keeps them: Apple's RTF, Helvetica Neue at 24 points (which RTF counts in halves), in
# white. The words are nobody's.
NOTES_RTF = ("{\\rtf1\\ansi\\ansicpg1252\\cocoartf2822\n"
             "\\cocoatextscaling0\\cocoaplatform0{\\fonttbl\\f0\\fnil\\fcharset0 HelveticaNeue;}\n"
             "{\\colortbl;\\red255\\green255\\blue255;\\red255\\green255\\blue255;}\n"
             "{\\*\\expandedcolortbl;;\\csgray\\c100000;}\n"
             "\\pard\\pardirnatural\\partightenfactor0\n\n"
             "\\f0\\fs48 \\cf2 Watch the band for the ending\\\n"
             "Hold the last chord}")


def prepare_notes(workspace, test_dir):
    """A song with notes on its second slide, as ProPresenter writes a slide's notes: the RTF, and beside it the
    attributes it leaves empty. It is a song of the workspace under another name, so that the song itself is left as
    it was. Nothing is written but that one file, in the copy's library."""
    library = os.path.join(workspace, "Libraries", "MainHall")
    with open(os.path.join(library, "Awake My Soul.pro"), "rb") as source:
        text = protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()
    places = [found.end() for found in re.finditer(r"^ +presentation \{\n", text, re.M)]
    if len(places) < 3 or "notes {" in text:
        raise RuntimeError("the song the notes test is made from is not as it was: %d slides" % len(places))
    notes = 'notes {\n  rtf_data: "%s"\n  attributes {\n  }\n}\n' % NOTES_RTF.replace("\\", "\\\\").replace("\n", "\\n")
    text = text[:places[1]] + notes + text[places[1]:]
    text = re.sub(r'^name: ".*"$', 'name: "Notes Song"', text, count=1, flags=re.M)
    text = re.sub(r'^uuid \{\n  string: "[^"]*"', 'uuid {\n  string: "%s"' % str(uuid.uuid4()).upper(), text, count=1, flags=re.M)
    target = os.path.join(library, "Notes Song.pro")
    if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("the notes test would write outside its own folder: " + os.path.realpath(target))
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))


def after_notes(workspace, test_dir):
    """What the notes and the links left in the files, read with protoc and not by the app: the song as the test kept
    it at each step (song-before, song-bold and the song-link-… copies) and the stage layouts against the fixture's."""
    import difflib
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(path, message="rv.data.Presentation", proto="presentation.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def changed(was, now):
        """The lines taken out and the lines put in, without their indents."""
        out, put = [], []
        for line in difflib.unified_diff(was.splitlines(), now.splitlines(), lineterm="", n=0):
            if line.startswith("-") and not line.startswith("---"):
                out.append(line[1:].strip())
            elif line.startswith("+") and not line.startswith("+++"):
                put.append(line[1:].strip())
        return out, put

    def kept(name):
        path = os.path.join(frames, name)
        return decoded(path) if os.path.exists(path) else ""

    before, bold = kept("song-before"), kept("song-bold")
    out, put = changed(before, bold)
    notes_block = re.search(r"notes \{\n\s+rtf_data: .*\n\s+attributes \{\n\s+\}\n\s+\}", bold)
    lines = [
        said(before != "" and bold != "" and len(out) == 1 and len(put) == 1 and out[0].startswith("rtf_data:") and put[0].startswith("rtf_data:")
             and "Watch" in put[0] and "\\\\b" in put[0],
             "in the song's file: a word of the notes made bold changed the notes' RTF and not one other line", "%d out, %d in" % (len(out), len(put))),
        said(notes_block is not None and bold.count("notes {") == 1,
             "in the song's file: the attributes ProPresenter wrote beside the notes are still beside them, and no other slide has notes"),
    ]
    linked = {name: kept("song-link-" + name) for name in ("notes", "preserve", "name", "clock", "video")}

    def link(name):
        """The link of the text box the test made, in one of the copies: the lines the copy has that the one before it had not."""
        return "\n".join(changed(bold, linked[name])[1])

    lines += [
        said("slide_text {" in link("notes") and "source_option: TEXT_SOURCE_OPTION_NOTES" in link("notes") and "preserve_notes_format" not in link("notes")
             and "source_slide" not in link("notes"),
             "in the song's file: a text box linked to the current slide's notes says so, and nothing about their formatting"),
        said("source_option: TEXT_SOURCE_OPTION_NOTES" in link("preserve") and "preserve_notes_format: true" in link("preserve"),
             "in the song's file: with their formatting kept, it says that too"),
        said("source_option: TEXT_SOURCE_OPTION_ELEMENT_MATCHING_NAME" in link("name") and 'name_to_match: "Lyrics"' in link("name")
             and "preserve_notes_format: true" in link("name"),
             "in the song's file: linked to the elements of a name, it has the name, and what was said of the notes' formatting is left as it was"),
        said("clock_text {" in link("clock") and 'clock_format_string: "${clock}"' in link("clock") and "date_type: DATE_FORMATTER_STYLE_FULL" in link("clock")
             and "time_format: DATE_FORMATTER_STYLE_SHORT" in link("clock") and "military_time_enabled: true" in link("clock") and "slide_text {" not in link("clock"),
             "in the song's file: linked to the clock, it has how the date and the time are written, and the slide's link is gone",
             " | ".join(l for l in link("clock").splitlines() if "clock" in l or "date" in l or "time" in l or "military" in l)),
        said("video_countdown {" in link("video") and 'timer_format_string: "${timer}"' in link("video") and "minute: STYLE_LONG" in link("video")
             and "second: STYLE_LONG" in link("video") and "ignore_looping_videos: true" in link("video") and "clock_text {" not in link("video"),
             "in the song's file: linked to a video's time, it has how that is written and that videos which loop are ignored",
             " | ".join(l for l in link("video").splitlines() if "timer" in l or "STYLE" in l or "loop" in l)),
    ]
    out, put = changed(bold, decoded(os.path.join(workspace, "Libraries", "MainHall", "Notes Song.pro")))
    lines.append(said(all(line.startswith("rtf_data:") for line in out) and len(out) <= 1,
                      "in the song's file: at the end, nothing that was there is gone but the notes' RTF as it was before they were formatted again",
                      "%d lines out" % len(out)))
    stage_was = decoded(os.path.join(FIXTURES, "workspaces", "Act", "Configuration", "Stage"), "rv.data.Stage.Document", "stage.proto")
    stage_now = decoded(os.path.join(workspace, "Configuration", "Stage"), "rv.data.Stage.Document", "stage.proto")
    out, put = changed(stage_was, stage_now)
    lines.append(said(out == [] and put == ["preserve_notes_format: true"],
                      "in the stage layouts' file: the one box set to keep the notes' formatting says so, and not one other line is changed",
                      "%d out, in: %s" % (len(out), " | ".join(put)[:200])))
    return lines


# A workspace's messages as ProPresenter kept the one file of them that was looked at (the ids and the names apart,
# which are made up): a message that shows a timer, in a slide of a theme, which goes when its timer has run out and
# can be seen on the network; and one that has only just been made. The timer is one of the workspace's by its name,
# with an id that is no timer's here; the theme is one of the workspace's by its name, with a slide that is none of
# its slides by id or by name, so that it is found by its place.
MESSAGES_FILE = """application_info {
  platform: PLATFORM_MACOS
  application: APPLICATION_PROPRESENTER
  application_version {
    major_version: 18
  }
}
messages {
  uuid {
    string: "5D1DAC57-0000-4AD0-A096-CCCB37FF425B"
  }
  title: "Service Countdown"
  visible_on_network: true
  template {
    name: "New Life Chapel"
    slide_uuid {
      string: "11FC4BA5-0000-4F7F-9286-B125F7EE3487"
    }
    slide_name: "Timer Overlay"
    slide_index: 0
  }
  clear_type: CLEAR_TYPE_AFTER_TIMERS
  message_text: "Starting in ${B4AFBF0E-0000-4F77-8831-CEA895044B8C}"
  tokens {
    uuid {
      string: "B4AFBF0E-0000-4F77-8831-CEA895044B8C"
    }
    timer {
      name: "Service Countdown"
      timer_uuid {
        string: "0E45D0AF-0000-4A31-BCFD-0F5A3358E225"
      }
    }
  }
  token_values {
    token_id {
      string: "B4AFBF0E-0000-4F77-8831-CEA895044B8C"
    }
    timer {
      configuration {
        countdown_to_time {
          time_of_day: 36120
        }
      }
      format {
        minute: STYLE_SHORT
        second: STYLE_LONG
      }
    }
    token_name: "Timer to 10:02"
  }
}
messages {
  uuid {
    string: "68F5B8E9-0000-4259-A990-D1863BC56C78"
  }
  title: "New Message"
}
"""


# A presentation of slides that are each one block of a colour and nothing else: no words, no background, so that what
# is under a slide shows around its block. Written as ProPresenter's own text of a presentation; nobody's but the tests'.
BLOCK_CUE = """cues {
  uuid {
    string: "%(cue)s"
  }
  completion_action_type: COMPLETION_ACTION_TYPE_LAST
  hot_key {
  }
  actions {
    uuid {
      string: "%(action)s"
    }
    isEnabled: true
    type: ACTION_TYPE_PRESENTATION_SLIDE
    slide {
      presentation {
        base_slide {
          elements {
            element {
              uuid {
                string: "%(element)s"
              }
              name: "Block"
              bounds {
                origin {
                  x: %(x)d
                  y: %(y)d
                }
                size {
                  width: 660
                  height: 340
                }
              }
              opacity: 1
              path {
                closed: true
                points {
                  point {
                  }
                  q0 {
                  }
                  q1 {
                  }
                }
                points {
                  point {
                    x: 1
                  }
                  q0 {
                    x: 1
                  }
                  q1 {
                    x: 1
                  }
                }
                points {
                  point {
                    x: 1
                    y: 1
                  }
                  q0 {
                    x: 1
                    y: 1
                  }
                  q1 {
                    x: 1
                    y: 1
                  }
                }
                points {
                  point {
                    y: 1
                  }
                  q0 {
                    y: 1
                  }
                  q1 {
                    y: 1
                  }
                }
                shape {
                  type: TYPE_RECTANGLE
                }
              }
              fill {
                enable: true
                color {
                  red: %(red)d
                  green: %(green)d
                  blue: %(blue)d
                  alpha: 1
                }
              }
              stroke {
                width: 1
                color {
                  alpha: 1
                }
              }
              shadow {
                color {
                }
              }
              feather {
              }
            }
            info: 3
          }
          size {
            width: 1920
            height: 1080
          }
          uuid {
            string: "%(slide)s"
          }
        }
      }
    }
  }
  isEnabled: true
}
"""


def blocks_presentation(name, colours, x, y):
    """The text of a presentation called `name` with a slide for each of `colours` (red, green and blue, each 0 or 1):
    a block of that colour 660 by 340 at (x, y) of a slide of 1920 by 1080, in one group."""
    def new():
        return str(uuid.uuid4()).upper()

    cues, ids = [], []
    for red, green, blue in colours:
        ids.append(new())
        cues.append(BLOCK_CUE % {"cue": ids[-1], "action": new(), "element": new(), "slide": new(), "x": x, "y": y,
                                 "red": red, "green": green, "blue": blue})
    group = ('cue_groups {\n  group {\n    uuid {\n      string: "%s"\n    }\n    name: "Blocks"\n    hotKey {\n    }\n  }\n' % new()
             + "".join('  cue_identifiers {\n    string: "%s"\n  }\n' % i for i in ids) + "}\n")
    return ('application_info {\n  platform: PLATFORM_MACOS\n  application: APPLICATION_PROPRESENTER\n}\n'
            'uuid {\n  string: "%s"\n}\nname: "%s"\n' % (new(), name) + "".join(cues) + group)


def prepare_announce(workspace, test_dir):
    """Two presentations of the test's own in the copy's library, each of three slides that are a block of colour:
    "Notices", whose blocks are in the top right quarter (red, green, blue), and "Plain", whose blocks are in the
    bottom left (yellow, cyan, white). And a picture of one colour beside the test's own files, to be the media
    under a notice. Nothing is written outside the test's folder, and nothing under the copy's Media."""
    library = os.path.join(workspace, "Libraries", "Demo")
    made = {"Notices.pro": blocks_presentation("Notices", [(1, 0, 0), (0, 1, 0), (0, 0, 1)], 1160, 100),
            "Plain.pro": blocks_presentation("Plain", [(1, 1, 0), (0, 1, 1), (1, 1, 1)], 100, 640)}
    for name, text in made.items():
        target = os.path.join(library, name)
        if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
            raise RuntimeError("the announcements test would write outside its own folder: " + os.path.realpath(target))
        with open(target, "wb") as out:
            out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))
    shutil.copy(os.path.join(library, "Notices.pro"), os.path.join(test_dir, "announce-before"))
    with open(os.path.join(test_dir, "magenta.png"), "wb") as out:
        out.write(tiny_png((255, 0, 255), 64))


def builds_presentation():
    """The text of a presentation called "Builds", with ProPresenter's builds as it writes them (src/builds.h), on
    blocks of colour 660 by 340 in the four quarters of a slide of 1920 by 1080 (red top left, green top right, blue
    bottom left, white bottom right). Its own transition is a Dissolve of a second and a half.

      1. "Timed": the red block builds in with the slide (a Dissolve of 0.4 s); the green one after it, a second on
         (a Wipe from the left of 0.8 s); the blue one, there from the start, builds out a second after that (a
         Dissolve of 0.4 s); the white one has no builds.
      2. "Clicks": the red block builds in On Click (a Dissolve of 0.3 s), the green one with it 0.3 s on (the
         same), the blue one out On Click (the same). Its own transition is a Cut.
      3. "Own": a white block, no builds; its own transition a Wipe from the top of half a second."""
    def new():
        return str(uuid.uuid4()).upper()

    def effect(name, render, category, variables=""):
        return ('effect {\n  uuid {\n    string: "%s"\n  }\n  name: "%s"\n  render_id: "%s"\n  behavior_description: "%s"\n'
                '  category: "%s"\n%s}\n' % (new(), name, render, name, category, variables))

    def transition(kind, seconds):
        if kind == "dissolve":
            made = effect("Dissolve", "EC52A828-AD85-4602-B70C-1DEE7C904DB6", "Dissolves")
        elif kind == "cut":
            made = effect("Cut", "AB29D07B-E9E2-4E0A-93BD-AD3EA58120FA", "")
        else:
            direction = {"wipe-left": "EFFECT_DIRECTION_LEFT", "wipe-top": "EFFECT_DIRECTION_TOP"}[kind]
            made = effect("Wipe", "7084A71F-C12C-4EEA-8D90-A2D30FB0B256", "Wipes",
                          '  variables {\n    name: "Direction"\n    description: "Direction of Wipe"\n    direction {\n'
                          '      direction: %s\n      default_direction: EFFECT_DIRECTION_TOP\n      available_directions: 495\n'
                          '    }\n  }\n' % direction)
        return "transition {\n  duration: %s\n%s}\n" % (seconds, "".join("  " + line + "\n" for line in made.rstrip("\n").split("\n")))

    def build(field, element, start, delay, kind, seconds):
        build_id = new()
        text = ('%s {\n  uuid {\n    string: "%s"\n  }\n  elementUUID {\n    string: "%s"\n  }\n' % (field, build_id, element)
                + ("  start: %s\n" % start if start != "START_ON_CLICK" else "")
                + ("  delayTime: %s\n" % delay if delay else "")
                + "".join("  " + line + "\n" for line in transition(kind, seconds).rstrip("\n").split("\n")) + "}\n")
        return build_id, text

    # One block: the element of BLOCK_CUE, with its builds
    block_cue = BLOCK_CUE
    element_text = block_cue[block_cue.index("\n          elements {\n") + 1:block_cue.index("\n          size {\n") + 1]
    places = {"red": (100, 100, 1, 0, 0), "green": (1160, 100, 0, 1, 0), "blue": (100, 640, 0, 0, 1), "white": (1160, 640, 1, 1, 1)}

    def slide(blocks, own=None):
        elements, order = [], []
        for colour, builds in blocks:
            x, y, red, green, blue = places[colour]
            element = new()
            made = element_text % {"element": element, "x": x, "y": y, "red": red, "green": green, "blue": blue}
            made = made.replace('name: "Block"', 'name: "%s"' % colour.capitalize())
            extra = ""
            for field, start, delay, kind, seconds in builds:
                build_id, text = build(field, element, start, delay, kind, seconds)
                order.append(build_id)
                extra += "".join("            " + line + "\n" for line in text.rstrip("\n").split("\n"))
            made = made.replace("            info: 3\n", extra + "            info: 3\n")
            elements.append(made)
        cue, action, slide_id = new(), new(), new()
        orders = "".join('          element_build_order {\n            string: "%s"\n          }\n' % i for i in order)
        own_text = "".join("        " + line + "\n" for line in transition(*own).rstrip("\n").split("\n")) if own else ""
        return cue, ('cues {\n  uuid {\n    string: "%s"\n  }\n  completion_action_type: COMPLETION_ACTION_TYPE_LAST\n  hot_key {\n  }\n'
                     '  actions {\n    uuid {\n      string: "%s"\n    }\n    isEnabled: true\n    type: ACTION_TYPE_PRESENTATION_SLIDE\n'
                     '    slide {\n      presentation {\n        base_slide {\n' % (cue, action)
                     + "".join(elements) + orders
                     + '          size {\n            width: 1920\n            height: 1080\n          }\n          uuid {\n'
                     '            string: "%s"\n          }\n        }\n' % slide_id + own_text
                     + '      }\n    }\n  }\n  isEnabled: true\n}\n')

    slides = [
        slide([("red", [("build_in", "START_WITH_SLIDE", 0, "dissolve", 0.4)]),
               ("green", [("build_in", "START_AFTER_PREVIOUS", 1, "wipe-left", 0.8)]),
               ("blue", [("build_out", "START_AFTER_PREVIOUS", 1, "dissolve", 0.4)]),
               ("white", [])]),
        slide([("red", [("build_in", "START_ON_CLICK", 0, "dissolve", 0.3)]),
               ("green", [("build_in", "START_WITH_PREVIOUS", 0.3, "dissolve", 0.3)]),
               ("blue", [("build_out", "START_ON_CLICK", 0, "dissolve", 0.3)])], own=("cut", 0)),
        slide([("white", [])], own=("wipe-top", 0.5)),
    ]
    labels = ["Timed", "Clicks", "Own"]
    for (cue, text), label in zip(slides, labels):
        assert text.count("    isEnabled: true\n    type: ACTION_TYPE_PRESENTATION_SLIDE\n") == 1
    cues = "".join(text.replace("    isEnabled: true\n    type: ACTION_TYPE_PRESENTATION_SLIDE\n",
                                '    label {\n      text: "%s"\n    }\n    isEnabled: true\n    type: ACTION_TYPE_PRESENTATION_SLIDE\n' % label)
                   for (cue, text), label in zip(slides, labels))
    group = ('cue_groups {\n  group {\n    uuid {\n      string: "%s"\n    }\n    name: "Builds"\n    hotKey {\n    }\n  }\n' % new()
             + "".join('  cue_identifiers {\n    string: "%s"\n  }\n' % cue for cue, text in slides) + "}\n")
    return ('application_info {\n  platform: PLATFORM_MACOS\n  application: APPLICATION_PROPRESENTER\n}\n'
            'uuid {\n  string: "%s"\n}\nname: "Builds"\n' % new() + cues + group + transition("dissolve", 1.5))


def prepare_builds(workspace, test_dir):
    """The presentation of builds (builds_presentation) in the copy's library, kept as it was given (builds-before).
    Nothing is written outside the test's folder, and nothing under the copy's Media."""
    library = os.path.join(workspace, "Libraries", "Demo")
    target = os.path.join(library, "Builds.pro")
    if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("the test of builds would write outside its own folder: " + os.path.realpath(target))
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", builds_presentation().encode()))
    shutil.copy(target, os.path.join(test_dir, "builds-before"))


def after_builds(workspace, test_dir):
    """The editor's Build tab: what the presentation's file holds, read with protoc and not by the app. The test kept
    the file as it was given it (builds-before) and as it was when the editor was left (builds-edited): the first two
    slides, and the presentation's own transition, are as they were; the third has a Build In of its block, a Wipe from
    the top as ProPresenter makes one, With Previous after a second and a half, in the slide's order of builds; it has
    no Build Out, and no transition of its own; and nothing else of it was changed."""
    import difflib

    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def cues(text):
        return re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)

    before = decoded(os.path.join(test_dir, "builds-before"))
    edited = decoded(os.path.join(frames, "builds-edited"))
    now = decoded(os.path.join(workspace, "Libraries", "Demo", "Builds.pro"))
    was, made = cues(before), cues(edited)
    third = made[2] if len(made) == 3 else ""
    build = re.search(r"^ {12}build_in \{\n.*?^ {12}\}\n", third, re.S | re.M)
    build = build.group(0) if build else ""
    build_id = re.search(r'uuid \{\n\s*string: "([^"]+)"', build)
    changed = [line[0] + line[1:].strip() for line in difflib.ndiff(was[2].split("\n"), third.split("\n")) if line[:1] in "+-"] if len(was) == 3 else []
    allowed = re.compile(r"^[+-](build_in \{|uuid \{|string: |elementUUID \{|start: |delayTime: |transition \{|duration: |effect \{|name: "
                         r"|render_id: |behavior_description: |category: |variables \{|description: |direction \{|direction: "
                         r"|default_direction: |available_directions: |element_build_order \{|\})")
    return [
        said(edited == now, "the file is as the editor left it", "it was changed again after"),
        said(len(made) == 3 and made[0] == was[0] and made[1] == was[1], "the first two slides are as they were"),
        said(before[before.rindex("\ntransition {"):] == edited[edited.rindex("\ntransition {"):], "the presentation's own transition is as it was"),
        said(build != "" and "start: START_WITH_PREVIOUS" in build and "delayTime: 1.5" in build and 'render_id: "7084A71F-C12C-4EEA-8D90-A2D30FB0B256"' in build
             and "direction: EFFECT_DIRECTION_TOP\n" in build and "available_directions: 495" in build and 'description: "Direction of Wipe"' in build
             and 'category: "Wipes"' in build,
             "in the file: the block's Build In, a Wipe from the top as ProPresenter makes one, With Previous after 1.5 s", build.replace("\n", " ")[:600]),
        said(build_id is not None and ('element_build_order {\n            string: "%s"' % build_id.group(1)) in third
             and third.count("element_build_order {") == 1,
             "and in the slide's order of builds, alone"),
        said("build_out" not in third and not re.search(r"^ {8}transition \{", third, re.M), "it has no Build Out, and no transition of its own"),
        said(all(allowed.match(line) for line in changed), "and nothing else of that slide was changed",
             "; ".join(line for line in changed if not allowed.match(line))),
    ]


class DeviceStandIns:
    """What the devices test sends to, on this computer: a web server that notes every request (its method, path,
    headers and body) and a RossTalk switcher that notes every line, each written to a file in the test's folder as it
    comes (http-requests.json, rosstalk.txt) for the script to read; and aseqdump, ALSA's own tool, listening on Midi
    Through from outside the app (aseqdump.txt). Midi Through is the kernel's own loop, which every Linux computer
    has: what is sent to it comes back from it, which is how the test has the app hear the notes it sends."""

    def __init__(self, test_dir):
        import http.server
        import threading
        self.http_port = free_port()
        self.ross_port = free_port()
        self.requests = []
        self.lines = []
        requests_file = os.path.join(test_dir, "http-requests.json")
        lines_file = os.path.join(test_dir, "rosstalk.txt")
        stand_in = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def answer(self):
                length = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(length).decode("utf-8", "replace") if length else ""
                stand_in.requests.append({"method": self.command, "path": self.path, "headers": dict(self.headers.items()), "body": body})
                with open(requests_file, "w") as out:
                    json.dump(stand_in.requests, out)
                data = b'{"ok": true}'
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            do_GET = do_POST = do_PUT = do_DELETE = answer

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", self.http_port), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.listener = socket.socket()
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", self.ross_port))
        self.listener.listen(4)

        def read(connection):
            buffer = b""
            while True:
                try:
                    chunk = connection.recv(1024)
                except OSError:
                    return
                if not chunk:
                    return
                buffer += chunk
                while b"\r\n" in buffer:
                    line, buffer = buffer.split(b"\r\n", 1)
                    stand_in.lines.append(line.decode("utf-8", "replace"))
                    with open(lines_file, "w") as out:
                        out.write("\n".join(stand_in.lines) + "\n")

        def serve():
            while True:
                try:
                    connection, _ = self.listener.accept()
                except OSError:
                    return
                threading.Thread(target=read, args=(connection,), daemon=True).start()

        threading.Thread(target=serve, daemon=True).start()
        self.dump_file = open(os.path.join(test_dir, "aseqdump.txt"), "w")
        self.dump = subprocess.Popen(["aseqdump", "-p", "Midi Through"], stdout=self.dump_file, stderr=subprocess.STDOUT)

    def stop(self):
        self.server.shutdown()
        self.server.server_close()
        self.listener.close()
        self.dump.terminate()
        self.dump.wait(timeout=5)
        self.dump_file.close()


DEVICE_STAND_INS = None
# ProPresenter's devices for the test's workspace, as its CommunicationDevices file has them (seen in a file of the
# maintainer's): a MIDI device on Midi Through, a RossTalk switcher with no address (as ProPresenter's file had), and
# a kind this app does not have
SHOW_MIDI_ID = "0C9FC689-0000-4B33-A8F2-FAC5CF89A30A"
SWITCHER_ID = "3224F612-0000-4821-9066-A3627C596F21"


def pro_device(name, device_id, reconnect, xml):
    import base64
    return {"name": name, "reconnect": reconnect, "was_connected": False, "id": device_id,
            "parser": base64.b64encode(b"\xff\xfe" + xml.encode("utf-16-le")).decode()}


def communication_action(device_id, device_name, inner, name):
    """A communication action, as ProPresenter writes one, in the text of a cue"""
    return ('  actions {\n    uuid {\n      string: "%s"\n    }\n    name: "%s"\n    isEnabled: true\n'
            '    type: ACTION_TYPE_COMMUNICATION\n    communication {\n      device_identification {\n        parameter_uuid {\n'
            '          string: "%s"\n        }\n        parameter_name: "%s"\n      }\n%s    }\n  }\n'
            % (str(uuid.uuid4()).upper(), name, device_id, device_name, inner))


def prepare_devices(workspace, test_dir):
    """ProPresenter's devices for the copy of the workspace (its CommunicationDevices file, kept as it was given:
    devices-before), a presentation "Devices" of three blocks whose first slide sends a MIDI note On and whose second a
    RossTalk Custom Control, both as ProPresenter writes them (kept as given: devices-pro-before), the stand-ins they
    are sent to, started, and where the stand-ins are, for the script (devices-ports.json). Nothing is written outside
    the test's folder, and nothing under the copy's Media."""
    global DEVICE_STAND_INS
    configuration = os.path.join(workspace, "Configuration")
    library = os.path.join(workspace, "Libraries", "MainHall")
    for target in (configuration, library):
        if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
            raise RuntimeError("the devices test would write outside its own folder: " + os.path.realpath(target))
    midi = ('<RVProtocolParserMIDI bundleName="ProPresenter 21.4.app" bundleIdentifier="com.renewedvision.propresenter" behavior="2">'
            '<RVMIDIHardwareCommunicator sources="Midi Through Port-0" destinations="Midi Through Port-0"></RVMIDIHardwareCommunicator>'
            '</RVProtocolParserMIDI>')
    ross = ('<RVProtocolParserRossTalk bundleName="ProPresenter 21.4.app" bundleIdentifier="com.renewedvision.propresenter" behavior="2">'
            '<RVTCPNetworkHardwareCommunicator interface="" port="7788" address="" mode="1"></RVTCPNetworkHardwareCommunicator>'
            '</RVProtocolParserRossTalk>')
    dmx = '<RVProtocolParserDMX behavior="2"></RVProtocolParserDMX>'
    devices = [pro_device("Show MIDI", SHOW_MIDI_ID, True, midi), pro_device("Carbonite", SWITCHER_ID, False, ross),
               pro_device("Lights", "11111111-0000-3333-4444-555555555555", False, dmx)]
    with open(os.path.join(configuration, "CommunicationDevices"), "w") as out:
        json.dump(devices, out)
    shutil.copy(os.path.join(configuration, "CommunicationDevices"), os.path.join(test_dir, "devices-before"))
    text = blocks_presentation("Devices", [(1, 0, 0), (0, 1, 0), (0, 0, 1)], 100, 100)
    cues = re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)
    assert len(cues) == 3 and all(cue.endswith("  isEnabled: true\n}\n") for cue in cues)
    note = communication_action(SHOW_MIDI_ID, "Show MIDI", "      midi_command {\n        channel: 1\n        note: 12\n        intensity: 100\n      }\n",
                                "MIDI Note On")
    control = communication_action(SWITCHER_ID, "Carbonite", '      format: "CC x"\n      description: "Executes the specified custom control."\n'
                                   '      commands {\n        name: "Custom Control"\n        value: "1:3"\n        replacement_range {\n'
                                   '          start: 3\n          end: 4\n        }\n      }\n', "Custom Control")
    given = cues[0][:-len("  isEnabled: true\n}\n")] + note + "  isEnabled: true\n}\n"
    given2 = cues[1][:-len("  isEnabled: true\n}\n")] + control + "  isEnabled: true\n}\n"
    text = text.replace(cues[0], given).replace(cues[1], given2)
    target = os.path.join(library, "Devices.pro")
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))
    shutil.copy(target, os.path.join(test_dir, "devices-pro-before"))
    DEVICE_STAND_INS = DeviceStandIns(test_dir)
    with open(os.path.join(test_dir, "devices-ports.json"), "w") as out:
        json.dump({"http": DEVICE_STAND_INS.http_port, "rosstalk": DEVICE_STAND_INS.ross_port}, out)


MEDIA_ROW = """\
        items {
          uuid { string: "%(id)s-ITEM" }
          name: "%(name)s"
          cue {
            uuid { string: "%(id)s-CUE" }
            name: "%(name)s"
            completion_action_type: COMPLETION_ACTION_TYPE_LAST
            actions {
              uuid { string: "%(id)s-ACTION" }
              isEnabled: true
              type: ACTION_TYPE_MEDIA
              media {
                element {
                  uuid { string: "%(id)s-MEDIA" }
                  url { absolute_string: "%(address)s" local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } }
                  metadata { format: "PNG" }
                  image {
                    drawing { natural_size { width: 320 height: 180 } alpha_type: ALPHA_TYPE_STRAIGHT }
                    file { local_url { absolute_string: "%(address)s" local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } } }
                  }
                }
                audio { }
              }
            }
            isEnabled: true
          }
        }
"""

# A media bin as ProPresenter might have one: a folder "Services" with a playlist "Walk In" of two pictures, and a
# playlist "Backgrounds" of one, at the top
MEDIA_BIN = """\
application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER application_version { major_version: 21 minor_version: 4 } }
type: TYPE_MEDIA
root_node {
  uuid { string: "0C0C0C0C-0000-0000-0000-000000000000" }
  name: "PLAYLIST"
  expanded: true
  playlists {
    playlists {
      uuid { string: "0C0C0C0C-0000-0000-0000-00000000F01D" }
      name: "Services"
      expanded: true
      playlists {
        playlists {
          uuid { string: "0C0C0C0C-0000-0000-0000-000000000001" }
          name: "Walk In"
          items {
%(walkin)s
          }
        }
      }
    }
    playlists {
      uuid { string: "0C0C0C0C-0000-0000-0000-000000000002" }
      name: "Backgrounds"
      items {
%(backgrounds)s
      }
    }
  }
}
"""

# Two messages: "Parent Wanted", whose two tokens are given words (Name and Room) and were last given Sam and Foyer, as
# ProPresenter keeps a text token's value; and "Car Lights", of words alone
PLAY_MESSAGES = """application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER application_version { major_version: 21 minor_version: 4 } }
messages {
  uuid { string: "7A7A7A7A-0000-0000-0000-000000000001" }
  title: "Parent Wanted"
  message_text: "Parent of ${7B7B7B7B-0000-0000-0000-00000000000A} to the ${7B7B7B7B-0000-0000-0000-00000000000B}"
  tokens { uuid { string: "7B7B7B7B-0000-0000-0000-00000000000A" } text { name: "Name" } }
  tokens { uuid { string: "7B7B7B7B-0000-0000-0000-00000000000B" } text { name: "Room" } }
  token_values { token_id { string: "7B7B7B7B-0000-0000-0000-00000000000A" } text { value: "Sam" } token_name: "Name" }
  token_values { token_id { string: "7B7B7B7B-0000-0000-0000-00000000000B" } text { value: "Foyer" } token_name: "Room" }
}
messages {
  uuid { string: "7A7A7A7A-0000-0000-0000-000000000002" }
  title: "Car Lights"
  message_text: "A car has its lights on"
}
"""


def media_row(key, name, file, folder):
    return MEDIA_ROW % {"id": "0D0D0D0D-0000-0000-0000-%012d" % key, "name": name, "file": file,
                        "address": "file://" + urllib.parse.quote(os.path.join(folder, file))}


# A video row of a media playlist, as ProPresenter 21.4 writes one (the fields of a row of the maintainer's own media
# bin, the names and files the test's): `loop` says whether it goes round
VIDEO_ROW = """\
        items {
          uuid { string: "%(id)s-ITEM" }
          name: "%(name)s"
          cue {
            uuid { string: "%(id)s-CUE" }
            name: "%(name)s"
            completion_action_type: COMPLETION_ACTION_TYPE_LAST
            hot_key { }
            actions {
              uuid { string: "%(id)s-ACTION" }
              isEnabled: true
              duration: %(seconds)s
              type: ACTION_TYPE_MEDIA
              media {
                element {
                  uuid { string: "%(id)s-MEDIA" }
                  url { absolute_string: "%(address)s" platform: PLATFORM_MACOS local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } }
                  metadata { format: "avc1" color_format: COLOR_FORMAT_SDR }
                  video {
                    drawing { natural_size { width: 320 height: 180 } custom_image_bounds { origin { } size { } } crop_insets { }
                              alpha_type: ALPHA_TYPE_STRAIGHT }
                    audio { volume: 1 }
                    transport { play_rate: 1 out_point: %(seconds)s should_fade_in: true should_fade_out: true end_point: %(seconds)s
                                %(loop)s times_to_loop: 1 retrigger: RETRIGGER_SETTING_AUTOMATIC }
                    video { frame_rate: 30 thumbnail_position: -1 soft_loop_duration: 0.5 }
                    file { local_url { absolute_string: "%(address)s" platform: PLATFORM_MACOS local { root: ROOT_SHOW path: "Media/Assets/%(file)s" } } }
                  }
                }
                audio { }
              }
            }
            isEnabled: true
          }
        }
"""

# The transports' media bin: one playlist of three short videos of one colour each, the first going round, a long one
# going round, and a picture
TRANSPORT_BIN = """\
application_info { platform: PLATFORM_MACOS application: APPLICATION_PROPRESENTER application_version { major_version: 21 minor_version: 4 } }
type: TYPE_MEDIA
root_node {
  uuid { string: "0E0E0E0E-0000-0000-0000-000000000000" }
  name: "PLAYLIST"
  expanded: true
  playlists {
    playlists {
      uuid { string: "0E0E0E0E-0000-0000-0000-000000000001" }
      name: "Run"
      items {
%(rows)s
      }
    }
  }
}
"""

# The videos the transports' test plays: their names, how long each is, and its colour
TRANSPORT_VIDEOS = [("Red.mp4", 3, "red"), ("Green.mp4", 3, "lime"), ("Blue.mp4", 3, "blue"), ("Long.mp4", 40, "white")]


def make_video(path, seconds, colour, test_dir):
    """A video of one colour, 320 by 180 at 30 frames a second, made with ffmpeg, only inside the test's own folder."""
    if not os.path.realpath(path).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("a test's video was about to be written outside the test's folder: %s" % os.path.realpath(path))
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=%s:s=320x180:r=30:d=%d" % (colour, seconds),
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-y", path], check=True)


def prepare_transports(workspace, test_dir):
    """The transports' test (transports): its own media bin (TRANSPORT_BIN: three videos of three seconds, red going
    round, green and blue stopping, a long white one going round, and a still), made with ffmpeg in the test's folder;
    an audio bin of a long tune and a sound effect; and a presentation "Notices" of blocks, which the test sends to the
    announcements and gives the long video. The media bin as given is kept (transports-media-before). Nothing is
    written outside the test's folder, and nothing under the copy's Media."""
    library = os.path.join(workspace, "Libraries", "MainHall")
    playlists = os.path.join(workspace, "Playlists")
    for target in (library, playlists):
        if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
            raise RuntimeError("the transports' test would write outside its own folder: " + os.path.realpath(target))
    videos = os.path.join(test_dir, "videos")
    os.makedirs(videos)
    rows = ""
    for key, (name, seconds, colour) in enumerate(TRANSPORT_VIDEOS, start=1):
        make_video(os.path.join(videos, name), seconds, colour, test_dir)
        rows += VIDEO_ROW % {"id": "0F0F0F0F-0000-0000-0000-%012d" % key, "name": name[:-4], "file": name, "seconds": seconds,
                             "address": "file://" + urllib.parse.quote(os.path.join(videos, name)),
                             "loop": "playback_behavior: PLAYBACK_BEHAVIOR_LOOP" if name in ("Red.mp4", "Long.mp4") else ""}
    with open(os.path.join(videos, "Still.png"), "wb") as out:
        out.write(quarters_png(320, 180, [(255, 255, 0)] * 4))
    rows += media_row(9, "Still", "Still.png", videos)
    with open(os.path.join(playlists, "Media"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (TRANSPORT_BIN % {"rows": rows}).encode()))
    shutil.copy(os.path.join(playlists, "Media"), os.path.join(test_dir, "transports-media-before"))
    sounds = os.path.join(test_dir, "sounds")
    os.makedirs(sounds)
    write_tone(os.path.join(sounds, "Long Tune.wav"), 30.0, 440, 0.25, 0.25, test_dir)
    write_tone(os.path.join(sounds, "Bell.wav"), *AUDIO_TONES["Bell.wav"], test_dir)
    before = audio_row(1, "Long Tune", "Long Tune.wav", 30, sounds, target=False)
    effects = audio_row(6, "Bell", "Bell.wav", 2, sounds, target=False, effect=True)
    with open(os.path.join(playlists, "Audio"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (AUDIO_BIN % {"before": before, "effects": effects}).encode()))
    text = blocks_presentation("Notices", [(1, 1, 1), (0, 1, 1)], 100, 100)
    with open(os.path.join(library, "Notices.pro"), "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))


def after_transports(workspace, test_dir):
    """The media bin once the test has given its rows their Next Cues, read with protoc and not by the app: Red, which
    went round, goes on to the next and stops at its end; Green goes on to Blue, named by Blue's cue; Blue goes on to
    the first; Red then set to Random; and every other field of the file as it was given."""
    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.PlaylistDocument", "propresenter.proto", source.read()).decode()

    def items(text):
        """Each row of the playlist by its name: a block that starts with its own uuid (the list of rows starts with a row)"""
        found = {}
        for match in re.finditer(r"^( *)items \{\n\1  uuid \{\n(?:.*\n)*?\1\}\n", text, re.M):
            found[re.search(r'^\s*name: "([^"]*)"', match.group(0), re.M).group(1)] = match.group(0)
        return found

    was = decoded(os.path.join(test_dir, "transports-media-before"))
    now = decoded(os.path.join(workspace, "Playlists", "Media"))
    rows_was, rows_now = items(was), items(now)
    red, green, blue = rows_now.get("Red", ""), rows_now.get("Green", ""), rows_now.get("Blue", "")
    blue_cue = re.search(r'cue \{\n\s*uuid \{\n\s*string: "([^"]*)"', blue)
    # The rows with the lines the test changed taken out: what is left is as it was given
    changed = re.compile(r"^\s*(completion_target_type: .*|playback_behavior: PLAYBACK_BEHAVIOR_LOOP|completion_target_uuid \{|string: \"0F0F0F0F-0000-0000-0000-000000000003-CUE\"|\})$")
    def rest(block):
        return [line for line in block.splitlines() if not changed.match(line)]
    return [
        said("completion_target_type: COMPLETION_TARGET_TYPE_RANDOM" in red and "PLAYBACK_BEHAVIOR_LOOP" not in red,
             "Red goes on to one at random, last of what the test gave it, and stops at its end now, having gone round", red[:300]),
        said("completion_target_type: COMPLETION_TARGET_TYPE_CUE" in green and blue_cue is not None
             and re.search(r'completion_target_uuid \{\n\s*string: "%s"' % re.escape(blue_cue.group(1)), green) is not None,
             "Green goes on to Blue, named by Blue's cue (a GUESS at ProPresenter's form)", green[:400]),
        said("completion_target_type: COMPLETION_TARGET_TYPE_FIRST" in blue and "completion_target_uuid" not in blue,
             "Blue goes on to the first", blue[:300]),
        said(sorted(rows_now) == sorted(rows_was) and all(rest(rows_now[name]) == rest(rows_was[name]) for name in rows_was),
             "every other field of every row is as it was given", ", ".join(sorted(rows_now))),
        said(re.sub(r"(?ms)^ *items \{\n.*", "", now) == re.sub(r"(?ms)^ *items \{\n.*", "", was), "and the file round the rows is as it was"),
    ]


def prepare_scrolling(workspace, test_dir):
    """The scrolling text's test (scrolling): "It Is Well" as it was given (scrolling-before.pro)."""
    shutil.copy(os.path.join(workspace, "Libraries", "Demo", "It Is Well.pro"), os.path.join(test_dir, "scrolling-before.pro"))


def after_scrolling(workspace, test_dir):
    """"It Is Well" once the test has changed its scrolling box, read with protoc and not by the app: the box scrolling
    down, from off the box, at 0.8, once, feathered 0.3 at its start and as it was at its end, with the distance it had;
    every other text_scroller in the file as it was; and the file otherwise as it was but for that box's."""
    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    was = decoded(os.path.join(test_dir, "scrolling-before.pro"))
    now = decoded(os.path.join(workspace, "Libraries", "Demo", "It Is Well.pro"))
    blocks = lambda text: [m.group(0) for m in re.finditer(r"(?ms)^( *)text_scroller \{\n.*?^\1\}\n", text)]
    before, after = blocks(was), blocks(now)
    changed = [(a, b) for a, b in zip(before, after) if a != b]
    old, new = changed[0] if len(changed) == 1 else ("", "")
    field = lambda block, name: (re.search(r"\n\s*" + name + r": ([^\n]+)", block) or re.search("$", "")).group(1) if re.search(r"\n\s*" + name + ": ", block) else None
    rest_was = was
    for block in before:
        rest_was = rest_was.replace(block, "", 1)
    rest_now = now
    for block in after:
        rest_now = rest_now.replace(block, "", 1)
    return [
        said(len(before) == len(after) and len(changed) == 1, "one box's scrolling changed, and every other as it was", "%d changed of %d" % (len(changed), len(before))),
        said(field(new, "should_scroll") == "true" and field(new, "scrolling_direction") == "DIRECTION_DOWN" and field(new, "starts_off_screen") == "true"
             and field(new, "scroll_rate") == "0.8" and field(new, "should_repeat") is None and field(new, "fade_left") == "0.3"
             and field(new, "fade_right") == field(old, "fade_right") and field(new, "repeat_distance") == field(old, "repeat_distance"),
             "scrolling down, from off the box, at 0.8, once, feathered at its start, its distance and its end's feathering kept", new),
        said(rest_was == rest_now, "and nothing else of the file changed"),
    ]


def prepare_disabled(workspace, test_dir):
    """The disabled slides' test (disabled): "It Is Well" as it was given (disabled-before.pro)."""
    shutil.copy(os.path.join(workspace, "Libraries", "Demo", "It Is Well.pro"), os.path.join(test_dir, "disabled-before.pro"))


def after_disabled(workspace, test_dir):
    """"It Is Well" once the test has enabled the slide ProPresenter disabled and disabled another, read with protoc and
    not by the app: the cue that was disabled now enabled, the third slide's disabled, the same cues in the same order,
    and every cue but those two and the one given a Go To Next Timer as it was, to the byte of its text."""
    def cues(path):
        with open(path, "rb") as source:
            text = protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()
        return [m.group(0) for m in re.finditer(r"(?ms)^cues \{\n.*?^\}\n", text)]

    was = cues(os.path.join(test_dir, "disabled-before.pro"))
    now = cues(os.path.join(workspace, "Libraries", "Demo", "It Is Well.pro"))
    uuid = lambda cue: re.search(r'uuid \{\n\s*string: "([^"]+)"', cue).group(1)
    enabled = lambda cue: "\n  isEnabled: true" in cue
    off_was = [uuid(c) for c in was if not enabled(c)]
    off_now = [uuid(c) for c in now if not enabled(c)]
    changed = [uuid(a) for a, b in zip(was, now) if a != b]
    return [
        said([uuid(c) for c in now] == [uuid(c) for c in was], "the same cues, in the same order", "%d and %d" % (len(was), len(now))),
        said(len(off_was) == 1 and off_was[0] not in off_now and len(off_now) == 1, "the cue ProPresenter disabled enabled, and one other disabled",
             "%s then %s" % (off_was, off_now)),
        said(len(changed) == 3 and set(off_was) <= set(changed) and set(off_now) <= set(changed),
             "and no other cue changed but those two and the one given a Go To Next Timer",
             "%d changed" % len(changed)),
    ]


def prepare_calendar(workspace, test_dir):
    """The Calendar's test (calendar): that the workspace has no calendar yet, as Act has none, and its set-up file as it
    was given (calendar-workspace-before), which a calendar is not kept in."""
    if os.path.exists(os.path.join(workspace, "Configuration", "Calendar")):
        raise RuntimeError("the calendar test wants a workspace with no Configuration/Calendar")
    shutil.copy(os.path.join(workspace, "Configuration", "Workspace"), os.path.join(test_dir, "calendar-workspace-before"))


def after_calendar(workspace, test_dir):
    """The calendar the test made, read with protoc and not by the app: enabled; its two events, the one deleted gone; the
    first every week on the day it starts and two days on (as the test picks them), its action the macro in both of the file's forms as ProPresenter writes them (the
    calendar's own, type 102, and a Macro action with the macro's id and name); the second once, its action one
    presentation of the playlist (type 101 with the playlist's id and the row's, and a Library Playlist action); and the
    workspace's set-up file untouched."""
    path = os.path.join(workspace, "Configuration", "Calendar")
    if not os.path.exists(path):
        return [said(False, "the calendar was written to Configuration/Calendar")]
    with open(path, "rb") as source:
        text = protoc("decode", "rv.data.Calendar", "calendar.proto", source.read()).decode()
    events = [m.group(0) for m in re.finditer(r"(?ms)^events \{\n.*?^\}\n", text)]
    first = next((e for e in events if 'name: "Prelude"' in e), "")
    second = next((e for e in events if 'name: "Evening Service"' in e), "")
    own = lambda event: (re.search(r"(?ms)^  action \{\n.*?^  \}\n", event) or re.search("$", "")).group(0)
    ordinary = lambda event: [m.group(0) for m in re.finditer(r"(?ms)^  actions \{\n.*?^  \}\n", event)]
    macro = re.search(r'parameter_uuid \{\n\s*string: "([^"]+)"\n\s*\}\n\s*parameter_name: "Singing"', own(first))
    playlist = re.search(r'playlist_uuid \{\n\s*string: "([^"]+)"', own(second))
    item = re.search(r'playlist_item_uuid \{\n\s*string: "([^"]+)"', own(second))
    # The days the test picked: ProPresenter's (Sunday 1 to Saturday 7) of the day the first starts on, here, and two
    # days on, in the file's order
    names = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"]
    starts = re.search(r"(?m)^  date \{\n\s*seconds: (\d+)", first)
    day = (time.localtime(int(starts.group(1))).tm_wday + 1) % 7 + 1 if starts else 0
    days = sorted([day, (day + 1) % 7 + 1])
    with open(os.path.join(workspace, "Configuration", "Workspace"), "rb") as source:
        workspace_now = source.read()
    with open(os.path.join(test_dir, "calendar-workspace-before"), "rb") as source:
        workspace_was = source.read()
    return [
        said("active: true" in text and len(events) == 2 and "Missed" not in text, "the calendar enabled, with its two events, the one deleted gone",
             "%d events" % len(events)),
        said(starts is not None and re.findall(r"recurrence_days: (\w+)", first) == ["DAY_OF_WEEK_" + names[d - 1] for d in days]
             and "recurrence_limit_date" not in first,
             "the first every %s and %s, with no end" % tuple(names[d - 1].capitalize() for d in days) if starts else "the first's days", first[:300]),
        said("type: 102" in own(first) and macro is not None and len(ordinary(first)) == 1 and "type: ACTION_TYPE_MACRO" in ordinary(first)[0]
             and 'name: "Singing"' in ordinary(first)[0] and "isEnabled: true" in ordinary(first)[0]
             and macro is not None and ('string: "%s"' % macro.group(1)) in ordinary(first)[0].split("macro {")[0],
             "its action the macro, in both forms, as ProPresenter writes them", first[:900]),
        said("recurrence_days" not in second and "type: 101" in own(second) and playlist is not None and item is not None
             and len(ordinary(second)) == 1 and "type: ACTION_TYPE_LIBRARY_PLAYLIST" in ordinary(second)[0] and 'name: "Evening"' in ordinary(second)[0]
             and ('string: "%s"' % item.group(1)) in ordinary(second)[0] and ('string: "%s"' % playlist.group(1)) in ordinary(second)[0].split("playlist_item {")[0],
             "the second once, its action a presentation of the playlist, in both forms", second[:900]),
        said(workspace_now == workspace_was, "and the workspace's set-up file untouched"),
    ]


def prepare_copyright(workspace, test_dir):
    """The copyright display's test (copyright): every presentation of the library as it was given
    (copyright-pro-before/), and that the workspace has no copyright display's file yet, as Act has none."""
    if os.path.exists(os.path.join(workspace, "Configuration", "CCLI")):
        raise RuntimeError("the copyright test wants a workspace with no Configuration/CCLI")
    os.makedirs(os.path.join(test_dir, "copyright-pro-before"))
    for path in glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", "*.pro")):
        shutil.copy(path, os.path.join(test_dir, "copyright-pro-before", os.path.basename(path)))


def after_copyright(workspace, test_dir):
    """The copyright display's file and the presentation the test gave made-up details, read with protoc and not by
    the app: the file made with the display enabled, the licence number, the first and last slides, the layout as the
    test left it (ProPresenter's but for the publisher, with the CCLI song number last), and the template's box where
    the test moved it, linked to the copyright; the presentation's details as typed, shown, and the rest of it as it
    was; every other presentation to the byte."""
    def decoded(path, message="rv.data.Presentation", proto="presentation.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    path = os.path.join(workspace, "Configuration", "CCLI")
    ccli = decoded(path, "rv.data.CCLIDocument", "ccli.proto") if os.path.exists(path) else ""
    tokens = re.findall(r"token_type: (\w+)", ccli)
    changed, same = [], 0
    for path in sorted(glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", "*.pro"))):
        with open(path, "rb") as source:
            data = source.read()
        with open(os.path.join(test_dir, "copyright-pro-before", os.path.basename(path)), "rb") as source:
            old = source.read()
        if data == old:
            same += 1
        else:
            changed.append((decoded(os.path.join(test_dir, "copyright-pro-before", os.path.basename(path))), decoded(path)))
    was, now = changed[0] if len(changed) == 1 else ("", "")
    block = lambda text: (re.search(r"(?ms)^ccli \{\n.*?^\}\n", text) or re.search("$", "")).group(0)
    details = block(now)
    return [
        said(ccli != "", "the copyright display's file was made"),
        said("enable_ccli_display: true" in ccli and 'ccli_license: "998877"' in ccli and "display_type: DISPLAY_TYPE_FIRST_AND_LAST_SLIDE" in ccli,
             "with the display enabled, the licence number, and the first and last slides", ccli[:200]),
        said(tokens == ["Title", "Artist", "CopyrightYear", "LicenseNumber", "SongNumber"],
             "the layout as the test left it: ProPresenter's, but for the publisher, with the CCLI song number last", ",".join(tokens)),
        said(re.search(r"origin \{\n\s*x: 60\n\s*y: 700\n", ccli) is not None and "ccli_text {" in ccli,
             "the template's box where the test moved it, linked to the copyright", ""),
        said(len(changed) == 1 and same > 0, "one presentation was changed, and every other is as it was, to the byte", "%d changed, %d the same" % (len(changed), same)),
        said('song_title: "A Made Up Song"' in details and 'artist_credits: "Someone"' in details and "copyright_year: 2001" in details
             and "display: true" in details, "its details as typed, shown", details[:300]),
        said(was.replace(block(was), "") == now.replace(details, ""), "and the rest of it as it was"),
    ]


def prepare_markers(workspace, test_dir):
    """The playback markers' test (markers): a media bin of the test's own (TRANSPORT_BIN's form) with one playlist
    "Run" of a white video of twelve seconds that stops at its end and a still, made with ffmpeg in the test's folder;
    and an audio bin with a tone of ten seconds. What each bin file was is kept (markers-media-before,
    markers-audio-before). Nothing is written outside the test's folder, and nothing under the copy's Media."""
    playlists = os.path.join(workspace, "Playlists")
    if not os.path.realpath(playlists).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("the markers' test would write outside its own folder: " + os.path.realpath(playlists))
    videos = os.path.join(test_dir, "videos")
    os.makedirs(videos)
    make_video(os.path.join(videos, "Count.mp4"), 12, "white", test_dir)
    rows = VIDEO_ROW % {"id": "0F0F0F0F-0000-0000-0000-%012d" % 1, "name": "Count", "file": "Count.mp4", "seconds": 12,
                        "address": "file://" + urllib.parse.quote(os.path.join(videos, "Count.mp4")), "loop": ""}
    with open(os.path.join(videos, "Still.png"), "wb") as out:
        out.write(quarters_png(320, 180, [(255, 255, 0)] * 4))
    rows += media_row(9, "Still", "Still.png", videos)
    with open(os.path.join(playlists, "Media"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (TRANSPORT_BIN % {"rows": rows}).encode()))
    shutil.copy(os.path.join(playlists, "Media"), os.path.join(test_dir, "markers-media-before"))
    sounds = os.path.join(test_dir, "sounds")
    os.makedirs(sounds)
    write_tone(os.path.join(sounds, "Tune.wav"), 10.0, 440, 0.25, 0.25, test_dir)
    before = audio_row(1, "Tune", "Tune.wav", 10, sounds, target=False)
    with open(os.path.join(playlists, "Audio"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (AUDIO_BIN % {"before": before, "effects": ""}).encode()))
    shutil.copy(os.path.join(playlists, "Audio"), os.path.join(test_dir, "markers-audio-before"))


def after_markers(workspace, test_dir):
    """The bins once the test has given their rows playback markers, read with protoc and not by the app: the video's
    two markers on its media action, in the order of their times, as ProPresenter's message has them (the first named,
    red, at two and a half seconds, clearing the slide; the second dragged on the transport to about nine and named
    there, putting a prop on), the duplicate made
    and deleted gone; the tone's one marker, clearing the props; and each file otherwise as it was given."""
    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.PlaylistDocument", "propresenter.proto", source.read()).decode()

    def markers(text):
        return [m.group(0) for m in re.finditer(r"^( *)markers \{\n(?:.*\n)*?\1\}\n", text, re.M)]

    def without(text):
        for block in markers(text):
            text = text.replace(block, "", 1)
        return text

    media_was = decoded(os.path.join(test_dir, "markers-media-before"))
    media_now = decoded(os.path.join(workspace, "Playlists", "Media"))
    audio_was = decoded(os.path.join(test_dir, "markers-audio-before"))
    audio_now = decoded(os.path.join(workspace, "Playlists", "Audio"))
    video = markers(media_now)
    tone = markers(audio_now)
    first = video[0] if len(video) == 2 else ""
    second = video[1] if len(video) == 2 else ""
    time = lambda block: float(re.search(r"\n\s*time: ([0-9.e+-]+)", block).group(1)) if re.search(r"\n\s*time: ", block) else 0
    return [
        said(len(video) == 2, "the video has two markers, the duplicate made and deleted being gone", len(video)),
        said('name: "Lights"' in first and abs(time(first) - 2.5) < 0.05 and "red: 0.898" in first and "type: ACTION_TYPE_CLEAR" in first
             and re.search(r"uuid \{\n\s*string: \"[0-9A-F-]{36}\"", first) is not None,
             "the first is named, red, at two and a half seconds, with an id, and clears", first[:600]),
        said(abs(time(second) - 9.0) < 0.4 and "type: ACTION_TYPE_PROP" in second and 'name: "Prop Cue"' in second.split("actions {")[0],
             "the second is at about nine seconds, where it was dragged on the transport, named there, and puts a prop on", second[:600]),
        said(len(tone) == 1 and abs(time(tone[0]) - 1.0) < 0.3 and "type: ACTION_TYPE_CLEAR" in tone[0],
             "the tone has its one marker, at about a second, clearing", tone[0][:400] if tone else "none"),
        said(media_now.index("markers {") > media_now.index("video {") if "markers {" in media_now else False,
             "the markers are the media action's, beside its media", ""),
        said(without(media_now) == media_was and without(audio_now) == audio_was, "and the rest of both files is as it was given"),
    ]


def prepare_playacts(workspace, test_dir):
    """The test's own media bin (MEDIA_BIN: two pictures of one colour each, red and green, in the test's folder),
    audio bin (AUDIO_BIN, with two short tones before the service, each followed by the next, and a sound effect),
    messages (PLAY_MESSAGES), and a presentation "Playlists" of five blocks in the top left of the slide, so that the
    media under a slide shows round it. Its fifth slide has two actions as ProPresenter writes them: an Audio Bin
    Playlist by the playlist's name and an id the playlist no longer has (as two of the maintainer's do), and a
    Message for Car Lights with nothing for its tokens. What each file was is kept (playacts-*-before). Nothing is
    written outside the test's folder, and nothing under the copy's Media."""
    configuration = os.path.join(workspace, "Configuration")
    library = os.path.join(workspace, "Libraries", "MainHall")
    playlists = os.path.join(workspace, "Playlists")
    for target in (configuration, library, playlists):
        if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
            raise RuntimeError("the playlist actions' test would write outside its own folder: " + os.path.realpath(target))
    pictures = os.path.join(test_dir, "pictures")
    os.makedirs(pictures)
    for name, colour in (("Red.png", (255, 0, 0)), ("Green.png", (0, 255, 0))):
        with open(os.path.join(pictures, name), "wb") as out:
            out.write(quarters_png(320, 180, [colour] * 4))
    walkin = media_row(1, "Red", "Red.png", pictures) + media_row(2, "Green", "Green.png", pictures)
    backgrounds = media_row(3, "Green", "Green.png", pictures)
    with open(os.path.join(playlists, "Media"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (MEDIA_BIN % {"walkin": walkin, "backgrounds": backgrounds}).encode()))
    sounds = os.path.join(test_dir, "sounds")
    os.makedirs(sounds)
    for name in ("Walk In.wav", "Second Tune.wav", "Bell.wav"):
        seconds, pitch, left, right = AUDIO_TONES[name]
        write_tone(os.path.join(sounds, name), seconds, pitch, left, right, test_dir)
    before = audio_row(1, "Walk In", "Walk In.wav", 3, sounds) + audio_row(2, "Second Tune", "Second Tune.wav", 3, sounds)
    effects = audio_row(6, "Bell", "Bell.wav", 2, sounds, target=False, effect=True)
    with open(os.path.join(playlists, "Audio"), "wb") as out:
        out.write(protoc("encode", "rv.data.PlaylistDocument", "propresenter.proto", (AUDIO_BIN % {"before": before, "effects": effects}).encode()))
    with open(os.path.join(configuration, "Messages"), "wb") as out:
        out.write(protoc("encode", "rv.data.MessageDocument", "messages.proto", PLAY_MESSAGES.encode()))
    shutil.copy(os.path.join(configuration, "Messages"), os.path.join(test_dir, "playacts-messages-before"))
    shutil.copy(os.path.join(configuration, "Macros"), os.path.join(test_dir, "playacts-macros-before"))
    text = blocks_presentation("Playlists", [(1, 1, 1), (1, 1, 0), (0, 1, 1), (1, 0, 1), (0, 0, 1)], 100, 100)
    cues = re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)
    assert len(cues) == 5 and cues[4].endswith("  isEnabled: true\n}\n")
    pro = ('  actions {\n    uuid {\n      string: "D2C17F20-0000-4B06-8F49-80F4994478A5"\n    }\n    isEnabled: true\n'
           '    type: ACTION_TYPE_AUDIO_BIN_PLAYLIST\n    playlist_item {\n      playlist_uuid {\n        string: "9E98B135-0000-4A67-9450-8F86707697DC"\n'
           '      }\n      playlist_name: "Before the Service"\n    }\n  }\n'
           '  actions {\n    uuid {\n      string: "829AB8CE-0000-4E06-9C02-102846871ED9"\n    }\n    isEnabled: true\n'
           '    type: ACTION_TYPE_MESSAGE\n    message {\n      message_identificaton {\n        parameter_uuid {\n'
           '          string: "7A7A7A7A-0000-0000-0000-000000000002"\n        }\n        parameter_name: "Car Lights"\n      }\n    }\n  }\n')
    text = text.replace(cues[4], cues[4][:-len("  isEnabled: true\n}\n")] + pro + "  isEnabled: true\n}\n")
    target = os.path.join(library, "Playlists.pro")
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))
    shutil.copy(target, os.path.join(test_dir, "playacts-pro-before"))


def after_playacts(workspace, test_dir):
    """What the presentation and the macros hold once the test has given them actions, read with protoc and not by the
    app: each new action as ProPresenter writes its kind, with nothing in it ProPresenter does not define; Always
    Retrigger written only where it is on; ProPresenter's own two actions as they were, to the byte; and the messages'
    file as it was given, the words an action gave a message's tokens never written into the message."""
    def decoded(path, message="rv.data.Presentation", proto="presentation.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def cues(text):
        return re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)

    def actions(cue, kind):
        return [block for block in re.findall(r"^  actions \{\n.*?^  \}\n", cue, re.S | re.M) if "type: " + kind + "\n" in block]

    # Every field ProPresenter's playlist and message actions have, and nothing else
    known = re.compile(r"^\s*(actions \{|uuid \{|string: |isEnabled: |type: |playlist_item \{|playlist_uuid \{|playlist_name: "
                       r"|always_retrigger: |message \{|message_identificaton \{|parameter_uuid \{|parameter_name: |content \{"
                       r"|token_id \{|text \{|value: |token_name: |\})")
    before = cues(decoded(os.path.join(test_dir, "playacts-pro-before")))
    after = cues(decoded(os.path.join(workspace, "Libraries", "MainHall", "Playlists.pro")))
    if len(after) != 5:
        return [said(False, "the presentation still has its five slides", str(len(after)))]
    walk = actions(after[0], "ACTION_TYPE_MEDIA_BIN_PLAYLIST")
    audio = actions(after[1], "ACTION_TYPE_AUDIO_BIN_PLAYLIST")
    parent = actions(after[2], "ACTION_TYPE_MESSAGE")
    lights = actions(after[3], "ACTION_TYPE_MESSAGE")
    backgrounds = actions(after[3], "ACTION_TYPE_MEDIA_BIN_PLAYLIST")
    made = walk + audio + parent + lights + backgrounds
    strange = [line.strip() for block in made for line in block.splitlines() if not known.match(line)]
    walk, audio, parent, lights, backgrounds = (b[0] if len(b) == 1 else "" for b in (walk, audio, parent, lights, backgrounds))
    with open(os.path.join(test_dir, "playacts-messages-before"), "rb") as was, open(os.path.join(workspace, "Configuration", "Messages"), "rb") as now:
        messages_same = was.read() == now.read()
    macros_now = decoded(os.path.join(workspace, "Configuration", "Macros"), "rv.data.MacrosDocument", "macros.proto")
    macros_was = decoded(os.path.join(test_dir, "playacts-macros-before"), "rv.data.MacrosDocument", "macros.proto")
    in_macros = macros_now.count("type: ACTION_TYPE_MEDIA_BIN_PLAYLIST") - macros_was.count("type: ACTION_TYPE_MEDIA_BIN_PLAYLIST")
    return [
        said('playlist_uuid {\n        string: "0C0C0C0C-0000-0000-0000-000000000001"' in walk and 'playlist_name: "Walk In"' in walk
             and "always_retrigger: true" in walk and "item_uuid" not in walk and "select_playlist" not in walk,
             "the first slide plays Walk In, by its id and name, Always Retrigger written as on, and no item and no Select Playlist", walk[:400]),
        said('playlist_name: "Before the Service"' in audio and "0C0C0C0C" not in audio and "0A0A0A0A-0000-0000-0000-000000000001" in audio
             and "always_retrigger" not in audio,
             "the second plays the audio bin's Before the Service, as ProPresenter writes one: Always Retrigger off and so not written", audio[:400]),
        said(audio.count("playlist_item {") == 1 and walk.count("playlist_item {") == 1, "each is one playlist item, as ProPresenter's are"),
        said('parameter_name: "Parent Wanted"' in parent and parent.count("content {") == 2
             and re.search(r'token_id \{\n\s*string: "7B7B7B7B-0000-0000-0000-00000000000A"\n\s*\}\n\s*text \{\n\s*value: "Alex"\n\s*\}\n\s*token_name: "Name"', parent) is not None
             and re.search(r'value: "Hall"\n\s*\}\n\s*token_name: "Room"', parent) is not None,
             "the third shows Parent Wanted, giving Name and Room words of its own, each as the message keeps a token's value", parent[:600]),
        said('parameter_name: "Car Lights"' in lights and "content" not in lights,
             "the fourth shows Car Lights, which has no tokens, and says nothing of them, as ProPresenter's own does", lights[:300]),
        said('playlist_name: "Backgrounds"' in backgrounds, "and plays Backgrounds, dropped on it from the media bin", backgrounds[:300]),
        said(strange == [], "nothing in them ProPresenter's actions do not have", " / ".join(strange[:6])),
        said(after[4] == before[4], "ProPresenter's own two actions, on the fifth slide, are as they were, to the byte"),
        said(messages_same, "the messages' file is as it was given: an action's words for a token are the action's, not the message's"),
        said(in_macros == 1, "a macro has the playlist dropped on it", in_macros),
    ]


def after_devices(workspace, test_dir):
    """What reached the stand-ins, and what the files hold, read with protoc and not by the app: the MIDI note seen by
    ALSA's own tool as the app sent it; ProPresenter's devices file as it was given; the presentation's two actions of
    ProPresenter's as they were, to the byte; and the third slide's HTTP request in ProPresenter's general form, with
    nothing in it that ProPresenter does not define."""
    stand_ins = DEVICE_STAND_INS
    stand_ins.stop()
    with open(os.path.join(test_dir, "aseqdump.txt")) as dump:
        heard = dump.read()

    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def cues(text):
        return re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)

    with open(os.path.join(test_dir, "devices-before"), "rb") as was, \
         open(os.path.join(workspace, "Configuration", "CommunicationDevices"), "rb") as now:
        same_devices = was.read() == now.read()
    before, after = cues(decoded(os.path.join(test_dir, "devices-pro-before"))), cues(decoded(os.path.join(workspace, "Libraries", "MainHall", "Devices.pro")))
    third = after[2] if len(after) == 3 else ""
    http = re.search(r"^  actions \{\n(?:(?!^  actions \{).)*?type: ACTION_TYPE_COMMUNICATION.*?^  \}\n", third, re.S | re.M)
    http = http.group(0) if http else ""
    # Every field ProPresenter's communication action has, and nothing else
    known = re.compile(r"^\s*(actions \{|uuid \{|string: |name: |isEnabled: |type: |communication \{|device_identification \{|parameter_uuid \{"
                       r"|parameter_name: |format: |description: |commands \{|value: |possible_values: |\})")
    return [
        said("Note on" in heard and "note 12, velocity 100" in heard, "ALSA's own tool saw the note go out to Midi Through, as sent",
             " / ".join(line.strip() for line in heard.splitlines() if "Note" in line)[:300]),
        said(same_devices, "ProPresenter's devices file is as it was given: it is read, and never written"),
        said(len(after) == 3 and after[0] == before[0] and after[1] == before[1], "the two slides' actions of ProPresenter's are as they were"),
        said(http != "" and 'name: "HTTP Request"' in http and 'format: "HTTP"' in http and 'value: "POST"' in http
             and http.count("possible_values:") == 4 and 'value: "/v1/go"' in http and 'value: "X-Slide: 3"' in http
             and 'parameter_name: "Webhooks"' in http,
             "in the file: the HTTP request as a communication action, its method, path, body and header as named values",
             http.replace("\n", " ")[:600]),
        said(http != "" and all(known.match(line) for line in http.split("\n") if line.strip()),
             "and nothing in it that ProPresenter's format does not have", "; ".join(line.strip() for line in http.split("\n") if line.strip() and not known.match(line))),
    ]


def prepare_advance(workspace, test_dir):
    """The two presentations of blocks the announcements test has (prepare_announce), and a third, "Timed", whose first
    slide has a Go To Next Timer of three seconds as ProPresenter writes one: the cue's target is the next cue, after a
    time. The test keeps copies of the files as they were given it."""
    prepare_announce(workspace, test_dir)
    library = os.path.join(workspace, "Libraries", "Demo")
    text = blocks_presentation("Timed", [(1, 0, 1), (1, 1, 0)], 100, 100)
    plain = "  completion_action_type: COMPLETION_ACTION_TYPE_LAST\n"
    timed = ("  completion_target_type: COMPLETION_TARGET_TYPE_NEXT\n  completion_action_type: COMPLETION_ACTION_TYPE_AFTER_TIME\n"
             "  completion_time: 3\n")
    assert text.count(plain) == 2
    text = text.replace(plain, timed, 1)
    target = os.path.join(library, "Timed.pro")
    if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("the test of going on would write outside its own folder: " + os.path.realpath(target))
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.Presentation", "presentation.proto", text.encode()))
    shutil.copy(target, os.path.join(test_dir, "advance-timed-before"))
    shutil.copy(os.path.join(library, "Plain.pro"), os.path.join(test_dir, "advance-before"))


def after_advance(workspace, test_dir):
    """Go To Next Timers and a Slide Show: what the presentations' files hold, read with protoc and not by the app.
    The test kept the file of blocks as it was given it (advance-before), with a timer of a second on its first
    slide (advance-timer), with one of half a second that loops on its last as well (advance-loop), and with a Slide
    Show and no timers (advance-show); and the file whose first slide came with a timer (advance-timed-before),
    which the test took off."""
    import difflib

    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(path):
        with open(path, "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def changed(was, now):
        """The lines that are in one and not the other: "+" for one that is new, "-" for one that is gone."""
        return [line[0] + line[1:].strip() for line in difflib.ndiff(was.split("\n"), now.split("\n")) if line[:1] in "+-"]

    def cues(text):
        return re.findall(r"^cues \{\n.*?^\}\n", text, re.S | re.M)

    def done(cue):
        """What a cue says of being done: its lines for that, in the file's order."""
        return [line.strip() for line in cue.split("\n") if line.startswith("  completion_")]

    before = decoded(os.path.join(test_dir, "advance-before"))
    timer, loop, show = (decoded(os.path.join(frames, name)) for name in ("advance-timer", "advance-loop", "advance-show"))
    timed_was = decoded(os.path.join(test_dir, "advance-timed-before"))
    timed_now = decoded(os.path.join(workspace, "Libraries", "Demo", "Timed.pro"))
    return [
        said(done(cues(timer)[0]) == ["completion_target_type: COMPLETION_TARGET_TYPE_NEXT", "completion_action_type: COMPLETION_ACTION_TYPE_AFTER_TIME",
                                      "completion_time: 1"]
             and [done(cue) for cue in cues(timer)[1:]] == [["completion_action_type: COMPLETION_ACTION_TYPE_LAST"]] * 2,
             "in the file: a Go To Next Timer is its cue's target, the next cue, after a time, as ProPresenter writes one; the other cues are as they were",
             " / ".join(", ".join(done(cue)) for cue in cues(timer))),
        said(sorted(changed(before, timer)) == sorted(["+completion_target_type: COMPLETION_TARGET_TYPE_NEXT", "+completion_time: 1",
                                                       "-completion_action_type: COMPLETION_ACTION_TYPE_LAST",
                                                       "+completion_action_type: COMPLETION_ACTION_TYPE_AFTER_TIME"]),
             "in the file: and nothing else of it was changed by that", "; ".join(changed(before, timer))),
        said(done(cues(loop)[2]) == ["completion_target_type: COMPLETION_TARGET_TYPE_FIRST", "completion_action_type: COMPLETION_ACTION_TYPE_AFTER_TIME",
                                     "completion_time: 0.5"] and done(cues(loop)[0]) == done(cues(timer)[0]),
             "in the file: with Loop to Beginning the target is the first cue", ", ".join(done(cues(loop)[2]))),
        said(changed(before, show) == ["+slide_show_duration: 0.6"],
             "in the file: a Slide Show is the presentation's time for it, and with the timers taken off again that is the only line that is not as it was",
             "; ".join(changed(before, show))),
        said(done(cues(timed_was)[0])[0] == "completion_target_type: COMPLETION_TARGET_TYPE_NEXT"
             and done(cues(timed_now)[0]) == ["completion_action_type: COMPLETION_ACTION_TYPE_LAST", "completion_time: 3"],
             "in the file: a timer that ProPresenter made, taken off, leaves its cue with no target and done last, the time still there",
             ", ".join(done(cues(timed_now)[0]))),
        said(sorted(changed(timed_was, timed_now)) == sorted(["-completion_target_type: COMPLETION_TARGET_TYPE_NEXT",
                                                              "-completion_action_type: COMPLETION_ACTION_TYPE_AFTER_TIME",
                                                              "+completion_action_type: COMPLETION_ACTION_TYPE_LAST"]),
             "in the file: and nothing else of that file was changed", "; ".join(changed(timed_was, timed_now))),
    ]


def prepare_masks(workspace, test_dir):
    """The two presentations of blocks the announcements test has (prepare_announce), one of them kept as it was given
    (masks-plain-before), for the label a slide of it is given in the editor's list."""
    prepare_announce(workspace, test_dir)
    shutil.copy(os.path.join(workspace, "Libraries", "Demo", "Plain.pro"), os.path.join(test_dir, "masks-plain-before"))


def after_masks(workspace, test_dir):
    """Masks, and a slide's label typed in the editor's list: what the files hold, read with protoc and not by the app.
    The workspace had no set-up file; the test kept it as it was with a mask made, drawn on, named and given to the
    screen (masks-given), and at the end the mask has been removed again and another is left."""
    import difflib

    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(path, message="rv.data.ProPresenterWorkspace", proto="proworkspace.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def masks(text):
        return re.findall(r"^masks \{\n.*?^\}\n", text, re.S | re.M)

    given = decoded(os.path.join(frames, "masks-given"))
    end = decoded(os.path.join(workspace, "Configuration", "Workspace"))
    made = masks(given)
    first = made[0] if made else ""
    slide_id = re.search(r'^    uuid \{\n      string: "([^"]+)"', first, re.M)
    names = [re.search(r'^  name: "([^"]*)"', mask, re.M).group(1) for mask in made]
    plain_was = decoded(os.path.join(test_dir, "masks-plain-before"), "rv.data.Presentation", "presentation.proto")
    plain_now = decoded(os.path.join(workspace, "Libraries", "Demo", "Plain.pro"), "rv.data.Presentation", "presentation.proto")
    changed = [line[0] + line[1:].strip() for line in difflib.ndiff(plain_was.split("\n"), plain_now.split("\n")) if line[:1] in "+-"]
    return [
        said(names == ["Left Edge", "Mask"] and first.startswith("masks {\n  base_slide {\n") and slide_id is not None and len(slide_id.group(1)) == 36,
             "in the workspace's set-up file: the masks are ProPresenter's, each a slide with an id and a name, in the order they were made",
             ", ".join(names)),
        # (Black is a colour of nothing but its alpha, in a file that leaves out what is nought.)
        said(len(re.findall(r"^      element \{", first, re.M)) == 1 and "type: TYPE_RECTANGLE" in first
             and re.search(r"fill \{\n\s+color \{\n\s+alpha: 1\n\s+\}\n\s+enable: true\n", first) is not None,
             "in the workspace's set-up file: the first has the one shape drawn on it, filled with black"),
        said(slide_id is not None and len(re.findall(r'mask_uuid \{\s+string: "%s"' % slide_id.group(1), given)) == 1 and given.count("mask_uuid") == 1,
             "in the workspace's set-up file: the live look's line for the screen names the mask by the id of its slide, and nothing else names one"),
        said([re.search(r'^  name: "([^"]*)"', mask, re.M).group(1) for mask in masks(end)] == ["Mask"] and "mask_uuid" not in end,
             "in the workspace's set-up file, at the end: the mask that was removed is gone, the other is there, and no look gives a screen a mask"),
        said(slide_id is not None and re.sub(r"^masks \{\n.*?^\}\n", "", given, flags=re.S | re.M)
             .replace('    mask_uuid {\n      string: "%s"\n    }\n' % slide_id.group(1), "") == re.sub(r"^masks \{\n.*?^\}\n", "", end, flags=re.S | re.M)
             and "live_audience_look {" in end,
             "in the workspace's set-up file: and the rest of the file, the live look in it, is line for line what it was but for that"),
        said(changed == ['+label {', '+text: "Welcome"', '+}'],
             "in the presentation's file: the slide renamed in the editor's list has its label, and nothing else is changed", "; ".join(changed)),
    ]


def after_announce(workspace, test_dir):
    """Where a presentation is sent, and an action that clears the announcements: what the files hold, read with protoc
    and not by the app. The test kept the notices' file as it was given it (announce-before, by prepare_announce), sent
    to the announcements (announce-set) and sent back (announce-back); and the playlists' file with the notices in a
    playlist twice, before they were sent anywhere (announce-playlists)."""
    import difflib

    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def raw(path):
        with open(path, "rb") as source:
            return source.read()

    def decoded(data, message="rv.data.Presentation", proto="presentation.proto"):
        return protoc("decode", message, proto, data).decode()

    def rows(text):
        """Each presentation row of a decoded playlists file: the name of its file, and its lines of the text."""
        found = []
        for block in re.finditer(r"^( +)presentation \{\n.*?^\1\}\n", text, re.S | re.M):
            lines = block.group(0)
            path = re.search(r'absolute_string: "([^"]*)"', lines)
            found.append((os.path.basename(path.group(1)) if path else "", lines))
        return found

    before, sent, back = (raw(os.path.join(test_dir, "announce-before")), raw(os.path.join(frames, "announce-set")),
                          raw(os.path.join(frames, "announce-back")))
    was, now = decoded(before).split("\n"), decoded(sent).split("\n")
    added = [line for line in now if line not in was]
    lists_was = decoded(raw(os.path.join(frames, "announce-playlists")), "rv.data.PlaylistDocument", "propresenter.proto")
    lists_now = decoded(raw(os.path.join(workspace, "Playlists", "Library")), "rv.data.PlaylistDocument", "propresenter.proto")
    def marked(text):
        return sorted(name for name, lines in rows(text) if "content_destination: CONTENT_DESTINATION_ANNOUNCEMENTS" in lines)

    # (The fixture's own playlists may have rows that are sent to the announcements already.)
    ours_was = [lines for name, lines in rows(lists_was) if name in ("Notices.pro", "Plain.pro")]
    changed = [line[0] + line[1:].strip() for line in difflib.ndiff(lists_was.split("\n"), lists_now.split("\n")) if line[:1] in "+-"]
    newly = list(marked(lists_now))
    for name in marked(lists_was):
        newly.remove(name)
    plain = decoded(raw(os.path.join(workspace, "Libraries", "Demo", "Plain.pro")))
    clears = re.findall(r"^    clear \{\n.*?^    \}\n", plain, re.S | re.M)
    return [
        said(added == ["content_destination: CONTENT_DESTINATION_ANNOUNCEMENTS"] and len(now) == len(was) + 1 and len(was) > 200,
             "in the notices' file: sent to the announcements, it says so in one line more, and every other line is as it was",
             "; ".join(added)),
        said(back == before, "in the notices' file: sent back to the presentation layers, it is the file it was, byte for byte"),
        said(len(ours_was) == 3 and not any("content_destination" in lines for lines in ours_was),
             "in the playlists' file, before: the test's three rows, none of which says where it is sent"),
        said(newly == ["Notices.pro", "Notices.pro"] and len(rows(lists_now)) == len(rows(lists_was)),
             "in the playlists' file: both rows that are the notices say they are sent to the announcements, and no other row is changed",
             ", ".join(newly)),
        said(changed == ["+content_destination: CONTENT_DESTINATION_ANNOUNCEMENTS"] * 2 and len(lists_was) > 1000,
             "in the playlists' file: and nothing else of it was changed: two lines more, and no line gone or altered", "; ".join(changed)),
        # (Every layer is what a clear means that names none, so the file has no line for it.)
        said(len(clears) == 1 and "target_layer" not in clears[0] and "content_destination: CONTENT_DESTINATION_ANNOUNCEMENTS" in clears[0],
             "in the other presentation's file: the action that clears the announcements is ProPresenter's: every layer, of the announcements",
             " ".join(clears[0].split()) if clears else "no clear"),
    ]


def prepare_videoinputs(workspace, test_dir):
    """What the workspace's file was before the test (videoinputs-workspace-before), and every presentation of its
    library (videoinputs-pro-before/), for after_videoinputs to compare what the test changed with."""
    shutil.copy(os.path.join(workspace, "Configuration", "Workspace"), os.path.join(test_dir, "videoinputs-workspace-before"))
    os.makedirs(os.path.join(test_dir, "videoinputs-pro-before"))
    for path in glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", "*.pro")):
        shutil.copy(path, os.path.join(test_dir, "videoinputs-pro-before", os.path.basename(path)))


def after_videoinputs(workspace, test_dir):
    """What the files hold once the test has worked on them, read with protoc and not by the app. The workspace's file:
    ProPresenter's own input as it was, to the byte; the one added after it, by its name, with an id, ProPresenter's
    colour and straight alpha, and nothing of this computer's (its device is in the app's settings); and the rest of the
    file as it was but for the live look, which the test changed (ProPresenter keeps the look that is live in this
    file). The presentation the test changed: two Video Input Actions as ProPresenter writes them (a media action of
    live video on the input layer, by the input's place), and the fill that was a colour made a video input; and with
    those taken out, and the fill as it was, the presentation as it was. Every other presentation to the byte."""
    def decoded(data, message="rv.data.Presentation", proto="presentation.proto"):
        return protoc("decode", message, proto, data).decode()

    def blocks(text, name):
        return [m.group(0) for m in re.finditer(r"^( *)" + name + r" \{\n.*?^\1\}\n", text, re.S | re.M)]

    def without(text, name):
        for block in blocks(text, name):
            text = text.replace(block, "")
        return text

    with open(os.path.join(test_dir, "videoinputs-workspace-before"), "rb") as source:
        was = decoded(source.read(), "rv.data.ProPresenterWorkspace", "proworkspace.proto")
    with open(os.path.join(workspace, "Configuration", "Workspace"), "rb") as source:
        now = decoded(source.read(), "rv.data.ProPresenterWorkspace", "proworkspace.proto")
    inputs_was, inputs_now = blocks(was, "videoInputs"), blocks(now, "videoInputs")
    added = inputs_now[1] if len(inputs_now) == 2 else ""
    rest_same = without(without(was, "videoInputs"), "live_audience_look") == without(without(now, "videoInputs"), "live_audience_look")
    # The presentations
    changed, same = [], 0
    for path in sorted(glob.glob(os.path.join(glob.escape(workspace), "Libraries", "*", "*.pro"))):
        with open(path, "rb") as source:
            data = source.read()
        with open(os.path.join(test_dir, "videoinputs-pro-before", os.path.basename(path)), "rb") as source:
            old = source.read()
        if data == old:
            same += 1
        else:
            changed.append((decoded(old), decoded(data)))
    text_was, text_now = changed[0] if len(changed) == 1 else ("", "")
    # (The new actions by their ids: a slide is itself held in an action of its cue, which the fill changed)
    def ident(action):
        return re.search(r'uuid \{\n\s*string: "([^"]*)"', action).group(1)

    had = {ident(a) for a in blocks(text_was, "actions")}
    made = [a for a in blocks(text_now, "actions") if ident(a) not in had]
    proper = [a for a in made if a.startswith("  actions {") and "type: ACTION_TYPE_MEDIA\n" in a and "layer_type: LAYER_TYPE_INPUT" in a
              and re.search(r"\n( *)live_video \{\n\1  live_video_index: 1\n", a) and "url {" not in a and re.search(r"\n *video \{", a) is None]
    fills_was, fills_now = blocks(text_was, "fill"), blocks(text_now, "fill")
    different = [i for i in range(min(len(fills_was), len(fills_now))) if fills_was[i] != fills_now[i]]
    fill = fills_now[different[0]] if len(different) == 1 and len(fills_was) == len(fills_now) else ""
    put_back = text_now
    for block in made:
        put_back = put_back.replace(block, "", 1)
    if fill:
        put_back = put_back.replace(fill, fills_was[different[0]], 1)
    return [
        said(len(inputs_was) == 1 and len(inputs_now) == 2 and inputs_now[0] == inputs_was[0],
             "ProPresenter's own input is first, as it was", "%d then %d" % (len(inputs_was), len(inputs_now))),
        said('user_description: "Camera Two"' in added and re.search(r'uuid \{\n\s*string: "[0-9A-F-]{36}"', added) is not None
             and "red: 0.0471" in added and "alpha_type: ALPHA_TYPE_STRAIGHT" in added,
             "the added one has its name, an id, ProPresenter's colour and straight alpha", added[:400]),
        said("video_input_device" not in added and "Test Pattern" not in now and "test:pattern" not in now,
             "and nothing of this computer's: which device an input is, is the app's to keep", added[:400]),
        said(rest_same, "the rest of the workspace's file is as it was, but for the live look the test chose"),
        said(len(changed) == 1 and same > 0, "one presentation was changed, and every other is as it was, to the byte", "%d changed, %d the same" % (len(changed), same)),
        said(len(made) == 2 and len(proper) == 2,
             "it has two Video Input Actions, as ProPresenter writes them: a media action of live video on the input layer, by the input's place",
             "\n".join(made)[:900]),
        said(fill != "" and "color {" in fills_was[different[0]] and "live_video {" in fill and "media {" in fill,
             "the fill that was a colour is a video input", fill[:400]),
        said(text_was != "" and put_back == text_was, "and with those taken out, and the fill as it was, the presentation is as it was"),
    ]


def prepare_red_picture(workspace, test_dir):
    """A picture of one colour, red and the shape of a screen, for a test to put on the media layer and look for
    (screenpreview, screens, decklink): in the test's own folder and NOT under the workspace's Media, which in a copy of a
    fixture is a link to a real one (see prepare_audio)."""
    folder = os.path.join(test_dir, "media")
    os.makedirs(folder)
    path = os.path.join(folder, "Red.png")
    if not os.path.realpath(path).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("a test's picture was about to be written outside the test's folder: %s" % os.path.realpath(path))
    with open(path, "wb") as out:
        out.write(tiny_png((255, 0, 0), 320, 180))


def quarters_png(width, height, colours):
    """A picture of four colours, one in each quarter (top left, top right, bottom left, bottom right), as the bytes of
    a PNG file."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    rows = b""
    for y in range(height):
        top = y < height // 2
        left, right = (colours[0], colours[1]) if top else (colours[2], colours[3])
        rows += b"\x00" + bytes(left) * (width // 2) + bytes(right) * (width - width // 2)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def prepare_quarters_picture(workspace, test_dir):
    """A picture the shape of a screen in four colours, red, green, blue and white from the top left, for the Media
    Inspector's test to turn, flip, crop and give effects to, and to tell which way it has gone by the colours: in the
    test's own folder and NOT under the workspace's Media (see prepare_red_picture)."""
    folder = os.path.join(test_dir, "media")
    os.makedirs(folder)
    path = os.path.join(folder, "Quarters.png")
    if not os.path.realpath(path).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("a test's picture was about to be written outside the test's folder: %s" % os.path.realpath(path))
    with open(path, "wb") as out:
        out.write(quarters_png(320, 180, [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]))


def prepare_messages(workspace, test_dir):
    """The workspace is given a file of messages as ProPresenter might have left one (MESSAGES_FILE). Nothing is
    written but that one file, in the copy's Configuration."""
    target = os.path.join(workspace, "Configuration", "Messages")
    if not os.path.realpath(target).startswith(os.path.realpath(test_dir) + os.sep):
        raise RuntimeError("the messages test would write outside its own folder: " + os.path.realpath(target))
    with open(target, "wb") as out:
        out.write(protoc("encode", "rv.data.MessageDocument", "messages.proto", MESSAGES_FILE.encode()))
    shutil.copy(target, os.path.join(test_dir, "messages-before"))


def after_messages(workspace, test_dir):
    """What the messages left in their file, read with protoc and not by the app, set against the file the test was
    given (prepare_messages); and the link to the stage message that the test made in the stage layouts' file, set
    against the fixture's."""
    import difflib

    def decoded(path, message="rv.data.MessageDocument", proto="messages.proto"):
        with open(path, "rb") as source:
            return protoc("decode", message, proto, source.read()).decode()

    def blocks(text):
        """Each message of a decoded file, by its title: its lines of the text."""
        found = {}
        for block in re.finditer(r"^messages \{\n.*?^\}\n", text, re.S | re.M):
            found[re.search(r'^  title: "([^"]*)"', block.group(0), re.M).group(1)] = block.group(0)
        return found

    was = decoded(os.path.join(test_dir, "messages-before"))
    now = decoded(os.path.join(workspace, "Configuration", "Messages"))
    before, after = blocks(was), blocks(now)
    made = after.get("Parent Wanted", "")
    tokens = re.findall(r'tokens \{\n    uuid \{\n      string: "([^"]*)"\n    \}\n    (\w+) \{', made)
    text = re.search(r'^  message_text: "(.*)"$', made, re.M)
    lines = [
        said(now.split("messages {")[0] == was.split("messages {")[0] and "application_info" in now,
             "in the messages' file: what the file says of the program that wrote it is as it was"),
        said(after.get("Service Countdown") == before.get("Service Countdown") and before.get("Service Countdown") is not None,
             "in the messages' file: ProPresenter's own message, which was shown and never changed, is as it was, line for line"),
        said("New Message" not in after and list(after) == ["Service Countdown", "Parent Wanted"],
             "in the messages' file: the message that was removed is gone, and the one that was made is last", " | ".join(after)),
        said(len(tokens) == 2 and [kind for _, kind in tokens] == ["text", "clock"] and text is not None
             and text.group(1) == "Would ${%s} come to the  at ${%s}. ${%s}!" % (tokens[0][0], tokens[1][0], tokens[0][0]),
             "in the messages' file: its text has each token standing in it as ${its id}, the one that was deleted gone",
             (text.group(1) if text else "no text") + " / " + " ".join(kind for _, kind in tokens)),
        said(made.count("token_values {") == 2 and 'value: "Kim Wu"' in made and 'token_name: "Name"' in made and "Row 12" not in made
             and "date_type" not in made and "time_format: DATE_FORMATTER_STYLE_SHORT" in made,
             "in the messages' file: what the name was last given is with it, what the deleted token was given is gone, and the clock is the time alone, the short way"),
        said('template {\n    name: "New Life Chapel"' in made and "slide_uuid {" in made and "slide_name: " in made,
             "in the messages' file: its theme is named as ProPresenter names one, by the theme's name and the slide's id and name"),
        said("clear_type" not in made and "time_to_remove: 5" in made,
             "in the messages' file: set to go by hand again, it says nothing of how it goes, and keeps the five seconds it had"),
    ]
    stage_was = decoded(os.path.join(FIXTURES, "workspaces", "Act", "Configuration", "Stage"), "rv.data.Stage.Document", "stage.proto")
    stage_now = decoded(os.path.join(workspace, "Configuration", "Stage"), "rv.data.Stage.Document", "stage.proto")
    out = [line[1:].strip() for line in difflib.unified_diff(stage_was.splitlines(), stage_now.splitlines(), lineterm="", n=0)
           if line.startswith("-") and not line.startswith("---")]
    put = "\n".join(line[1:] for line in difflib.unified_diff(stage_was.splitlines(), stage_now.splitlines(), lineterm="", n=0)
                    if line.startswith("+") and not line.startswith("+++"))
    lines.append(said(out == [] and put.count("stage_message {") == 1 and "flash_color {" in put and "should_flash" not in put,
                      "in the stage layouts' file: the box that was made is linked to the stage message, with its colour and no flash, and nothing that was there is gone",
                      "%d lines out, %d stage messages in" % (len(out), put.count("stage_message {"))))
    return lines


def after_order(workspace, test_dir):
    """An element dragged to another place in the editor's list of a slide's elements: what the song's file holds
    after, read with protoc and not by the app. The test kept the file as it was before the drag (order-before) and
    after it (order-moved)."""
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(name):
        with open(os.path.join(frames, name), "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def elements(text):
        """Every element of every slide of a decoded presentation, in the file's order: its lines of the text."""
        return [block.group(0) for block in re.finditer(r"^( +)elements \{\n.*?^\1\}\n", text, re.S | re.M)]

    def named(blocks):
        """The names of the three the test made, in the order those blocks have them."""
        return [name for block in blocks for name in re.findall(r'^ +name: "(Red|Green|Blue)"$', block, re.M)]

    was, now = decoded("order-before"), decoded("order-moved")
    before, after = elements(was), elements(now)
    rest = lambda text: re.sub(r"^( +)elements \{\n.*?^\1\}\n", "", text, flags=re.S | re.M)
    return [
        said(named(before) == ["Red", "Green", "Blue"] and named(after) == ["Blue", "Red", "Green"],
             "in the song's file: the element that was dragged under the other two is before them in the order drawn",
             ",".join(named(before)) + " then " + ",".join(named(after))),
        said(len(before) == len(after) and len(before) > 3 and sorted(before) == sorted(after) and before != after,
             "in the song's file: every element of every slide is there as it was, line for line, and only the order is other",
             "%d elements, %d as they were" % (len(before), len(set(before) & set(after)))),
        said(rest(was) == rest(now) and len(rest(was)) > 200,
             "in the song's file: nothing but the elements' order is changed"),
    ]


def after_arrange(workspace, test_dir):
    """Arrangements made and changed in the editor over the slides: what the song's file holds at each step, read
    with protoc and not by the app. The test kept the file as it was given it (arrange-before), with the new
    arrangement (arrange-new), with that arrangement changed (arrange-edited) and at the end (arrange-end)."""
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(name):
        with open(os.path.join(frames, name), "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def arrangements(text):
        """Each arrangement of a decoded presentation, in the file's order: its name, its id, the ids of the groups
        it goes through, and its lines of the text."""
        found = []
        for block in re.finditer(r"^arrangements \{\n.*?^\}\n", text, re.S | re.M):
            lines = block.group(0)
            found.append({"name": re.search(r'^  name: "([^"]*)"', lines, re.M).group(1),
                          "id": re.search(r'^  uuid \{\s+string: "([^"]+)"', lines, re.M).group(1),
                          "groups": re.findall(r'^  group_identifiers \{\s+string: "([^"]+)"', lines, re.M), "lines": lines})
        return found

    def groups(text):
        """The ids of the presentation's groups, in the order it has them: Master."""
        return [re.search(r'uuid \{\s+string: "([^"]+)"', block.group(0)).group(1) for block in re.finditer(r"^cue_groups \{\n.*?^\}\n", text, re.S | re.M)]

    def selected(text):
        found = re.search(r'^selected_arrangement \{\s+string: "([^"]+)"', text, re.M)
        return found.group(1) if found else ""

    def rest(text):
        """A decoded presentation without its arrangements and which of them is selected."""
        return re.sub(r"^(arrangements|selected_arrangement) \{\n.*?^\}\n", "", text, flags=re.S | re.M)

    before, new, edited, end = (decoded(name) for name in ("arrange-before", "arrange-new", "arrange-edited", "arrange-end"))
    was, made, changed, left = (arrangements(text) for text in (before, new, edited, end))
    master = groups(before)
    lines = [
        said([a["name"] for a in made] == ["Default", "New Arrangement"] and made[1]["groups"] == master and len(master) == 7
             and len(made[1]["id"]) == 36 and made[1]["id"] != made[0]["id"],
             "in the song's file: the new arrangement is the last, with an id of its own, and goes through every group once in the order the file has them",
             " ".join(a["name"] for a in made)),
        said(selected(before) == was[0]["id"] and selected(new) == made[1]["id"],
             "in the song's file: the new arrangement is the one selected, where ProPresenter's own was before"),
        said(made[0]["lines"] == was[0]["lines"] and rest(new) == rest(before) and len(rest(before)) > 1000,
             "in the song's file: ProPresenter's own arrangement, the groups, the slides and all the rest are as they were, line for line"),
        said(len(changed) == 2 and changed[1]["groups"] == [master[1], master[0]] + master[2:] and changed[1]["id"] == made[1]["id"],
             "in the song's file: after a group was taken out, put in at the end and moved to the front, the arrangement goes through the groups in that order",
             "%d places" % (len(changed[1]["groups"]) if len(changed) == 2 else -1)),
        said(len(changed) == 2 and changed[0]["lines"] == was[0]["lines"] and rest(edited) == rest(before),
             "in the song's file: and nothing else was touched by that"),
        said(len(left) == 1 and left[0]["lines"] == was[0]["lines"] and selected(end) == "" and rest(end) == rest(before),
             "in the song's file: with the copy and the new arrangement removed, ProPresenter's own is there as it was, none is selected, and the rest is as it was",
             " ".join(a["name"] for a in left) + " / selected " + (selected(end) or "none")),
    ]
    return lines


def after_reflow(workspace, test_dir):
    """A slide split in two and slides joined in the reflow editor: what the presentation's file holds, read with
    protoc and not by the app. The test kept the file with its one slide (reflow-before), after the first split
    (reflow-split) and at the end (reflow-end). The words are the test's own."""
    frames = os.path.join(os.path.dirname(test_dir), "frames", os.path.basename(test_dir))

    def decoded(name):
        with open(os.path.join(frames, name), "rb") as source:
            return protoc("decode", "rv.data.Presentation", "presentation.proto", source.read()).decode()

    def cues(text):
        """Each cue of a decoded presentation, in the file's order: its id, the ids of its elements, how many
        actions it has, and its lines of the text."""
        found = []
        for block in re.finditer(r"^cues \{\n.*?^\}\n", text, re.S | re.M):
            lines = block.group(0)
            found.append({"id": re.search(r'^  uuid \{\s+string: "([^"]+)"', lines, re.M).group(1),
                          "elements": re.findall(r'^ +elements \{\n +element \{\n +uuid \{\s+string: "([^"]+)"', lines, re.M),
                          "actions": len(re.findall(r"^  actions \{$", lines, re.M)), "lines": lines})
        return found

    def grouped(text):
        """The ids of the cues the presentation's groups list, in order."""
        return [cue for block in re.finditer(r"^cue_groups \{\n.*?^\}\n", text, re.S | re.M)
                for cue in re.findall(r'^  cue_identifiers \{\s+string: "([^"]+)"', block.group(0), re.M)]

    def rest(text):
        """A decoded presentation without its cues and its groups' lists of them."""
        text = re.sub(r"^cues \{\n.*?^\}\n", "", text, flags=re.S | re.M)
        return re.sub(r"^  cue_identifiers \{\n.*?^  \}\n", "", text, flags=re.S | re.M)

    before, split, end = (decoded(name) for name in ("reflow-before", "reflow-split", "reflow-end"))
    one, two, last = cues(before), cues(split), cues(end)
    made = two[1] if len(two) == 2 else {"id": "", "elements": [], "actions": 0, "lines": ""}
    return [
        said(len(one) == 1 and len(two) == 2 and two[0]["id"] == one[0]["id"] and made["id"] != one[0]["id"] and len(made["id"]) == 36,
             "in the presentation's file: a split made a second cue straight after the first, with an id of its own", "%d then %d cues" % (len(one), len(two))),
        said(grouped(split) == [cue["id"] for cue in two] and grouped(before) == [one[0]["id"]],
             "in the presentation's file: the group that had the slide lists the new one straight after it"),
        said(len(made["elements"]) == len(one[0]["elements"]) == 2 and not set(made["elements"]) & set(one[0]["elements"])
             and two[0]["elements"] == one[0]["elements"] and made["actions"] == 1,
             "in the presentation's file: the new slide has the same elements, each with an id of its own, and its cue does nothing but show it",
             "%d elements, %d actions" % (len(made["elements"]), made["actions"])),
        said("Bright morning" in two[0]["lines"] and "over the hill" in two[0]["lines"] and "Quiet evening" not in two[0]["lines"]
             and "Quiet evening" in made["lines"] and "under the stars" in made["lines"] and "Bright morning" not in made["lines"],
             "in the presentation's file: each of the two slides has its own half of the words, and not the other's"),
        said(rest(split) == rest(before) and rest(end) == rest(before) and len(rest(before)) > 100,
             "in the presentation's file: nothing but the slides and the group's list of them is changed, by the split or by the end"),
        said(len(last) == 2 and grouped(end) == [cue["id"] for cue in last] and last[0]["id"] == one[0]["id"]
             and "morning" not in last[0]["lines"].split("Bright", 1)[-1][:40] and "Home" in last[1]["lines"] and "Quieter evening" in last[1]["lines"],
             "in the presentation's file: after the joins and the last split there are two slides, the group lists both, and the words are where they were put",
             "%d cues" % len(last)),
    ]


def test(workspace, drawn=SHELL, timeout=170, env=None, prepare=None, after=None, also=(), monitors=1):
    return {"workspace": workspace, "drawn": drawn, "timeout": timeout, "env": env or {}, "prepare": prepare, "after": after, "also": also,
            "monitors": monitors}


# ---- Bible mode: a made-up Bible, and a stand-in for api.bible
#
# Nothing of any real Bible is in the tests. The verses are made up here, numbered so
# that a test can tell which is which, some longer than others so that they fill slides
# unevenly, and one with words in red. api.bible is played by a small server on this
# computer, which the test-hook build is pointed at (SIMPLEPRESENTER_API_BIBLE_URL, read
# only by such a build: see src/bibles.cpp), and which notes every request it is sent.


def made_up_verse(chapter, verse):
    words = "Made up words of verse %d of chapter %d, said for a test" % (verse, chapter)
    if verse % 3 == 0:
        words += "; and a few more words after them, so that this one runs longer than the others do"
    return words + "."


def made_up_bible():
    """A Zefania file: Genesis 1 (5 verses), John 3 and 4 (20 each), Jude 1 (3); John 3:16 has words in red."""
    books = [(1, "Genesis", {1: 5}), (43, "John", {3: 20, 4: 20}), (65, "Jude", {1: 3})]
    out = ['<?xml version="1.0" encoding="UTF-8"?>', '<XMLBIBLE biblename="Made Up Version">',
           "<INFORMATION><identifier>MUV</identifier><rights>Made up for the tests: no Bible's words.</rights></INFORMATION>"]
    for number, name, chapters in books:
        out.append('<BIBLEBOOK bnumber="%d" bname="%s">' % (number, name))
        for chapter, count in chapters.items():
            out.append('<CHAPTER cnumber="%d">' % chapter)
            for verse in range(1, count + 1):
                words = made_up_verse(chapter, verse)
                if number == 43 and chapter == 3 and verse == 16:
                    words = 'Made up words, <STYLE css="color:#ff0000">said in red by a speaker</STYLE>, for a test.'
                out.append('<VERS vnumber="%d">%s</VERS>' % (verse, words))
            out.append("</CHAPTER>")
        out.append("</BIBLEBOOK>")
    out.append("</XMLBIBLE>")
    return "\n".join(out)


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


BIBLE_PORT = free_port()
BIBLE_KEY = "a-made-up-test-key"


class BibleStandIn:
    """api.bible, as far as the app asks it: the Bibles a key gives (one, made up), passages of it, and FUMS."""

    def __init__(self):
        import http.server
        import threading
        self.requests = []
        stand_in = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def answer(self, status, body):
                data = json.dumps(body).encode() if not isinstance(body, bytes) else body
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def do_GET(self):
                url = urllib.parse.urlparse(self.path)
                query = urllib.parse.parse_qs(url.query)
                stand_in.requests.append({"path": url.path, "query": query, "key": self.headers.get("api-key", "")})
                if url.path == "/f3":
                    return self.answer(200, b"")
                if self.headers.get("api-key", "") != BIBLE_KEY:
                    return self.answer(401, {"statusCode": 401, "error": "Unauthorized", "message": "Invalid API key"})
                if url.path == "/v1/bibles":
                    return self.answer(200, {"data": [{"id": "made-up-api", "name": "Made Up Online", "abbreviation": "MUO",
                                                        "abbreviationLocal": "MUO", "language": {"name": "English"}}]})
                parts = url.path.split("/")
                if len(parts) == 6 and parts[1:4] == ["v1", "bibles", "made-up-api"] and parts[4] == "passages":
                    return self.passage(parts[5])
                return self.answer(404, {"statusCode": 404, "message": "Not found"})

            def passage(self, passage_id):
                # JHN.3.16, JHN.3.16-JHN.3.18, or JHN.3 (a chapter: 20 verses each)
                verses = []
                first = passage_id.split("-")[0].split(".")
                if len(first) == 2:
                    verses = [(int(first[1]), v) for v in range(1, 21)]
                else:
                    last = passage_id.split("-")[-1].split(".")
                    verses = [(int(first[1]), v) for v in range(int(first[2]), int(last[2]) + 1) if v <= 20]
                if first[0] != "JHN" or not verses:
                    return self.answer(404, {"statusCode": 404, "message": "No such passage"})
                items = []
                for chapter, verse in verses:
                    vid = "JHN.%d.%d" % (chapter, verse)
                    items.append({"name": "verse", "type": "tag", "attrs": {"number": str(verse), "style": "v"}, "items": [{"text": str(verse), "type": "text"}]})
                    if verse == 16:
                        items.append({"text": "Online words, ", "type": "text", "attrs": {"verseId": vid}})
                        items.append({"name": "char", "type": "tag", "attrs": {"style": "wj"}, "items": [{"text": "said in red online", "type": "text", "attrs": {"verseId": vid}}]})
                        items.append({"text": ", for a test.", "type": "text", "attrs": {"verseId": vid}})
                    else:
                        items.append({"text": "Online " + made_up_verse(chapter, verse), "type": "text", "attrs": {"verseId": vid}})
                return self.answer(200, {"data": {"id": passage_id, "bibleId": "made-up-api", "copyright": "Made up online for the tests.",
                                                  "content": [{"name": "para", "type": "tag", "attrs": {"style": "p"}, "items": items}]},
                                         "meta": {"fumsToken": "token-" + passage_id}})

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", BIBLE_PORT), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()


BIBLE_STAND_IN = None


def prepare_bible(workspace, test_dir):
    """The made-up Bible, in the test's own copy of the workspace, to be imported from there; and the stand-in for
    api.bible, started."""
    global BIBLE_STAND_IN
    with open(os.path.join(workspace, "Made Up Bible.xml"), "w", encoding="utf-8") as out:
        out.write(made_up_bible())
    BIBLE_STAND_IN = BibleStandIn()


def prepare_bible_file(workspace, test_dir):
    """The made-up Bible alone, in the test's own copy of the workspace."""
    with open(os.path.join(workspace, "Made Up Bible.xml"), "w", encoding="utf-8") as out:
        out.write(made_up_bible())


def after_bible(workspace, test_dir):
    """What the stand-in for api.bible was asked: every request with the key, but FUMS's, which has none; and FUMS told
    of what was shown, with a device's id and a session's."""
    stand_in = BIBLE_STAND_IN
    stand_in.stop()
    asked = [r for r in stand_in.requests if r["path"] != "/f3"]
    told = [r for r in stand_in.requests if r["path"] == "/f3"]
    tokens = [t for r in told for t in r["query"].get("t", [])]
    passages = [r for r in asked if "/passages/" in r["path"]]
    return [
        # (One request is made with a wrong key on purpose, to see what is said of it.)
        said(len(asked) >= 3 and all(r["key"] in (BIBLE_KEY, "a-wrong-key") for r in asked) and sum(r["key"] == "a-wrong-key" for r in asked) == 1,
             "api.bible was asked for its Bibles and for passages, with the key in its header",
             "%d asked" % len(asked)),
        said(all(r["query"].get("content-type") == ["json"] and r["query"].get("include-titles") == ["false"] and r["query"].get("fums-version") == ["3"]
                 for r in passages) and len(passages) >= 1,
             "each passage asked for as JSON, without headings, with FUMS's token", ", ".join(r["path"] for r in passages)),
        said(len(told) >= 1 and all(r["key"] == "" for r in told) and any(t.startswith("token-JHN") for t in tokens)
             and all(r["query"].get("dId") and r["query"].get("sId") for r in told),
             "FUMS told what was shown, with no key, and with a device's id and a session's",
             "%d told: %s" % (len(told), ", ".join(tokens))),
        # (The script asks for John 3:16 and then Next Verse: the passage grows to 3:16-17, and only 3:17 is asked for. No
        # other step asks for 3:17 by itself.)
        said("JHN.3.17" in [r["path"].rsplit("/", 1)[-1] for r in passages],
             "Next Verse asked api.bible for the verse it adds, and for no more", ", ".join(r["path"].rsplit("/", 1)[-1] for r in passages)),
    ]


TESTS = {
    "media": test("Demo"),
    "transport": test("Demo"),
    "timers": test("Demo"),
    "link": test("ProPresenter MR"),
    # (Settings remembered, in the test's own folder of them: the Color Triggers kept as the default go there.)
    "triggers": test("ProPresenter MR", env={"SP_TEST_REMEMBER": "1"}),
    "rapid": test("Demo"),
    "chrome": test("Demo"),
    "arrange": test("Demo", timeout=240, after=after_arrange),
    "reflow": test("Demo", timeout=240, after=after_reflow),
    "announce": test("Demo", timeout=240, prepare=prepare_announce, after=after_announce),
    "advance": test("Demo", timeout=300, prepare=prepare_advance, after=after_advance),
    "masks": test("Demo", timeout=300, prepare=prepare_masks, after=after_masks),
    "switch": test("Demo", MEMORY, 60, also=("ProPresenter MR",)),
    "smoke": test("Demo", MEMORY, 120, {"SP_TEST_WINDOWS": "0"}),
    "edit": test("Demo", MEMORY, 120, {"SP_TEST_WINDOWS": "0", "SP_TEST_WIDTH": "1200", "SP_TEST_HEIGHT": "660"}),
    "props": test("Demo"),
    "stage": test("ProPresenter MR"),
    "actions": test("Demo", SHELL, 120, prepare=prepare_actions),
    "log": test("Demo"),
    "sound": test("Demo"),
    "audio": test("Act", timeout=300, prepare=prepare_audio, after=after_audio),
    "notes": test("Act", timeout=300, prepare=prepare_notes, after=after_notes),
    "messages": test("Act", timeout=300, prepare=prepare_messages, after=after_messages),
    "screenpreview": test("Act", timeout=240, prepare=prepare_red_picture),
    # (With the pretend device the app has for tests: there is no camera here, or none that may be looked at.)
    "videoinputs": test("Act", timeout=240, env={"SP_TEST_VIDEOINPUT": "1"}, prepare=prepare_videoinputs, after=after_videoinputs),
    "textstyle": test("Act", timeout=240),
    # (With the two Blackmagic outputs the app pretends for tests: there is no card here.)
    "decklink": test("Demo", env={"SP_TEST_DECKLINK": "1"}, prepare=prepare_red_picture),
    "httpapi": test("Demo"),
    "quickedit": test("Act"),
    "builds": test("Demo", timeout=300, prepare=prepare_builds, after=after_builds),
    "themebuilds": test("Act", timeout=240),
    "devices": test("Act", timeout=300, env={"SP_TEST_REMEMBER": "1"}, prepare=prepare_devices, after=after_devices),
    "playacts": test("Act", timeout=300, prepare=prepare_playacts, after=after_playacts),
    "transports": test("Act", timeout=300, prepare=prepare_transports, after=after_transports),
    "markers": test("Act", timeout=360, prepare=prepare_markers, after=after_markers),
    "copyright": test("Act", timeout=300, prepare=prepare_copyright, after=after_copyright),
    "calendar": test("Act", timeout=300, prepare=prepare_calendar, after=after_calendar),
    "disabled": test("Demo", timeout=240, prepare=prepare_disabled, after=after_disabled),
    "scrolling": test("Demo", timeout=300, prepare=prepare_scrolling, after=after_scrolling),
    "mediainspector": test("Demo", timeout=300, prepare=prepare_quarters_picture),
    "drop": test("Demo"),
    "simple": test("Demo"),
    "shapes": test("Shapes", SHELL, 120),
    "order": test("Shapes", SHELL, 120, after=after_order),
    "slides": test("Shapes", SHELL, 120),
    "keys": test("Shapes", SHELL, 120, {"SP_TEST_REMEMBER": "1"}, lambda w, d: prepare_keys(w, d, True), after_keys),
    "keys2": test("Shapes", SHELL, 60, {"SP_TEST_REMEMBER": "1"}, lambda w, d: prepare_keys(w, d, False), after_keys2),
    "number": test("Shapes", SHELL, 120),
    "acts": test("Act", after=after_acts),
    "refine": test("Act"),
    "search": test("Demo"),
    "library": test("Demo", prepare=prepare_library, after=after_library),
    "slidemenu": test("Act", timeout=240, prepare=prepare_slidemenu, after=after_slidemenu),
    "cleargroups": test("Act", timeout=240, prepare=prepare_cleargroups, after=after_cleargroups),
    # (With its settings read and saved, as a run of the app's own has them: the tests are of a setting.)
    "softdecode": test("Demo", env={"SP_TEST_REMEMBER": "1"}, prepare=prepare_softdecode, after=after_softdecode),
    # (On a system made up for the guard to look at: an NVIDIA chip on nouveau, without its decoder's firmware.)
    "decodeguard": test("Demo", env={"SP_TEST_REMEMBER": "1", "SP_TEST_SYSTEM_ROOT": "<test>/system", "SP_TEST_NOUVEAU_CHIPSET": "e4"},
                        prepare=prepare_decodeguard, after=after_decodeguard),
    "decodeguard2": test("Demo", env={"SP_TEST_REMEMBER": "1", "SP_TEST_SYSTEM_ROOT": "<test>/system", "SP_TEST_NOUVEAU_CHIPSET": "e4"},
                         prepare=prepare_decodeguard2),
    "looks": test("Act", after=after_looks),
    "themes": test("Act"),
    "chords": test("ProPresenter MR", prepare=prepare_chords, after=after_chords),
    "chords2": test("ProPresenter MR", timeout=240, prepare=prepare_chords),
    "chords3": test("ProPresenter MR", timeout=240, prepare=prepare_chords, after=after_chords3),
    "chords4": test("ProPresenter MR", timeout=240, prepare=prepare_chords),
    "chords5": test("ProPresenter MR", timeout=240, prepare=prepare_chords),
    # (Settings remembered, in the test's own folder of them: to see that Bible mode's slides are not remembered as
    # the presentation that was open.)
    "bible": test("Demo", timeout=300, env={"SIMPLEPRESENTER_API_BIBLE_URL": "http://127.0.0.1:%d" % BIBLE_PORT, "SP_TEST_REMEMBER": "1"},
                  prepare=prepare_bible, after=after_bible),
    # (A theme of ProPresenter's whose Bible slide's verse box is Single Line: the Act workspace's New Life Chapel.)
    "bible2": test("Act", timeout=200, prepare=prepare_bible_file),
    # (On a desktop of its own, with two displays to send screens to.)
    "screens": test("Demo", prepare=prepare_red_picture, after=after_screens, monitors=2),
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
    # (A value may name the test's own folder, as "<test>": a made-up system it is to be shown, say.)
    env.update({name: value.replace("<test>", test_dir) for name, value in recipe["env"].items()})
    command = [binary, "--workspace", workspace, "--script", script, "--frames", frames]
    try:
        done = subprocess.run(command, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=recipe["timeout"])
        output = done.stdout.decode(errors="replace")
    except subprocess.TimeoutExpired as late:
        # (A failure, whether the script had ended or not: an app that does not quit when it is told to is one, as
        # the Calendar's window once kept it running with no windows)
        output = (late.stdout or b"").decode(errors="replace") + "\n  FAILED  TIMED OUT after %d seconds: the app had not quit\n" % recipe["timeout"]
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
