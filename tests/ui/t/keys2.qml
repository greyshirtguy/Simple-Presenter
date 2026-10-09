import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// Settings as an earlier version left them, with hotkeys in them, on a workspace that has no list of groups: the
// workspace is given one, with the hotkeys in it.
QtObject {
    id: t

//COMMON
    function run() {
        steps = [
            () => {
                const keys = groups.filter(g => g.key).map(g => g.name + "=" + g.key).join(" ")
                check("the hotkeys an earlier version kept in the settings are the groups' still", keys === "Verse=V Chorus=C Tag=T", keys)
                check("and the workspace now has a list of groups, with them in it", GroupKeys.exists && GroupKeys.keys.map(h => h.label + "=" + h.key).join(" ") === "Verse=V Chorus=C Tag=T", JSON.stringify(GroupKeys.keys))
                check("the app's settings no longer hold them", !String(settings.value("groups", "")).includes("key") && String(settings.value("groups", "")).includes("Tag"), String(settings.value("groups", "")).slice(0, 80))
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says so", lines.some(l => l.includes("moved from the app's settings into a list of groups for the workspace")))
            }
        ]
        next()
    }
}
