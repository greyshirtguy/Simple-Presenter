import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Changing workspace: the timers are the workspace's.
QtObject {
    id: t

//COMMON
    readonly property string other: "@WORKSPACES@/ProPresenter MR"

    function run() {
        steps = [
            () => {
                kept.first = catalog.workspacePath
                check("the first workspace has the one default timer", Timers.timers.length === 1 && Timers.timers[0].name === "Countdown")
                Timers.start(Timers.timers[0].id)
                return 1300
            },
            () => {
                check("which is running", Timers.state(Timers.timers[0].id).running)
                check("the first workspace has no props and no stage layouts", Props.collections.length === 0 && StageLayouts.layouts.length === 0 && stageLayoutId === "")
                switchWorkspace(other)
                return 800
            },
            () => {
                check("the other workspace's timers replace it", catalog.workspacePath === other && Timers.timers.length === 4 && Timers.timers[0].name === "Timer",
                      Timers.timers.map(x => x.name).join(", "))
                check("none of them running but the countdown to a time of day", Timers.timers.filter(x => Timers.state(x.id).running).map(x => x.kind).every(k => k === "countdownTo"))
                check("and nothing is said to be wrong", notice === "")
                check("its props and stage layouts are read too", Props.collections.length === 2 && Props.collections[0].props.length === 3 && StageLayouts.layouts.length === 4,
                      Props.collections.length + " collections, " + StageLayouts.layouts.length + " layouts")
                check("the stage has the plain view until a layout is chosen", stageLayoutId === "" && stageLayout === null)
                Show.stageLayoutId = StageLayouts.layouts[0].id
                toggleProp(Props.collections[0].props[0].id)
                return 600
            },
            () => {
                check("a prop is on and the stage has a layout", liveProps.length === 1 && shownProps.length === 1 && stageLayout !== null && stageLayout.name === "Singing")
                switchWorkspace(kept.first)
                return 800
            },
            () => {
                check("back in the first, its timer is at its start, not running", Timers.timers.length === 1 && !Timers.state(Timers.timers[0].id).running
                      && Timers.state(Timers.timers[0].id).seconds === 300)
                check("and no timers file was made for it by all that", !testInput.exists(kept.first + "/Configuration/Timers"))
                check("the other workspace's prop went off with it, and its stage layout", liveProps.length === 0 && shownProps.length === 0 && stageLayoutId === "" && stageLayout === null
                      && Props.collections.length === 0 && StageLayouts.layouts.length === 0)
                check("nor a props or a stage file", !testInput.exists(kept.first + "/Configuration/Props") && !testInput.exists(kept.first + "/Configuration/Stage"))
                return 100
            }
        ]
        next()
    }
}
