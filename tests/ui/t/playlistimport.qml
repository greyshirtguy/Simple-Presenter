import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Importing an exported playlist (.proplaylist) that brings what is here already (the maintainer, 2026-11-08: an import
// had left the library's older presentations as they were, and they would rather be offered to overwrite them). The
// archive (prepare_playlistimport) has the workspace's own playlists, so a playlist of each name is here, a presentation
// the library has ("Abandoned") with other words in it, and one it has not. Asked first, at the Playlists "+", with what is
// here named; Cancel does nothing; Keep What Is Here leaves the library's presentation as it was and adds the playlists
// beside the old ones; Replace Them puts the archive's presentation in its place, says that the old one is kept among the
// editor's backups, and puts each playlist in place of the one of its name, the number of playlists as it was. What is
// looked at is the files themselves.
QtObject {
    id: t

//COMMON
    readonly property string archive: "file://" + catalog.workspacePath + "/Exported.proplaylist"

    function library() {
        return catalog.libraries.find(l => l.name === "Demo").path
    }

    function hashOf(name) {
        return testInput.fileHash(library() + "/" + name + ".pro")
    }

    function playlistCount() {
        return catalog.playlists.filter(p => !p.folder).length
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                openLibrary(library())
                kept.before = hashOf("Abandoned")
                kept.other = hashOf("Fresh Wind")
                kept.playlists = playlistCount()
                kept.names = catalog.playlists.filter(p => !p.folder).map(p => p.name)
                const here = catalog.describePlaylistImport(archive, libraryPath)
                check("what the archive would replace is told: the library's presentation of the same name, and the playlists of the same names",
                      here.error === "" && Array.from(here.presentations).join() === "Abandoned" && here.playlists.length === kept.names.length,
                      JSON.stringify(here))
                askPlaylistImport(archive)
                return 300
            },
            () => {
                check("so the import asks first, naming what is here", menu.opened && labels().startsWith("[Import Playlist], (note), Replace Them, Keep What Is Here, Cancel")
                      && menu.shown[1].note.includes("“Abandoned”"), labels())
                testInput.grab("1-asked")
                click(menuRow("Cancel"))
                return 1200
            },
            () => {
                check("Cancel imports nothing", playlistCount() === kept.playlists && hashOf("Abandoned") === kept.before && !testInput.exists(library() + "/Brand New Song.pro"))
                askPlaylistImport(archive)
                return 300
            },
            () => {
                click(menuRow("Keep What Is Here"))
                return 2000
            },
            () => {
                check("Keep What Is Here: the library's presentation as it was, the new one copied in", hashOf("Abandoned") === kept.before
                      && testInput.exists(library() + "/Brand New Song.pro"), notice)
                check("and the playlists added beside the old ones", playlistCount() === 2 * kept.playlists, playlistCount())
                check("the user is told so", /1 already here, kept/.test(notice) && !noticeIsError, notice)
                kept.afterKeep = playlistCount()
                askPlaylistImport(archive)
                return 300
            },
            () => {
                click(menuRow("Replace Them"))
                return 2000
            },
            () => {
                check("Replace Them: the archive's presentation in place of the library's", hashOf("Abandoned") === kept.other && hashOf("Abandoned") !== kept.before)
                check("each playlist in place of the one of its name: no more playlists than there were", playlistCount() === kept.afterKeep, playlistCount())
                check("the user is told, and that the old is kept among the editor's backups", /2 replaced \(the old kept among the editor's backups\)/.test(notice)
                      && !noticeIsError, notice)
                check("the log says what was asked and chosen", logged(/import +asked whether to replace/).length === 3
                      && logged(/import +the archive's to replace what is here/).length === 1 && logged(/import +what is here kept/).length === 1
                      && logged(/import +not done/).length === 1)
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
