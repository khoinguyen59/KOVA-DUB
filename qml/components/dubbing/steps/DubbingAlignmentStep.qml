import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal runRequested(string nodeId)
    signal openAlignmentStudioRequested()
    signal previousStepRequested()
    signal nextStepRequested()

    readonly property bool hasTranscript: (root.dubbing.segments || []).length > 0
    readonly property bool canRun: !root.dubbing.processing
                                     && root.hasTranscript
                                     && (root.dubbing.normalizedAudioPath || "").length > 0
    readonly property int timingConflictCount: (root.dubbing && root.dubbing.timingConflicts) ? root.dubbing.timingConflicts.length : 0
    readonly property bool hasConflicts: timingConflictCount > 0

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
            LineIcon { name: "alignment"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            Text {
                Layout.fillWidth: true
                text: qsTr("7. ALIGN · TEXT & TIMING")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Theme.radiusSmall
                color: root.hasTranscript
                       ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08)
                       : Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.08)
                border.color: root.hasTranscript ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30)
                                                   : Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.30)
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingSmall
                    spacing: Theme.paddingSmall
                    LineIcon { name: root.hasTranscript ? "check" : "alert"; color: root.hasTranscript ? Theme.success : Theme.warning; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%1 segments").arg((root.dubbing.segments || []).length)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: 128
                Layout.preferredHeight: 48
                radius: Theme.radiusSmall
                color: root.hasConflicts ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.10) : Qt.rgba(1, 1, 1, 0.03)
                border.color: root.hasConflicts ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.34) : Qt.rgba(1, 1, 1, 0.08)
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("CONFLICTS"); color: Theme.textSecondary; font.pixelSize: 9; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: String(root.timingConflictCount); color: root.hasConflicts ? Theme.warning : Theme.success; font.pixelSize: Theme.fontSmall; font.bold: true }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Audio balance")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("Original %1%").arg(originalAudioSlider.value)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.preferredWidth: 94
            }
            Slider {
                id: originalAudioSlider
                objectName: "dubbingOriginalAudioLevelSlider"
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 5
                value: root.dubbing.audioMixConfiguration
                       && root.dubbing.audioMixConfiguration.originalGainPercent !== undefined
                       ? root.dubbing.audioMixConfiguration.originalGainPercent : 0
                onMoved: root.dubbing.setAudioMixLevels(Math.round(value), Math.round(dubbedAudioSlider.value))
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("Dubbed %1%").arg(dubbedAudioSlider.value)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.preferredWidth: 94
            }
            Slider {
                id: dubbedAudioSlider
                objectName: "dubbingDubbedAudioLevelSlider"
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 5
                value: root.dubbing.audioMixConfiguration
                       && root.dubbing.audioMixConfiguration.dubbedGainPercent !== undefined
                       ? root.dubbing.audioMixConfiguration.dubbedGainPercent : 100
                onMoved: root.dubbing.setAudioMixLevels(Math.round(originalAudioSlider.value), Math.round(value))
            }
        }

        PrimaryButton {
            objectName: "dubbingRunAlignmentButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            text: root.stepComplete ? qsTr("Re-run alignment") : qsTr("Run alignment")
            iconName: root.dubbing.processing ? "activity" : "alignment"
            loading: root.dubbing.processing
            buttonColor: Theme.accent
            enabled: root.canRun
            onClicked: root.runRequested("fit-timing")
        }

        PrimaryButton {
            objectName: "dubbingOpenAlignmentStudioButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            text: qsTr("Alignment Studio")
            iconName: "edit"
            quiet: true
            enabled: root.canRun
            onClicked: root.openAlignmentStudioRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            PrimaryButton {
                text: qsTr("Back")
                iconName: "chevron-left"
                quiet: true
                Layout.preferredWidth: 100
                Layout.preferredHeight: 38
                onClicked: root.previousStepRequested()
            }
            PrimaryButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: qsTr("Continue")
                iconName: "chevron-right"
                buttonColor: Theme.accent
                enabled: !root.dubbing.processing && root.hasTranscript && !root.hasConflicts
                onClicked: root.nextStepRequested()
            }
        }
    }
}
