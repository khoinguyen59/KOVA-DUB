import QtQuick
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal nextStepRequested()
    signal previousStepRequested()

    function hasRenderableAudio() {
        if ((root.dubbing.dubbedVocalPath || "").length > 0) return true
        const entries = root.dubbing.segments || []
        for (let i = 0; i < entries.length; ++i) {
            if ((entries[i].clipPath || "").length > 0) return true
        }
        return false
    }

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
            LineIcon { name: "sliders"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("9. TRỘN ÂM THANH (MIX AUDIO)")
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
            implicitHeight: infoLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: (root.dubbing.dubbedVocalPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: (root.dubbing.dubbedVocalPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1

            RowLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                LineIcon {
                    name: root.dubbing.dubbedVocalPath.length > 0 ? "check" : "refresh-cw"
                    color: root.dubbing.dubbedVocalPath.length > 0 ? Theme.success : Theme.textSecondary
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                Text {
                    Layout.fillWidth: true
                    text: root.dubbing.dubbedVocalPath.length > 0
                          ? qsTr("Bản thu âm lồng tiếng đã hoàn tất: %1").arg(root.dubbing.dubbedVocalPath)
                          : qsTr("Chưa trộn âm. Hãy hoàn thành bước lồng tiếng TTS trước.")
                    color: root.dubbing.dubbedVocalPath.length > 0 ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                }
            }
        }

        // Action Controls & Run Buttons (Clean 2-row layout with zero horizontal overflow)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (Full width)
            PrimaryButton {
                id: runMixBtn
                text: (root.dubbing.dubbedVocalPath || "").length > 0
                      ? qsTr("⚡ Trộn Lại Âm Thanh (Remix)")
                      : qsTr("⚡ Chạy Trộn Âm Thanh (Mix Audio)")
                iconName: root.dubbing.processing ? "activity" : "sliders"
                loading: root.dubbing.processing
                enabled: !root.dubbing.processing && root.hasRenderableAudio()
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                buttonColor: Theme.accent
                onClicked: root.dubbing.runWorkflowNode("mix")
            }

            // Row 2: Navigation Buttons (Quay lại & Tiếp tục)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                PrimaryButton {
                    text: qsTr("⬅ Quay lại")
                    iconName: "chevron-left"
                    quiet: true
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 100
                    onClicked: root.previousStepRequested()
                }

                PrimaryButton {
                    text: qsTr("Tiếp tục: Xuất File ➔")
                    iconName: "chevron-right"
                    buttonColor: Theme.accent
                    enabled: !root.dubbing.processing && (root.dubbing.previewPath || "").length > 0
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }
            }
        }
    }
}
