import QtQuick

// A matrix of where each of some inputs goes among some outputs, as ProPresenter draws
// one (its Channel Routing window, and its Cue Inspector's Audio tab: see
// support.renewedvision.com, "Audio Routing in ProPresenter"): the inputs down the left,
// the outputs across the top by their numbers from 1, and a cell where each row meets
// each column, lit where that input goes to that output. An input can go to any number
// of outputs, or none. A click on a cell asks for it to be lit or put out (`toggled`);
// what is lit is `routes`, which whoever has the matrix changes (the rules are
// audiorouting's, in C++). The left part of each row is whoever has it's to draw
// (`rowHeader`, an item with a `row` property, which is set to the row's number from
// 0), as is the corner above them (`corner`).
//
// It is made only where it is shown (the settings' Channel Routing window, a media's
// Audio tab), and each row has one place that takes clicks, not one for each cell: a
// device of thirty-two channels and an app of sixteen are five hundred cells.
Item {
    id: matrix

    property int rows: 0
    property int columns: 0
    // For each row, the columns it goes to, from 0
    property var routes: []
    // What the columns are, said over them (a device's name, "Channels")
    property string columnsTitle: ""
    property real headerWidth: 220
    property Component rowHeader: null
    property Component corner: null
    readonly property real cell: 22
    readonly property real gap: 4

    signal toggled(int row, int column)

    implicitWidth: headerWidth + columns * (cell + gap)
    implicitHeight: top.height + rows * (cell + gap + 4)

    Item {
        id: top

        width: parent.width
        height: Math.max(cornerItem.height, heads.height) + 6

        Loader {
            id: cornerItem

            width: matrix.headerWidth - 8
            sourceComponent: matrix.corner
        }

        Column {
            id: heads

            x: matrix.headerWidth
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            spacing: 4

            Text {
                width: Math.max(implicitWidth, matrix.columns * (matrix.cell + matrix.gap))
                visible: text !== ""
                elide: Text.ElideRight
                color: "#8d9097"
                font.pixelSize: 11
                font.bold: true
                text: matrix.columnsTitle
            }

            Row {
                spacing: matrix.gap

                Repeater {
                    model: matrix.columns

                    Text {
                        required property int index

                        width: matrix.cell
                        horizontalAlignment: Text.AlignHCenter
                        color: "#b0b3b8"
                        font.pixelSize: 12
                        text: index + 1
                    }
                }
            }
        }
    }

    Column {
        anchors.top: top.bottom
        spacing: 4

        Repeater {
            model: matrix.rows

            Item {
                id: line

                required property int index
                readonly property var to: matrix.routes[index] || []

                objectName: "routingRow:" + index
                width: matrix.implicitWidth
                height: matrix.cell + matrix.gap

                // (A line between rows, as between the groups of the operator window)
                Rectangle {
                    y: parent.height + 1
                    width: parent.width
                    height: 1
                    color: "#2e3035"
                }

                Loader {
                    width: matrix.headerWidth - 8
                    height: parent.height
                    sourceComponent: matrix.rowHeader
                    onLoaded: item.row = Qt.binding(() => line.index)
                }

                Row {
                    id: cells

                    x: matrix.headerWidth
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: matrix.gap

                    Repeater {
                        model: matrix.columns

                        Rectangle {
                            required property int index
                            readonly property bool lit: line.to.indexOf(index) >= 0

                            objectName: "routingCell:" + line.index + ":" + index
                            width: matrix.cell
                            height: matrix.cell
                            radius: 4
                            color: lit ? "#ff8a1f" : "#15161a"
                            border.width: 1
                            border.color: lit ? "#ff8a1f" : "#45484e"
                        }
                    }
                }

                MouseArea {
                    anchors.fill: cells
                    onClicked: (mouse) => {
                        const column = Math.floor(mouse.x / (matrix.cell + matrix.gap))
                        if (column >= 0 && column < matrix.columns && mouse.x - column * (matrix.cell + matrix.gap) <= matrix.cell)
                            matrix.toggled(line.index, column)
                    }
                }
            }
        }
    }
}
