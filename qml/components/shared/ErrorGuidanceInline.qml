import QtQuick
import QtQuick.Layouts
import LAStudio
import "../base"

Item {
    id: root

    property string message: ""
    property string source: ""
    property bool compact: false
    readonly property var presentation: AppController.explainError(root.message, root.source)

    visible: root.message.trim().length > 0
    implicitHeight: visible ? card.implicitHeight : 0
    Layout.fillWidth: true

    signal actionRequested(string route)

    Rectangle {
        id: card
        anchors.fill: parent
        implicitHeight: content.implicitHeight + Theme.paddingMedium * 2
        radius: Theme.radiusSmall
        color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.08)
        border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.36)
        border.width: 1

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            Text {
                Layout.fillWidth: true
                text: root.presentation.title || qsTr("Tác vụ chưa hoàn tất")
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: root.presentation.summary || root.message
                color: Theme.textSecondary
                font.pixelSize: root.compact ? Theme.fontXSmall : Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: !root.compact
                text: root.presentation.guidance || ""
                color: Theme.textMuted
                font.pixelSize: Theme.fontXSmall
                wrapMode: Text.WordWrap
            }

            PrimaryButton {
                visible: (root.presentation.actionRoute || "") !== ""
                text: root.presentation.actionLabel || qsTr("Mở cấu hình")
                iconName: "settings"
                quiet: true
                Layout.alignment: Qt.AlignLeft
                onClicked: {
                    root.actionRequested(root.presentation.actionRoute)
                    AppController.workflows.openStudioRoute(root.presentation.actionRoute)
                }
            }
        }
    }
}
