import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The guide (docs/guide.md, src/guide.h, qml/GuideWindow.qml): the ? button at the right of the toolbar, after Settings,
// putting up its window, which shows the repository's page from its first heading on, made rich text from its
// Markdown, its links in the app's orange; the button again putting it away; its own close putting it away too, the button saying so; and
// the app quitting with it open, which the runner sees. (Esc is not tried: the test's desktop makes no window active
// that a person's own click did not open, and the keys go to the window that is.)
QtObject {
    id: t

//COMMON
    function guide() {
        return testInput.windowContent("guideWindow") ? Lib.find(testInput.windowContent("guideWindow"), i => i.objectName === "guideText") : null
    }

    function words() {
        const view = guide()
        return view ? Lib.find(view, i => i.textFormat !== undefined && i.text !== undefined && i.text.length > 100) : null
    }

    function run() {
        steps = [
            () => {
                const button = named("guideButton")
                const settings = Lib.find(win.contentItem, i => i.label === "Settings" && i.kind === "settings")
                check("the toolbar has a small ? button, to the right of Settings", button !== null && button.visible && button.text === "?" && button.width <= 30
                      && settings !== null && button.mapToItem(null, 0, 0).x > settings.mapToItem(null, 0, 0).x)
                check("and nothing of the guide is made until it is wanted", testInput.windowContent("guideWindow") === null)
                button.clicked()
                return 800
            },
            () => {
                const w = words()
                check("it puts up the guide, in a window of its own", guideOpen && w !== null && w.visible, w ? w.text.length : "none")
                check("which is the repository's page from its first heading on, made rich text", w !== null && w.textFormat === Text.RichText
                      && w.text.includes("Simple Presenter, in short") && w.text.includes("Its own features") && w.text.includes(">Keys<")
                      && !w.text.includes("../README.md") && !w.text.includes("This page is also what"), w ? w.text.slice(0, 120) : "none")
                check("its links among it, in the app's orange", w !== null && w.text.includes("https://github.com/greyshirtguy/Simple-Presenter")
                      && w.text.toLowerCase().includes("#ff8a1f"))
                check("saying the app's line", w !== null && w.text.includes("not a product for use") && w.text.includes("nobody supports it"))
                testInput.grabIn("guideWindow", "1-guide")
                named("guideButton").clicked()
                return 500
            },
            () => {
                check("the button puts it away", !guideOpen && !testInput.windowContent("guideWindow").Window.window.visible)
                named("guideButton").clicked()
                return 500
            },
            () => {
                check("and up again", guideOpen && words().visible)
                named("guideButton").clicked()
                return 500
            },
            () => {
                check("and away", !guideOpen)
                named("guideButton").clicked()
                return 500
            },
            () => {
                testInput.windowContent("guideWindow").Window.window.close()
                return 500
            },
            () => {
                check("its own close puts it away, and the button says so", !guideOpen && named("guideButton").highlighted === false)
                named("guideButton").clicked()
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                // (Left open: the app is to quit with it so, which the runner sees)
                return 600
            },
        ]
        next()
    }
}
