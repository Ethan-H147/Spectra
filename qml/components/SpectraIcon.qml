import QtQuick

Canvas {
    id: icon

    property string name: ""
    property color color: "white"
    property real strokeWidth: 1.5

    implicitWidth: 18
    implicitHeight: 18

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function line(context, x1, y1, x2, y2) {
        context.beginPath()
        context.moveTo(x1, y1)
        context.lineTo(x2, y2)
        context.stroke()
    }

    function rect(context, x, y, width, height) {
        context.beginPath()
        context.rect(x, y, width, height)
        context.stroke()
    }

    onPaint: {
        const context = getContext("2d")
        const scale = Math.min(width, height) / 18
        const offsetX = (width - 18 * scale) / 2
        const offsetY = (height - 18 * scale) / 2

        context.reset()
        context.clearRect(0, 0, width, height)
        context.translate(offsetX, offsetY)
        context.scale(scale, scale)
        context.strokeStyle = color
        context.fillStyle = color
        context.lineWidth = strokeWidth
        context.lineCap = "round"
        context.lineJoin = "round"

        switch (name) {
        case "help":
            context.beginPath()
            context.arc(9, 9, 6.75, 0, Math.PI * 2)
            context.stroke()
            context.beginPath()
            context.moveTo(6.8, 7)
            context.bezierCurveTo(7, 4.8, 11.2, 4.6, 11.3, 7)
            context.bezierCurveTo(11.35, 8.35, 9, 8.5, 9, 10.25)
            context.stroke()
            context.beginPath()
            context.arc(9, 12.8, 0.65, 0, Math.PI * 2)
            context.fill()
            break
        case "fullscreen":
            line(context, 8, 4, 4, 4)
            line(context, 4, 4, 4, 8)
            line(context, 4, 4, 8, 8)
            line(context, 10, 14, 14, 14)
            line(context, 14, 14, 14, 10)
            line(context, 14, 14, 10, 10)
            break
        case "fullscreen-exit":
            line(context, 3.5, 7.5, 7.5, 7.5)
            line(context, 7.5, 7.5, 7.5, 3.5)
            line(context, 3.5, 3.5, 7.5, 7.5)
            line(context, 14.5, 10.5, 10.5, 10.5)
            line(context, 10.5, 10.5, 10.5, 14.5)
            line(context, 14.5, 14.5, 10.5, 10.5)
            break
        case "overview":
            context.beginPath()
            context.moveTo(2.75, 8.25)
            context.lineTo(9, 3)
            context.lineTo(15.25, 8.25)
            context.stroke()
            context.beginPath()
            context.moveTo(4.25, 7.5)
            context.lineTo(4.25, 14.5)
            context.lineTo(13.75, 14.5)
            context.lineTo(13.75, 7.5)
            context.stroke()
            break
        case "synth":
            rect(context, 2.5, 3.25, 13, 11.5)
            line(context, 6.8, 3.25, 6.8, 14.75)
            line(context, 11.2, 3.25, 11.2, 14.75)
            context.fillRect(5.6, 3.25, 2.35, 5.25)
            context.fillRect(10.05, 3.25, 2.35, 5.25)
            break
        case "analysis":
            context.beginPath()
            context.moveTo(2, 9)
            context.lineTo(4.25, 9)
            context.lineTo(5.5, 5.25)
            context.lineTo(7.3, 12.75)
            context.lineTo(9.15, 3.75)
            context.lineTo(11, 11.3)
            context.lineTo(12.6, 7.2)
            context.lineTo(14, 9)
            context.lineTo(16, 9)
            context.stroke()
            break
        case "harmonics":
            line(context, 2.5, 14.5, 15.5, 14.5)
            line(context, 4, 14.5, 4, 4)
            line(context, 7.3, 14.5, 7.3, 7)
            line(context, 10.7, 14.5, 10.7, 9)
            line(context, 14, 14.5, 14, 10.5)
            break
        case "spectrogram":
            rect(context, 2.75, 3, 12.5, 12)
            context.globalAlpha = 0.35
            context.fillRect(4.25, 4.5, 2.2, 2.2)
            context.fillRect(9.05, 4.5, 4.7, 2.2)
            context.fillRect(6.65, 7.9, 4.7, 2.2)
            context.fillRect(4.25, 11.3, 4.7, 2.2)
            context.fillRect(11.45, 11.3, 2.3, 2.2)
            context.globalAlpha = 1
            break
        case "pitch":
            line(context, 5.5, 14.5, 5.5, 3.5)
            line(context, 2.75, 6.25, 5.5, 3.5)
            line(context, 8.25, 6.25, 5.5, 3.5)
            line(context, 12.5, 3.5, 12.5, 14.5)
            line(context, 9.75, 11.75, 12.5, 14.5)
            line(context, 15.25, 11.75, 12.5, 14.5)
            break
        case "range-eq":
            line(context, 2.25, 14.5, 15.75, 14.5)
            context.beginPath()
            context.moveTo(2.5, 10.75)
            context.bezierCurveTo(4.5, 10.75, 4.75, 5, 7.1, 5)
            context.bezierCurveTo(9.4, 5, 9.6, 11.25, 12, 11.25)
            context.bezierCurveTo(13.5, 11.25, 14.25, 8.25, 15.5, 8.25)
            context.stroke()
            context.beginPath()
            context.arc(7.1, 5, 1, 0, Math.PI * 2)
            context.fill()
            context.beginPath()
            context.arc(12, 11.25, 1, 0, Math.PI * 2)
            context.fill()
            break
        case "settings":
            context.beginPath()
            for (let tooth = 0; tooth < 8; ++tooth) {
                const centerAngle = -Math.PI / 2 + tooth * Math.PI / 4
                const angles = [-22.5, -14, -10, 10, 14, 22.5]
                const radii = [5.25, 5.25, 7.15, 7.15, 5.25, 5.25]
                for (let point = 0; point < angles.length; ++point) {
                    const angle = centerAngle + angles[point] * Math.PI / 180
                    const x = 9 + Math.cos(angle) * radii[point]
                    const y = 9 + Math.sin(angle) * radii[point]
                    if (tooth === 0 && point === 0)
                        context.moveTo(x, y)
                    else
                        context.lineTo(x, y)
                }
            }
            context.closePath()
            context.stroke()
            context.beginPath()
            context.arc(9, 9, 2.35, 0, Math.PI * 2)
            context.stroke()
            break
        }
    }
}
