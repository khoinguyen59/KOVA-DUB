import QtQuick
import QtQuick.Layouts
import QtMultimedia
import "../../base"
import LAStudio

Rectangle {
    id: root

    property var ocr: AppController.subtitleOcr
    property var player
    property real displayedWidth: 0
    property real displayedHeight: 0
    property real displayedX: 0
    property real displayedY: 0
    property real draftRoiX: 0
    property real draftRoiY: 0
    property real draftRoiWidth: 0
    property real draftRoiHeight: 0
    property bool roiDragActive: false

    signal roiCommitted(real x, real y, real width, real height)
    signal videoPicked()

    color: "#0a0c10"
    radius: Theme.radiusMedium
    clip: true

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    // ROI Selection Overlay Box
    Rectangle {
        id: roiBox
        visible: root.ocr && root.ocr.hasSource && root.displayedWidth > 0
        x: root.displayedX + root.draftRoiX * root.displayedWidth
        y: root.displayedY + root.draftRoiY * root.displayedHeight
        width: Math.max(16, root.draftRoiWidth * root.displayedWidth)
        height: Math.max(16, root.draftRoiHeight * root.displayedHeight)
        color: Qt.rgba(0.2, 0.6, 1.0, 0.15)
        border.color: Theme.accent
        border.width: 2
        radius: 2

        Text {
            anchors.left: parent.left
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            text: qsTr("Vùng nhận diện phụ đề (ROI)")
            color: Theme.accentLight
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
        }
    }

    // Empty state placeholder
    ColumnLayout {
        anchors.centerIn: parent
        visible: !root.ocr || !root.ocr.hasSource
        spacing: Theme.paddingMedium

        LineIcon {
            Layout.alignment: Qt.AlignHCenter
            name: "video"
            size: 48
            color: Theme.textTertiary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Chọn video để bắt đầu bóc tách phụ đề")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontMedium
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Mở Video...")
            primary: true
            onClicked: root.videoPicked()
        }
    }
}
