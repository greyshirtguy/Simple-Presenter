import QtQuick
import SimplePresenterApp

Item {
    id: holder

    function run() {
        smokeTest()
    }

    // TEMPORARY: a scripted run of the operator window with the mouse and the keyboard.
    property var smokeSteps: []
    property int smokeStep: 0
    property int smokeFailures: 0

    function smokeCheck(what, ok, detail) {
        if (!ok)
            ++smokeFailures
        testInput.say((ok ? "  ok      " : "  FAILED  ") + what + (detail !== undefined ? "   [" + detail + "]" : ""))
    }

    // The first item under `root` for which `test` holds, looking through everything.
    function smokeFind(root, test) {
        const queue = [root]
        while (queue.length > 0) {
            const item = queue.shift()
            if (item !== root && test(item))
                return item
            for (let i = 0; i < item.children.length; ++i)
                queue.push(item.children[i])
        }
        return null
    }

    function smokeTest() {
        const named = name => smokeFind(win.contentItem, item => item.objectName === name)
        const centre = item => item.mapToItem(null, item.width / 2, item.height / 2)
        const click = (p, button, modifiers) => {
            testInput.mouse(0, p.x, p.y, modifiers ?? 0, button ?? Qt.LeftButton)
            testInput.mouse(2, p.x, p.y, modifiers ?? 0, button ?? Qt.LeftButton)
        }
        const drag = (from, to) => {
            testInput.mouse(0, from.x, from.y)
            for (let i = 1; i <= 10; ++i)
                testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
            testInput.mouse(2, to.x, to.y)
        }
        // The row of a list showing the entry for which `test` holds.
        const row = (listName, test) => {
            const list = named(listName)
            const index = list.model.findIndex(test)
            if (index < 0)
                return null
            list.positionViewAtIndex(index, ListView.Contain)
            return list.itemAtIndex(index)
        }
        const cell = (gridName, index) => {
            const grid = named(gridName)
            grid.positionViewAtIndex(index, GridView.Contain)
            return grid.itemAtIndex(index)
        }
        const menuRow = label => {
            const found = smokeFind(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === label)
            return found ? centre(found) : null
        }
        const labels = () => menu.items.map(i => i.header !== undefined ? "[" + i.header + "]" : i.label ?? "(note)").join(", ")
        // A scripted press is taken by the toolbar's drag handler (it has no history of
        // where the pointer was, and reads the press as a drag), so toolbar buttons are
        // worked by their signal instead of by the mouse.
        const toolbarIcon = label => smokeFind(win.contentItem, item => item.label === label && item.kind !== undefined)
        let made = ""
        let before = 0
        let key = ""

        smokeSteps = [
            // ---- the lists
            () => {
                click(centre(row("libraryList", l => l.name === "Demo")))
                smokeCheck("a library opens", playlistId === "" && libraryPath.endsWith("/Demo") && documents.length > 10 && document !== null, documents.length + " presentations")
                click(centre(row("presentationList", d => d.name === "Build My Life")))
                smokeCheck("a presentation opens", document.name === "Build My Life" && document.slides.length > 0, document.slides.length + " slides")
            },
            () => {
                click(centre(row("playlistList", p => p.name === "Sunday Morning")))
                smokeCheck("a playlist opens", playlistId !== "" && selectedNode === playlistId && documents.some(d => d.kind === "header"), documents.length + " rows")
                const first = documents.find(d => openable(d))
                click(centre(row("presentationList", d => d.path === first.path)))
                smokeCheck("a playlist row opens its presentation", documentKey === first.path && document.name === first.name)
            },
            // ---- going live
            () => {
                click(centre(cell("slideGrid", 1)))
                smokeCheck("a click puts a slide on the output", liveIndex === 1 && !cleared && viewingLive && liveSlide !== null)
                testInput.key(Qt.Key_Right)
                smokeCheck("Right goes to the next slide", liveIndex === 2)
                testInput.key(Qt.Key_Left)
                smokeCheck("Left goes back", liveIndex === 1)
                testInput.key(Qt.Key_F2)
                smokeCheck("F2 clears the slide", cleared && liveSlide === null)
                testInput.key(Qt.Key_Space)
                smokeCheck("Space brings it back", !cleared && liveIndex === 1)
                key = documentKey
                testInput.key(Qt.Key_Down)
                smokeCheck("Down opens the next presentation", documentKey !== key && !viewingLive)
                testInput.key(Qt.Key_Up)
                smokeCheck("Up goes back to the live one", documentKey === key && viewingLive)
            },
            () => {
                click(centre(row("mediaPlaylistList", p => p.name === "Assets")))
                smokeCheck("a media playlist opens", mediaFiles.length > 5, mediaFiles.length + " files")
                click(centre(cell("mediaGrid", 2)))
                smokeCheck("a click puts media on the output", liveMedia !== null && liveMedia.path === mediaFiles[2].path && liveMediaPlaylistId === mediaPlaylistId)
                testInput.key(Qt.Key_F3)
                smokeCheck("F3 clears the media", liveMedia === null && !cleared)
                testInput.key(Qt.Key_F1)
                smokeCheck("F1 clears everything", cleared)
            },
            // ---- menus
            () => click(centre(row("presentationList", d => d.path === documentKey)), Qt.RightButton),
            () => {
                testInput.grab("m01-presentation-menu")
                smokeCheck("a presentation's menu", menu.opened && menu.items.some(i => i.label === "Edit") && menu.items.some(i => i.header === "Arrangement")
                           && menu.items.some(i => i.label === "Remove from Playlist") && menu.items.some(i => i.label === "Rebuild Thumbnails"), labels())
                testInput.key(Qt.Key_Escape)
                smokeCheck("Esc closes it", !menu.opened)
                click(centre(cell("slideGrid", 2)), Qt.RightButton)
            },
            () => {
                smokeCheck("a slide's menu", menu.opened && /^Edit, Add Action, Remove Action(\(off\))?, Theme(\(off\))?, \[Media\], Background, Foreground, Remove Media, \[Slide\], Copy, Paste(\(off\))?, Delete Slide…$/.test(labels()), labels())
                testInput.key(Qt.Key_Escape)
                click(centre(row("playlistList", p => p.name === "Sunday Morning")), Qt.RightButton)
            },
            () => {
                smokeCheck("a playlist's menu", menu.opened && labels().includes("Rename") && labels().includes("Remove Playlist…"), labels())
                testInput.key(Qt.Key_Escape)
                click(centre(row("mediaPlaylistList", p => p.name === "Assets")), Qt.RightButton)
            },
            () => {
                smokeCheck("a media playlist's menu", menu.opened && labels().includes("Rebuild Thumbnails") && labels().includes("Remove Playlist…"), labels())
                testInput.key(Qt.Key_Escape)
                click(centre(cell("mediaGrid", 1)), Qt.RightButton)
            },
            () => {
                smokeCheck("a media file's menu", menu.opened && labels() === "[Behaviour], Background, Foreground, [Playlist], Remove from Playlist", labels())
                testInput.key(Qt.Key_Escape)
                // ---- a new playlist, from the + menu
                before = catalog.playlists.length
                click(centre(named("addPlaylistButton")))
            },
            () => {
                smokeCheck("the add menu", menu.opened && labels() === "Add Folder, Add Playlist, Import Playlist…", labels())
                click(menuRow("Add Playlist"))
            },
            () => {
                smokeCheck("a playlist is added and opened", catalog.playlists.length === before + 1 && documents.length === 0 && playlistId !== "")
                made = playlistId
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Smoke")
                testInput.key(Qt.Key_Return)
                smokeCheck("it is renamed in place", catalog.playlists.some(p => p.path === made && p.name === "Smoke"), catalog.playlists.map(p => p.name).join(", "))
            },
            () => {
                // ---- drag a presentation from the library onto it
                click(centre(row("libraryList", l => l.name === "Demo")))
            },
            () => {
                const from = centre(row("presentationList", d => d.name === "Abandoned"))
                const to = centre(row("playlistList", p => p.path === made))
                drag(from, to)
                smokeCheck("a presentation dragged onto a playlist is added to it", catalog.playlistItems(made).length === 1
                           && catalog.playlistItems(made)[0].name === "Abandoned", catalog.playlistItems(made).map(i => i.name).join(", "))
            },
            () => {
                // ---- drag media onto a slide
                click(centre(row("presentationList", d => d.name === "Abandoned")))
            },
            () => {
                const media = mediaFiles[3]
                drag(centre(cell("mediaGrid", 3)), centre(cell("slideGrid", 2)))
                smokeCheck("media dragged onto a slide becomes its media", document.slides[2].mediaName === media.name, document.slides[2].mediaName + " / " + media.name)
            },
            () => click(centre(cell("slideGrid", 2)), Qt.RightButton),
            () => {
                smokeCheck("Remove Media is offered now", menu.opened && menu.items.find(i => i.label === "Remove Media").disabled !== true
                           && menu.items.find(i => i.label === "Background").current === true)
                click(menuRow("Remove Media"))
            },
            () => {
                smokeCheck("and removes it", document.slides[2].mediaName === "")
                // ---- reorder the media playlist by dragging
                const first = mediaFiles[0].id
                const second = mediaFiles[1].id
                const a = cell("mediaGrid", 0)
                const b = cell("mediaGrid", 1)
                const from = centre(a)
                const to = b.mapToItem(null, b.width * 0.8, b.height / 2)
                drag(from, to)
                smokeCheck("media dragged past its neighbour changes places with it", mediaFiles[0].id === second && mediaFiles[1].id === first)
                // and back
                const a2 = cell("mediaGrid", 1)
                const b2 = cell("mediaGrid", 0)
                drag(centre(a2), b2.mapToItem(null, b2.width * 0.2, b2.height / 2))
                smokeCheck("and back again", mediaFiles[0].id === first && mediaFiles[1].id === second)
            },
            () => {
                // ---- remove the playlist again, through its menu's confirmation
                click(centre(row("playlistList", p => p.path === made)), Qt.RightButton)
            },
            () => click(menuRow("Remove Playlist…")),
            () => {
                smokeCheck("removing asks first", menu.opened && menu.items.some(i => i.label === "Remove" && i.danger), labels())
                click(menuRow("Remove"))
            },
            () => {
                smokeCheck("the playlist is gone", !catalog.playlists.some(p => p.path === made) && catalog.playlists.length === before)
                // ---- the toolbar and the panes
                toolbarIcon("Media").clicked()
                smokeCheck("the Media button hides the media bin", !mediaBinVisible)
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                smokeCheck("Ctrl+V shows it again", mediaBinVisible)
            },
            () => {
                const width = sidebarWidth
                const divider = smokeFind(win.contentItem, item => item.cursorShape === Qt.SplitHCursor && item.moved !== undefined)
                const p = centre(divider)
                drag(p, Qt.point(p.x + 40, p.y))
                smokeCheck("dragging a divider resizes a pane", Math.abs(sidebarWidth - width - 40) < 3, width + " -> " + sidebarWidth)
                const thumbnails = thumbnailWidth
                zoomThumbnails(-1)
                zoomThumbnails(-1)
                smokeCheck("thumbnails can be made smaller", thumbnailWidth < thumbnails || thumbnails === smallestThumbnail, thumbnails + " -> " + thumbnailWidth)
                toolbarIcon("Settings").clicked()
            },
            () => {
                testInput.grab("m02-settings")
                smokeCheck("Settings opens", settingsOpen)
                testInput.key(Qt.Key_Escape)
                smokeCheck("Esc closes it", !settingsOpen)
                toolbarIcon("Edit").clicked()
            },
            () => {
                testInput.grab("m03-editor")
                smokeCheck("Edit brings up the editor", editing && editScreen.editor.count > 0)
                toolbarIcon("Edit").clicked()
                smokeCheck("and takes it down", !editing)
                testInput.key(Qt.Key_Right)
                smokeCheck("the keys drive the slides again", !cleared && liveIndex >= 0)
            },
            () => {
                testInput.grab("m04-end")
                // Last, since showing these windows takes the keyboard away in a test.
                const output = outputEnabled
                const stage = stageEnabled
                testInput.key(Qt.Key_1, Qt.ControlModifier)
                smokeCheck("Ctrl+1 switches the output", outputEnabled === !output)
                toolbarIcon("Output").clicked()
                smokeCheck("the Output button switches it back", outputEnabled === output)
                toolbarIcon("Stage").clicked()
                smokeCheck("the Stage button switches the stage", stageEnabled === !stage)
                testInput.say(smokeFailures === 0 ? "ALL PASSED" : smokeFailures + " FAILED")
                testInput.quit()
            }
        ]
        smokeStep = 0
        smokeTimer.start()
    }

    Timer {
        id: smokeTimer

        interval: 400
        repeat: true
        onTriggered: {
            if (holder.smokeStep >= holder.smokeSteps.length) {
                stop()
                return
            }
            try {
                testInput.focus()
                holder.smokeSteps[holder.smokeStep]()
            } catch (e) {
                testInput.say("  EXCEPTION in step " + (holder.smokeStep + 1) + ": " + e + " at " + e.lineNumber)
                holder.smokeFailures++
            }
            holder.smokeStep++
        }
    }


}
