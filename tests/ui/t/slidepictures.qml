import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Pictures of slides through the API (src/slidepictures.h), as ProPresenter Remote fetches
// them: GET .../thumbnail/{index}, by a presentation's UUID or an item of a playlist,
// drawn by a process of the app's own (src/slidepictureworker.h) and sent when they come.
// What comes back is looked at as a picture: its size (`quality`, the longer side), JPEG
// or PNG as asked, ProPresenter's checkerboard where the slide is transparent, the slide's
// words drawn, and its media where it has some. A cue that is not there is 404. A request
// sent after one for a picture, on the same connection, is answered after it, in turn.
QtObject {
    id: t

//COMMON
    readonly property int port: 50125

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function get(path, more) {
        return testInput.httpSend(port, "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\n\r\n" + (more ?? ""))
    }

    // The responses on a connection, in order: each {status, headers, body}
    function responses(id) {
        const raw = testInput.httpReceived(id)
        const found = []
        let at = 0
        while (at < raw.length) {
            const end = raw.indexOf("\r\n\r\n", at)
            if (end < 0)
                break
            const lines = raw.slice(at, end).split("\r\n")
            const headers = {}
            for (const line of lines.slice(1)) {
                const colon = line.indexOf(":")
                headers[line.slice(0, colon).toLowerCase()] = line.slice(colon + 1).trim()
            }
            const length = Number(headers["content-length"] ?? 0)
            found.push({ status: Number(lines[0].split(" ")[1]), headers: headers, body: raw.slice(end + 4, end + 4 + length) })
            at = end + 4 + length
        }
        return found
    }

    function picture(response, points) {
        return testInput.pictureIn(response ? response.body : "", points ?? [[1, 1], [9, 1]], 150)
    }

    // The checkerboard's two greys, as JPEG gives them back (within a little)
    function checker(color) {
        const grey = parseInt(color.slice(1, 3), 16)
        return color.slice(1, 3) === color.slice(3, 5) && (Math.abs(grey - 0x22) <= 4 || Math.abs(grey - 0x0a) <= 4)
    }

    function run() {
        steps = [
            () => {
                win.networkPort = t.port
                win.networkEnabled = true
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                return 800
            },
            () => {
                kept.words = document.slides.findIndex(s => (s.plainText ?? "").length > 20)
                kept.count = document.slides.length
                check("a presentation with words, open, and the API on", kept.words >= 0 && document.uuid !== "" && HttpApi.listening, document.name)
                const at = "/v1/presentation/" + document.uuid + "/thumbnail/"
                kept.jpeg = get(at + kept.words + "?quality=400")
                kept.png = get(at + kept.words + "?quality=200&thumbnail_type=png")
                kept.before = get(at + "-1")
                kept.past = get(at + (kept.count + 40))
                // (One for a picture, then /version, on one connection: answered in turn)
                kept.inTurn = get(at + kept.words + "?quality=320", "GET /version HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                // An item of a playlist, by the playlist's name
                const playlist = catalog.playlists.find(p => !p.folder && catalog.playlistItems(p.path).some(r => openable(r)))
                kept.item = playlist ? catalog.playlistItems(playlist.path).findIndex(r => openable(r)) : -1
                kept.playlist = playlist ? get("/v1/playlist/" + encodeURIComponent(playlist.name) + "/" + kept.item + "/thumbnail/0") : -1
                // A slide with media, where Demo has one
                kept.media = null
                for (const row of documents) {
                    if (!openable(row))
                        continue
                    const index = catalog.open(row.file).slides.findIndex(s => s.media !== undefined && !s.media.missing)
                    if (index >= 0) {
                        kept.media = { name: row.name, request: get("/v1/presentation/" + HttpApi.presentationUuid(row.file) + "/thumbnail/" + index) }
                        break
                    }
                }
                return 4000
            },
            () => {
                const jpeg = responses(kept.jpeg)[0]
                // (Along the foot: The Joy's slides darken their top behind their words)
                const drawn = picture(jpeg, [[1, 209], [9, 209], [1, 217], [9, 217]])
                check("a picture of a slide by its presentation's UUID and its cue: a JPEG, its length said, 400 by 225",
                      jpeg !== undefined && jpeg.status === 200 && jpeg.headers["content-type"] === "image/jpeg"
                      && Number(jpeg.headers["content-length"]) === jpeg.body.length && drawn.width === 400 && drawn.height === 225,
                      jpeg ? jpeg.status + " " + jpeg.headers["content-type"] + " " + drawn.width + "x" + drawn.height : "nothing")
                check("drawn: ProPresenter's checkerboard where the slide is transparent, in 8-pixel squares",
                      drawn.colors !== undefined && drawn.colors.every(checker) && drawn.colors[0] !== drawn.colors[1] && drawn.colors[1] === drawn.colors[2]
                      && drawn.colors[0] === drawn.colors[3], JSON.stringify(drawn.colors))
                check("drawn: the slide's words, light over it", drawn.lighter > 0.002 && drawn.lighter < 0.4, drawn.lighter)
                const png = responses(kept.png)[0]
                const small = picture(png)
                check("PNG, and another size, as asked: 200 by 113", png !== undefined && png.status === 200 && png.headers["content-type"] === "image/png"
                      && png.body.startsWith("\u0089PNG") && small.width === 200 && small.height === 113, png ? png.headers["content-type"] + " " + small.width : "nothing")
                check("a cue that is not there is 404: -1, and past the last", responses(kept.before)[0]?.status === 404 && responses(kept.past)[0]?.status === 404,
                      responses(kept.before)[0]?.status + " " + responses(kept.past)[0]?.status)
                const inTurn = responses(kept.inTurn)
                check("a request after one for a picture is answered after it, in turn", inTurn.length === 2 && inTurn[0].headers["content-type"] === "image/jpeg"
                      && picture(inTurn[0]).width === 320 && JSON.parse(inTurn[1].body).api_version === "v1", inTurn.map(r => r.status + " " + r.headers["content-type"]).join(", "))
                if (kept.playlist >= 0) {
                    const item = responses(kept.playlist)[0]
                    check("a picture of an item of a playlist", item !== undefined && item.status === 200 && picture(item).width === 400, item ? item.status : "nothing")
                } else {
                    check("(no playlist of Demo has a presentation)", true)
                }
                if (kept.media) {
                    const media = responses(kept.media.request)[0]
                    const middle = picture(media, [[200, 112], [100, 60], [300, 170]])
                    check("a slide with media has its media drawn behind it, not the checkerboard", media !== undefined && media.status === 200
                          && !middle.colors.every(checker), kept.media.name + " " + JSON.stringify(middle.colors))
                } else {
                    check("(no slide of Demo has media)", true)
                }
                check("the log says the process that draws them was started, and was ready",
                      logged(/the process that draws the pictures of slides for the API is started/).length === 1
                      && logged(/slide pictures: ready/).length === 1)
                check("and none failed", logged(/could not be made|were not made/).length === 0)
                win.networkEnabled = false
                return 300
            },
        ]
        next()
    }
}
