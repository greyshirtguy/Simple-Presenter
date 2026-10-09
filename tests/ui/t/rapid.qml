import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Slides clicked in quick succession: the second, with the same background as the first,
// while that background is still on its way.
QtObject {
    id: t

//COMMON
    function shown() {
        // What the slide layer's instances hold: the ids of the slides that are content somewhere on the output
        return Lib.findAll(output.contentItem, item => item.content !== undefined && item.content !== null && item.content.id !== undefined && item.content.elements !== undefined)
                  .map(item => item.content.id)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                goLive(6)
                check("the first slide waits for its background", output.heldSlide !== undefined && output.heldSlide.id === document.slides[6].id && shown().length === 0)
                goLive(15)
                check("the second, with the same background, takes its place in waiting", output.heldSlide !== undefined && output.heldSlide.id === document.slides[15].id
                      && shown().length === 0 && liveIndex === 15 && liveMedia.name.startsWith("Colorflow"))
                return 3000
            },
            () => {
                check("and goes on with the background when that arrives", output.heldSlide === undefined && shown().length === 1 && shown()[0] === document.slides[15].id
                      && player() !== null && player().playbackState === MediaPlayer.PlayingState && player().position > 300, shown().join(",") + " at " + (player() ? player().position : "none"))
                kept.player = player()
                kept.position = player().position
                goLive(26)
                check("with the background playing, the next such slide goes on at once", output.heldSlide === undefined && shown().includes(document.slides[26].id))
                return 1500
            },
            () => {
                check("and the background was not started again", player() === kept.player && player().position > kept.position + 900, kept.position + " -> " + player().position)
                return 100
            }
        ]
        next()
    }
}
