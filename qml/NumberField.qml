import QtQuick

// A box for typing a number. It shows `value` and reports a different one with edited()
// when Enter is pressed or the box is left; it does not change `value` itself. Up and
// Down, and the wheel, step the number. `finished` follows every edit, and Esc, so that
// whoever owns the keyboard can take it back.
AppTextField {
    id: field

    property real value: 0
    property int decimals: 0
    property real from: -100000
    property real to: 100000
    property real step: 1
    // Shown after the number, such as "%"
    property string suffix

    signal edited(real value)
    signal finished

    function show() {
        text = Number(value.toFixed(decimals)).toString() + suffix
    }

    function offer(number) {
        const bounded = Math.max(from, Math.min(to, Number(number.toFixed(decimals))))
        if (bounded !== Number(value.toFixed(decimals)))
            edited(bounded)
    }

    // What is typed, as a number, or the value if it is not one.
    function typed() {
        const number = parseFloat(text.replace(",", "."))
        return isFinite(number) ? number : value
    }

    width: 62
    height: 26
    leftPadding: 6
    rightPadding: 6
    font.pixelSize: 12
    horizontalAlignment: TextInput.AlignRight
    onValueChanged: show()
    onSuffixChanged: show()
    Component.onCompleted: show()
    onEditingFinished: {
        offer(typed())
        show()
    }
    onAccepted: finished()
    Keys.onEscapePressed: {
        show()
        finished()
    }
    Keys.onUpPressed: offer(typed() + step)
    Keys.onDownPressed: offer(typed() - step)

    WheelHandler {
        // Only while the box is being typed in, so scrolling the panel past it does nothing.
        enabled: field.activeFocus
        onWheel: (event) => field.offer(field.typed() + (event.angleDelta.y > 0 ? field.step : -field.step))
    }
}
