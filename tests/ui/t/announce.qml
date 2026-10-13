import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The announcements layer: a presentation sent there by its Destination, from the menu
// of its row and from the button over the slides, which is kept in its file and on its
// rows in the playlists; its slides shown over the slide that is live, which stays; its
// own media under them; stepping; the layer's clear button, Clear All and an action
// that clears it; and a look that keeps it from a screen. Throughout it looks at what
// is drawn on the output and in the operator's preview.
//
// The two presentations are the test's own (prepare_announce in run.py): "Notices",
// each slide a block of colour in the top right quarter, and "Plain", whose blocks are
// in the bottom left. Neither slide has a background, so each shows around the other.
QtObject {
    id: t

//COMMON
    // Three points of a screen, as shares of it: in a notice's block, in a block of
    // the other presentation, and where neither has anything
    readonly property point atNotice: Qt.point(0.78, 0.25)
    readonly property point atPlain: Qt.point(0.22, 0.75)
    readonly property point atFree: Qt.point(0.22, 0.25)

    // The colour at a point of the output, and at the same point of the preview the
    // operator has of it
    function out(p) {
        return String(testInput.pixel(false, p.x, p.y))
    }

    function seen(p) {
        const preview = named("previewSlide")
        const at = preview.mapToItem(null, preview.width * p.x, preview.height * p.y)
        return String(testInput.windowPixel(at.x, at.y))
    }

    function is(colour, wanted) {
        const part = (c, i) => parseInt(String(c).slice(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(colour, i) - part(wanted, i)) <= 40)
    }

    // Whether both have that colour there, and what they have, to say when they do not
    function both(p, wanted) {
        return is(out(p), wanted) && is(seen(p), wanted)
    }

    function drawn() {
        return "output " + out(atNotice) + " " + out(atPlain) + " " + out(atFree) + ", preview " + seen(atNotice) + " " + seen(atPlain) + " " + seen(atFree)
    }

    // A row of the list of presentations, brought into view: by the name it shows
    function row(name) {
        const list = named("presentationList")
        for (let i = 0; i < list.count; ++i) {
            if (list.model[i].name === name) {
                list.positionViewAtIndex(i, ListView.Contain)
                return list.itemAtIndex(i)
            }
        }
        return null
    }

    function open(name) {
        openEntry(entry(name))
    }

    // The colour a clear button is drawn in: red when its layer has something on it
    function buttonRed(name) {
        const button = named(name)
        const at = button.mapToItem(null, 4, button.height / 2)
        return is(testInput.windowPixel(at.x, at.y), "#c62828")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                kept.notices = entry("Notices").file
                kept.before = testInput.fileHash(kept.notices)
                // The notices are put in a playlist twice, and the other presentation once.
                const made = catalog.createPlaylist("Sunday", "")
                kept.playlist = made.id
                check("a playlist is made for them", made.error === "" && kept.playlist !== undefined, JSON.stringify(made))
                check("the notices go in it twice and the other once", catalog.addToPlaylist(kept.playlist, kept.notices) === ""
                      && catalog.addToPlaylist(kept.playlist, entry("Plain").file) === "" && catalog.addToPlaylist(kept.playlist, kept.notices) === "")
                testInput.snapshot(catalog.workspacePath + "/Playlists/Library", "announce-playlists")
                open("Plain")
                return 500
            },
            () => {
                check("every presentation is sent to the presentation layers until it says otherwise",
                      entry("Notices").destination === "" && entry("Plain").destination === "" && document.destination === "" && !viewedGoesToAnnouncements)
                check("there is a sixth clear button, for the announcements, with nothing to clear",
                      named("clearAnnouncements") !== null && !named("clearAnnouncements").live && !buttonRed("clearAnnouncements")
                      && named("clearAnnouncements").width > 20 && named("clearSlide").width === named("clearAnnouncements").width)
                goLive(0)
                return 700
            },
            () => {
                check("a slide of the other presentation is on the output and in the preview, and nothing is where a notice would be",
                      both(atPlain, "#ffff00") && both(atNotice, "#000000") && both(atFree, "#000000"), drawn())
                // The menu of the notices' row
                const item = row("Notices")
                check("the notices' row has its ordinary icon", item !== null && !item.announces && !Lib.find(item, i => i.objectName === "rowAnnounces").visible)
                click(centre(item), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on a presentation has its Destination: Presentation, which is ticked, and Announcements",
                      labels().indexOf("[Destination], *Presentation, Announcements") >= 0, labels())
                click(menuRow("Announcements"))
                return 600
            },
            () => {
                check("chosen, the presentation is sent to the announcements, and every list says so", entry("Notices").destination === "announcements"
                      && entry("Plain").destination === "" && catalog.playlistItems(kept.playlist).map(r => r.destination).join(",") === "announcements,,announcements",
                      catalog.playlistItems(kept.playlist).map(r => r.destination).join(","))
                const item = row("Notices")
                const mark = Lib.find(item, i => i.objectName === "rowAnnounces")
                const at = mark.mapToItem(null, mark.width / 2, mark.height / 2)
                check("its row has ProPresenter's picture of the announcements in place of its icon, in green", item.announces && mark.visible
                      && String(mark.ink) === "#3ddc68" && mark.status === Image.Ready, mark.status)
                check("which is drawn", (() => {
                    // Some point of the middle of the picture is green
                    for (let dx = -6; dx <= 6; dx += 2) {
                        if (is(testInput.windowPixel(at.x + dx, at.y), "#3ddc68"))
                            return true
                    }
                    return false
                })())
                check("the file says so", testInput.fileHash(kept.notices) !== kept.before)
                testInput.snapshot(kept.notices, "announce-set")
                check("the slide that is live is where it was, and nothing has gone to the announcements", both(atPlain, "#ffff00") && both(atNotice, "#000000")
                      && Show.liveIndex === 0 && !Show.announcementsOn, drawn())
                click(centre(row("Notices")), Qt.RightButton)
                return 400
            },
            () => {
                check("the menu now has Announcements ticked", labels().indexOf("[Destination], Presentation, *Announcements") >= 0, labels())
                click(menuRow("Presentation"))
                return 600
            },
            () => {
                check("sent back, the file is what it was to the byte", entry("Notices").destination === "" && testInput.fileHash(kept.notices) === kept.before)
                testInput.snapshot(kept.notices, "announce-back")
                open("Notices")
                return 500
            },
            () => {
                const button = named("destinationButton")
                const picture = Lib.find(button, i => i.name === "Announcements")
                check("over the slides is a button for the destination, grey for a presentation that goes to the presentation layers",
                      button !== null && button.visible && !button.announces && String(picture.ink) === "#8d9096")
                button.parent.parent.forceActiveFocus()
                click(centre(button))
                return 400
            },
            () => {
                check("it opens the same two choices", labels() === "[Destination], *Presentation, Announcements", labels())
                click(menuRow("Announcements"))
                return 600
            },
            () => {
                const button = named("destinationButton")
                const picture = Lib.find(button, i => i.name === "Announcements")
                const at = picture.mapToItem(null, picture.width / 2, picture.height / 2)
                check("chosen there, the presentation being viewed is sent to the announcements and the button is green", document.destination === "announcements"
                      && viewedGoesToAnnouncements && button.announces && String(picture.ink) === "#3ddc68" && entry("Notices").destination === "announcements")
                check("which is drawn", (() => {
                    for (let dx = -6; dx <= 6; dx += 2) {
                        if (is(testInput.windowPixel(at.x + dx, at.y), "#3ddc68"))
                            return true
                    }
                    return false
                })())
                check("the other presentation's slide is still the one that is live", Show.liveIndex === 0 && Show.liveKey === entry("Plain").path && !viewingLive)
                // A click on the first notice
                click(centre(slideCell(0)))
                return 800
            },
            () => {
                check("a notice goes to the announcements, and the show stays at the slide it was at", Show.announcementIndex === 0 && Show.announcementLive
                      && Show.announcementKey === entry("Notices").path && Show.liveIndex === 0 && Show.liveKey === entry("Plain").path && !cleared)
                check("both are drawn, the notice over the slide, on the output and in the operator's preview",
                      both(atNotice, "#ff0000") && both(atPlain, "#ffff00") && both(atFree, "#000000"), drawn())
                check("the notice is marked as the one that is on, in its presentation", slideCell(0).live && !slideCell(1).live && viewingAnnouncements)
                check("the announcements' clear button is red, and the slide's is too", named("clearAnnouncements").live && buttonRed("clearAnnouncements")
                      && buttonRed("clearSlide") && !buttonRed("clearMedia"))
                testInput.grabOutput("1-output-notice-over-slide")
                testInput.grab("1-operator-notice-over-slide")
                // The next, by the keyboard
                testInput.key(Qt.Key_Right)
                return 800
            },
            () => {
                check("the arrow steps the announcements, since theirs is the presentation being viewed", Show.announcementIndex === 1 && Show.liveIndex === 0
                      && both(atNotice, "#00ff00") && both(atPlain, "#ffff00") && slideCell(1).live && !slideCell(0).live, drawn())
                open("Plain")
                return 500
            },
            () => {
                check("in the other presentation its own slide is the one marked", viewingLive && !viewingAnnouncements && slideCell(0).live && !slideCell(1).live)
                click(centre(slideCell(1)))
                return 800
            },
            () => {
                check("a slide of it changes the slide layer and leaves the notice", Show.liveIndex === 1 && Show.announcementIndex === 1
                      && both(atPlain, "#00ffff") && both(atNotice, "#00ff00"), drawn())
                kept.screen = Screens.audience[0].id
                kept.gets = Looks.liveOf(kept.screen)
                check("the live look gives the screen the announcements", kept.gets.announcements === true, JSON.stringify(kept.gets))
                kept.lookError = Looks.setScreen(Looks.live.id, kept.screen, { announcements: false })
                return 900
            },
            () => {
                check("the live look can keep the announcements from a screen", kept.lookError === "" && Looks.liveOf(kept.screen).announcements === false,
                      kept.lookError)
                check("which takes them off that screen and out of the preview, and leaves the slide", both(atNotice, "#000000") && both(atPlain, "#00ffff")
                      && Show.announcementLive, drawn())
                check("they are still on, as far as the show goes, and their button is still red", Show.announcementsOn && buttonRed("clearAnnouncements"))
                kept.lookError = Looks.setScreen(Looks.live.id, kept.screen, { announcements: true })
                return 900
            },
            () => {
                check("given them again, the screen has the notice again", kept.lookError === "" && both(atNotice, "#00ff00") && both(atPlain, "#00ffff"), drawn())
                click(centre(named("clearAnnouncements")))
                return 800
            },
            () => {
                check("the announcements' clear button takes the notice off and leaves the slide", Show.announcementCleared && !Show.announcementsOn
                      && !cleared && both(atNotice, "#000000") && both(atPlain, "#00ffff"), drawn())
                check("and is grey again, with the slide's still red", !named("clearAnnouncements").live && !buttonRed("clearAnnouncements") && buttonRed("clearSlide"))
                open("Notices")
                return 500
            },
            () => {
                check("cleared, no notice is marked", !slideCell(0).live && !slideCell(1).live)
                // Media under a notice: a picture of the test's own, as a background
                kept.picture = catalog.workspacePath.replace(/\/[^\/]*\/[^\/]*$/, "") + "/magenta.png"
                check("the test's picture is there", testInput.exists(kept.picture), kept.picture)
                check("it is put on the third notice", report(catalog.setSlideMedia(document.path, document.slides[2].id, kept.picture, false)))
                reloadDocument()
                return 500
            },
            () => {
                check("the notice has its media", document.slides[2].media !== undefined && document.slides[2].media.path === kept.picture
                      && document.destination === "announcements", JSON.stringify(document.slides[2].media ?? null))
                testInput.key(Qt.Key_Right)
                return 800
            },
            () => {
                check("a step from where they were cleared brings back the notice they were at", Show.announcementIndex === 1 && Show.announcementLive
                      && both(atNotice, "#00ff00") && both(atPlain, "#00ffff"), drawn())
                click(centre(slideCell(2)))
                return 1500
            },
            () => {
                check("a notice with media has it under it on the announcements layer: over the slide that is live, which it covers",
                      Show.announcementIndex === 2 && both(atNotice, "#0000ff") && both(atFree, "#ff00ff") && both(atPlain, "#ff00ff"), drawn())
                check("it is the announcements' media, not the media layer's: the show has none, and the media's clear button is grey",
                      liveMedia === null && !Show.hasMedia && !buttonRed("clearMedia") && buttonRed("clearAnnouncements") && Show.liveIndex === 1 && !cleared)
                testInput.grabOutput("2-output-notice-with-media")
                testInput.grab("2-operator-notice-with-media")
                click(centre(slideCell(0)))
                return 1000
            },
            () => {
                check("a notice that brings none leaves that background under it", Show.announcementIndex === 0 && both(atNotice, "#ff0000") && both(atFree, "#ff00ff"),
                      drawn())
                click(centre(named("clearAnnouncements")))
                return 1000
            },
            () => {
                check("clearing the announcements takes their media with their slide, and the slide under them is there again",
                      !Show.announcementsOn && both(atNotice, "#000000") && both(atFree, "#000000") && both(atPlain, "#00ffff"), drawn())
                click(centre(slideCell(0)))
                return 800
            },
            () => {
                check("a notice shown after that has nothing under it", both(atNotice, "#ff0000") && both(atFree, "#000000") && both(atPlain, "#00ffff"), drawn())
                // F1: everything
                testInput.key(Qt.Key_F1)
                return 900
            },
            () => {
                check("Clear All clears the announcements with the rest", !Show.announcementsOn && cleared && both(atNotice, "#000000") && both(atPlain, "#000000")
                      && !buttonRed("clearAnnouncements") && !buttonRed("clearSlide"), drawn())
                // An action that clears the announcements, on the third slide of the other presentation
                open("Plain")
                return 500
            },
            () => {
                const items = addActionItems({ slide: 2 })
                const clears = items.find(i => i.label === "Clear").items
                check("the actions a slide can be given have one that clears the announcements",
                      clears.map(i => i.header ?? i.label).join(", ") === "Clear, Everything, The Slide, The Media, The Props, The Messages, The Announcements",
                      clears.map(i => i.header ?? i.label).join(", "))
                clears.find(i => i.label === "The Announcements").run()
                return 600
            },
            () => {
                const actions = document.slides[2].actions ?? []
                check("the slide has it", actions.length === 1 && actions[0].kind === "clear" && actions[0].announcements === true && actions[0].done === true,
                      JSON.stringify(actions))
                open("Notices")
                return 400
            },
            () => {
                click(centre(slideCell(1)))
                return 700
            },
            () => {
                check("a notice is on again", both(atNotice, "#00ff00") && Show.announcementLive, drawn())
                open("Plain")
                return 400
            },
            () => {
                click(centre(slideCell(2)))
                return 900
            },
            () => {
                check("the slide with that action clears the announcements as it goes live, and is itself shown", Show.liveIndex === 2 && !cleared
                      && !Show.announcementsOn && both(atPlain, "#ffffff") && both(atNotice, "#000000"), drawn())
                check("nothing was reported", notice === "", notice)
            },
        ]
        next()
    }
}
