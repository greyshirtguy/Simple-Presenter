import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Getting NDI's library: what comes up when a screen is set to NDI without it, the steps
// by hand, and the app fetching it: NDI's licence shown, nothing put in place unless it
// is agreed to, and the screen on the network once it is, without the app being started
// again. (The fetching is from a copy of NDI's installer on this computer, where there
// is one; where there is not, what is tried is what the app says when fetching fails.)
QtObject {
    id: t

//COMMON
    readonly property bool installerHere: !Ndi.source.includes("/nonexistent/")
    readonly property string library: Ndi.folder + "/" + Ndi.fileName

    function settingsRows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow")
    }

    function shown(name) {
        const item = named(name)
        if (!item)
            return false
        for (let at = item; at; at = at.parent) {
            if (!at.visible)
                return false
        }
        return true
    }

    function run() {
        steps = [
            () => {
                check("NDI's library is not on this computer to begin with", !Ndi.available && Ndi.folder.includes("/ndisetup/data/") && !testInput.exists(library), Ndi.folder)
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                Screens.add("audience")
                return 500
            },
            () => {
                kept.id = Screens.audience[1].id
                const choice = Lib.find(settingsRows()[1], item => String(item.objectName) === "screenOutput")
                check("NDI is among the things a screen can be sent to", choice.model.indexOf("NDI") > 0, JSON.stringify(choice.model))
                check("nothing is asked until it is chosen", !shown("ndiSetup"))
                choice.activated(choice.model.indexOf("NDI"))
                return 600
            },
            () => {
                check("choosing NDI without its library brings up how to get it", Screens.audience[1].output === "ndi" && shown("ndiSetup") && shown("ndiFetch")
                      && shown("ndiByHand") && shown("ndiLookAgain"))
                check("with the steps by hand: where NDI gives it out, which file, and the folder to put it in", named("ndiByHand").text.includes("ndi.video")
                      && named("ndiByHand").text.includes("lib/" + Ndi.sdkFolder + "/libndi.so") && Ndi.sdkFolder === "x86_64-linux-gnu"
                      && named("ndiFolder").text === Ndi.folder, named("ndiByHand").text)
                check("the screen is not on the network meanwhile, and its line says why", senders[kept.id] !== undefined && senders[kept.id].sending === false
                      && Lib.find(settingsRows()[1], item => String(item.objectName) === "screenStatus").text.includes("not on this computer")
                      && shown("screenGetNdi") === false)
                testInput.grab("1-asked")
                click(centre(named("ndiFetch")))
                return installerHere ? 15000 : 1500
            },
            () => {
                if (!installerHere) {
                    check("a download that fails says so, and leaves the steps by hand", Ndi.fetching === "failed" && Ndi.fetchProblem.includes("could not be downloaded")
                          && named("ndiState").text.includes("did not work") && shown("ndiByHand") && shown("ndiFetch") && !testInput.exists(library), Ndi.fetchProblem)
                    return 100
                }
                check("the app fetches NDI's installer and shows NDI's licence", Ndi.fetching === "licence" && shown("ndiLicence")
                      && named("ndiLicence").text.startsWith("NDI SDK License Agreement") && named("ndiLicence").text.length > 5000 && shown("ndiAgree") && shown("ndiDecline"),
                      Ndi.fetching + " " + Ndi.fetchProblem + " " + Ndi.licence.length)
                check("nothing is in place before the licence is agreed to", !testInput.exists(library) && !Ndi.available)
                testInput.grab("2-licence")
                click(centre(named("ndiDecline")))
                return 500
            },
            () => {
                if (!installerHere)
                    return 100
                check("not agreed to, nothing is put in place and the download is forgotten", Ndi.fetching === "" && Ndi.licence === "" && !testInput.exists(library)
                      && !Ndi.available && shown("ndiFetch"))
                click(centre(named("ndiFetch")))
                return 15000
            },
            () => {
                if (!installerHere)
                    return 100
                check("fetched again, the licence is there again", Ndi.fetching === "licence" && shown("ndiAgree"), Ndi.fetching + " " + Ndi.fetchProblem)
                click(centre(named("ndiAgree")))
                return 15000
            },
            () => {
                if (!installerHere)
                    return 100
                check("agreed to, the library is put in the app's own folder, with NDI's licence beside it", Ndi.fetching === "done" && testInput.exists(library)
                      && testInput.exists(Ndi.folder + "/NDI SDK License Agreement.txt"), Ndi.fetching + " " + Ndi.fetchProblem)
                check("and is found without the app being started again", Ndi.available && Ndi.version.includes("NDI") && Ndi.path === library, Ndi.version + " at " + Ndi.path)
                check("the screen that was waiting for it is on the network", senders[kept.id].sending === true && senders[kept.id].problem === "", senders[kept.id].problem)
                check("the panel says it is ready, and has nothing more to ask", named("ndiState").text.includes("in place") && !shown("ndiFetch") && !shown("ndiByHand")
                      && named("ndiClose").text === "Done")
                testInput.grab("3-ready")
                return 2500
            },
            () => {
                if (installerHere)
                    check("pictures go out", senders[kept.id].framesSent >= 2, senders[kept.id].framesSent)
                click(centre(named("ndiClose")))
                return 400
            },
            () => {
                check("the panel is put away", !shown("ndiSetup"))
                if (!installerHere) {
                    check("and a screen still without the library has a button to bring it back", shown("screenGetNdi"))
                    click(centre(named("screenGetNdi")))
                }
                return 400
            },
            () => {
                if (!installerHere) {
                    check("which does", shown("ndiSetup") && shown("ndiFetch"))
                    click(centre(named("ndiLookAgain")))
                    check("looking again where nothing has been put changes nothing", !Ndi.available && shown("ndiSetup"))
                    click(centre(named("ndiClose")))
                }
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "about"
                return 400
            },
            () => {
                check("About says whose NDI is, and where to find it", shown("aboutNdi") && named("aboutNdi").text.includes("NDI® is a registered trademark of Vizrt NDI AB")
                      && named("aboutNdi").text.includes("https://ndi.video"))
                Screens.setOutput(kept.id, { output: "none" })
                return 300
            }
        ]
        next()
    }
}
