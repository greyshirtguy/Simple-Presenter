import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A slide's menu, for which group a slide is in: ProPresenter's way, which is the slide and those after it in its
// group (the whole group from its first slide, a split from a later one). What the file then holds is looked at when
// the test is over, by after_slidemenu in run.py.
QtObject {
    id: t

//COMMON
    // The runs of slides that are one group after another: [{ name, from, count }]
    function runs(slides) {
        const out = []
        slides.forEach((slide, i) => {
            if (i === 0 || slide.groupStart || slide.group !== slides[i - 1].group)
                out.push({ name: slide.group, from: i, count: 1 })
            else
                ++out[out.length - 1].count
        })
        return out
    }

    function said(slides) {
        return runs(slides).map(run => (run.name === "" ? "-" : run.name) + "x" + run.count).join(" ")
    }

    // A song for this: a group of three slides or more and another of two or more, both named, shown in the order
    // its groups are kept in, and with arrangements to be brought along
    function suits(entry) {
        if (!openable(entry) || entry.arrangements.length === 0 || entry.arrangement !== "")
            return false
        const named = runs(catalog.open(entry.path).slides).filter(run => run.name !== "")
        return named.some(run => run.count >= 3) && named.filter(run => run.count >= 2).length >= 2
               && new Set(named.map(run => run.name)).size === named.length
    }

    function cellOf(index) {
        const grid = named("slideGrid")
        grid.positionViewAtIndex(index, GridView.Contain)
        return grid.itemAtIndex(index)
    }

    // The colour of a slide's caption as the window draws it: a point at the far end of it from its words
    function captionColor(index) {
        const cell = cellOf(index)
        const at = cell.mapToItem(null, cell.width - 13, cell.height - 6 - 13)
        return String(testInput.windowPixel(at.x, at.y)).toLowerCase()
    }

    function same(a, b) {
        const part = (c, i) => parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(a, i) - part(b, i)) <= 3)
    }

    function menuItem(label) {
        return Lib.find(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === label)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries.find(library => library.name === "MainHall").path)
                return 500
            },
            () => {
                kept.entry = documents.find(suits)
                check("(the library has a song for this, and the workspace a list of groups)", kept.entry !== undefined && GroupKeys.groups.length > 5,
                      GroupKeys.groups.length + " groups in the list")
                openEntry(kept.entry)
                return 700
            },
            () => {
                const groups = runs(document.slides)
                kept.before = said(document.slides)
                kept.split = groups.find(run => run.name !== "" && run.count >= 3)
                kept.whole = groups.find(run => run.name !== "" && run.count >= 2 && run.from !== kept.split.from)
                // Two groups of the workspace's list that the song has none of, from near the top of the list, which
                // are drawn in other colours than the groups they are to take the place of. (A group is drawn in
                // the colour the settings here have for its name, or failing that in its own.)
                const shownAs = (name, color) => String(groupColor({ group: name, groupColor: color }))
                const unused = GroupKeys.groups.slice(0, 14).filter(group => !groups.some(run => run.name === group.name))
                kept.a = unused.find(group => !same(shownAs(group.name, group.color), shownAs(kept.split.name, document.slides[kept.split.from].groupColor)))
                kept.b = unused.find(group => group.name !== kept.a.name
                                              && !same(shownAs(group.name, group.color), shownAs(kept.whole.name, document.slides[kept.whole.from].groupColor)))
                kept.at = kept.split.from + 1
                check("(a group of three or more, another of two or more, and two of the list's groups to give them)", kept.a !== undefined && kept.b !== undefined,
                      kept.before)
                testInput.snapshot(document.path, "song-before")
                // The show is at a slide further on in the group that is to be split.
                goLive(kept.split.from + 2)
                kept.color = captionColor(kept.at)
                click(centre(cellOf(kept.at)), Qt.RightButton)
                return 400
            },
            () => {
                check("a slide's menu has Group", labels().includes("Theme, Group, "), labels())
                click(menuRow("Group"))
                return 500
            },
            () => {
                const current = document.slides[kept.at].group
                check("which lists the workspace's groups, in its order, the slide's own ticked",
                      labels() === "[Group], " + GroupKeys.groups.map(group => (group.name === current ? "*" : "") + group.name).join(", "), labels().substring(0, 200))
                const swatch = Lib.find(menuItem(kept.a.name), item => item.objectName === "menuSwatch")
                const at = swatch.mapToItem(null, swatch.width / 2, swatch.height / 2)
                const drawn = String(testInput.windowPixel(at.x, at.y))
                check("each with its colour drawn beside it", swatch.visible && same(drawn, kept.a.color), drawn + " for " + kept.a.color)
                testInput.grab("1-group-menu")
                click(menuRow(kept.a.name))
                return 700
            },
            () => {
                const slides = document.slides
                const end = kept.split.from + kept.split.count
                check("choosing one puts the slide and those after it in its group in that group, and no others: the group is split there",
                      slides.slice(kept.at, end).every(slide => slide.group === kept.a.name) && slides[kept.at].groupStart
                      && slides.slice(kept.split.from, kept.at).every(slide => slide.group === kept.split.name)
                      && (end >= slides.length || slides[end].group !== kept.a.name) && runs(slides).length === runs(catalog.open(document.path).slides).length
                      && said(slides) !== kept.before, said(slides))
                kept.afterSplit = runs(slides).length
                const caption = Lib.find(cellOf(kept.at), item => item.font !== undefined && String(item.text).includes(kept.a.name))
                check("the window draws it: the new group is named on its first slide, in the colour the slide's caption now has",
                      caption !== null && strokes(caption).light + strokes(caption).dark > 10 && same(captionColor(kept.at), String(groupColor(slides[kept.at])))
                      && same(captionColor(kept.at + 1), String(groupColor(slides[kept.at]))) && same(captionColor(kept.split.from), kept.color)
                      && !same(captionColor(kept.at), kept.color), captionColor(kept.at) + " was " + kept.color + ", the group's " + groupColor(slides[kept.at]))
                check("the show is where it was: the same slide is live", liveIndex === kept.split.from + 2 && viewingLive && !cleared)
                check("the log has a line for it", logged(/  edit +slide \d+ of ".*", and those after it in its group, put in the group "/).length === 1)
                testInput.grab("2-group-split")
                testInput.snapshot(document.path, "song-split")
                // The first slide of another group: the whole group
                const whole = runs(slides).find(run => run.name === kept.whole.name)
                kept.wholeFrom = whole.from
                kept.wholeCount = whole.count
                click(centre(cellOf(whole.from)), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Group"))
                return 500
            },
            () => {
                click(menuRow(kept.b.name))
                return 700
            },
            () => {
                const slides = document.slides
                check("from the first slide of a group, the whole group becomes the one chosen, and the song has as many groups as before",
                      slides.slice(kept.wholeFrom, kept.wholeFrom + kept.wholeCount).every(slide => slide.group === kept.b.name)
                      && runs(slides).length === kept.afterSplit && !slides.some(slide => slide.group === kept.whole.name), said(slides))
                check("drawn so, in its new colour", same(captionColor(kept.wholeFrom), String(groupColor(slides[kept.wholeFrom])))
                      && same(captionColor(kept.wholeFrom + kept.wholeCount - 1), String(groupColor(slides[kept.wholeFrom]))), captionColor(kept.wholeFrom))
                kept.hash = testInput.fileHash(document.path)
                // A slide that is in that group already
                click(centre(cellOf(kept.wholeFrom + 1)), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Group"))
                return 500
            },
            () => {
                check("(its own group is the one ticked)", labels().includes("*" + kept.b.name), labels().substring(0, 160))
                click(menuRow(kept.b.name))
                return 600
            },
            () => {
                check("choosing the group a slide is in already changes nothing: the file is not written", testInput.fileHash(document.path) === kept.hash
                      && logged(/put in the group/).length === 2)
            }
        ]
        next()
    }
}
