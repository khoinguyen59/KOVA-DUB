import QtQuick
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal openExportDialogRequested()
    signal previousStepRequested()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon { name: "share"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("8. MIX & EXPORT")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: exportInfoLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: ((root.dubbing.exportPath || "").length > 0 || (root.dubbing.previewPath || "").length > 0) ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: ((root.dubbing.exportPath || "").length > 0 || (root.dubbing.previewPath || "").length > 0) ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                id: exportInfoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    LineIcon {
                        name: root.dubbing.exportPath.length > 0 ? "check-circle" : "film"
                        color: root.dubbing.exportPath.length > 0 ? Theme.success : Theme.accentLight
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.dubbing.exportPath.length > 0
                              ? qsTr("Tệp thành phẩm: %1").arg(root.dubbing.exportPath)
                              : (root.dubbing.previewPath.length > 0 ? qsTr("Tệp xem trước: %1").arg(root.dubbing.previewPath) : qsTr("Chưa xuất bản video thành phẩm."))
                        color: root.dubbing.exportPath.length > 0 ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }
                }

                // Main Action Button
                PrimaryButton {
                    text: qsTr("Export")
                    iconName: "download"
                    buttonColor: Theme.accent
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    enabled: !root.dubbing.processing && (root.dubbing.sourceMediaPath || "").length > 0
                    onClicked: root.openExportDialogRequested()
                }

                PrimaryButton {
                    objectName: "dubbingOpenCapCutDraftButton"
                    visible: (root.dubbing.capCutDraftPath || "").length > 0
                    text: qsTr("Open in CapCut")
                    iconName: "external-link"
                    quiet: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    enabled: !root.dubbing.processing
                    onClicked: root.dubbing.openCapCutDraft()
                }

                // Navigation Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        text: qsTr("⬅ Quay lại")
                        iconName: "chevron-left"
                        quiet: true
                        Layout.preferredHeight: 38
                        Layout.fillWidth: true
                        onClicked: root.previousStepRequested()
                    }
                }
            }
        }
    }
}
