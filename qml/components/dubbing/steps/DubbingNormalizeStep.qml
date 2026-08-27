import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    property string currentPlayingAudioPath: ""

    signal nextStepRequested()
    signal previousStepRequested()

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (AppController.player && !AppController.player.playing) {
                root.currentPlayingAudioPath = ""
            }
        }
        function onPlaybackFinished() {
            root.currentPlayingAudioPath = ""
        }
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon { name: "activity"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("2. CHUẨN HÓA ÂM THANH (NORMALIZE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    text: qsTr("Chuẩn bị master 48kHz stereo và analysis 16kHz mono cho các bước tiếp theo.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: infoLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: (root.dubbing.normalizedAudioPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: (root.dubbing.normalizedAudioPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1

            RowLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                LineIcon {
                    name: (root.dubbing.normalizedAudioPath || "").length > 0 ? "check" : "refresh-cw"
                    color: (root.dubbing.normalizedAudioPath || "").length > 0 ? Theme.success : Theme.textSecondary
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: (root.dubbing.normalizedAudioPath || "").length > 0
                              ? qsTr("Đã chuẩn hóa thành công (master 48kHz stereo / analysis 16kHz mono)")
                              : qsTr("Chưa chuẩn hóa âm thanh")
                        color: (root.dubbing.normalizedAudioPath || "").length > 0 ? Theme.success : Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        id: normalizedPathText
                        Layout.fillWidth: true
                        text: (root.dubbing.normalizedAudioPath || "").length > 0
                              ? root.dubbing.normalizedAudioPath
                              : qsTr("Nhấn 'Chạy Chuẩn Hóa Âm Thanh' bên dưới để trích xuất master audio.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                        HoverHandler { id: normalizedPathHover }
                        ToolTip.visible: normalizedPathHover.hovered && (root.dubbing.normalizedAudioPath || "").length > 0
                        ToolTip.text: root.dubbing.normalizedAudioPath
                    }
                }
            }
        }

        // Action Controls & Run Buttons (Clean 2-row layout with zero horizontal overflow)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (and optional Listen button)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                PrimaryButton {
                    id: runNormalizeBtn
                    text: (root.dubbing.normalizedAudioPath || "").length > 0
                          ? qsTr("⚡ Chuẩn Hóa Lại Âm Thanh")
                          : qsTr("⚡ Chạy Chuẩn Hóa Âm Thanh (PCM WAV)")
                    iconName: root.dubbing.processing ? "activity" : "play"
                    loading: root.dubbing.processing
                    enabled: !root.dubbing.processing && (root.dubbing.sourceMediaPath || "").length > 0
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    buttonColor: Theme.accent
                    onClicked: root.dubbing.runWorkflowNode("ingest")
                }

                PrimaryButton {
                    visible: (root.dubbing.normalizedAudioPath || "").length > 0
                    text: AppController.player.playing && root.currentPlayingAudioPath === root.dubbing.normalizedAudioPath
                          ? qsTr("Dừng") : qsTr("Nghe thử")
                    iconName: AppController.player.playing && root.currentPlayingAudioPath === root.dubbing.normalizedAudioPath
                              ? "pause" : "play"
                    buttonColor: AppController.player.playing && root.currentPlayingAudioPath === root.dubbing.normalizedAudioPath
                                 ? Theme.warning : Theme.accent
                    quiet: !(AppController.player.playing && root.currentPlayingAudioPath === root.dubbing.normalizedAudioPath)
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 88
                    onClicked: {
                        if (AppController.player.playing && root.currentPlayingAudioPath === root.dubbing.normalizedAudioPath) {
                            AppController.player.stop()
                            root.currentPlayingAudioPath = ""
                        } else {
                            root.currentPlayingAudioPath = root.dubbing.normalizedAudioPath
                            AppController.player.playFile(root.dubbing.normalizedAudioPath)
                        }
                    }
                }
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
                    text: qsTr("Tiếp tục: Tách Giọng ➔")
                    iconName: "chevron-right"
                    buttonColor: Theme.accent
                    enabled: !root.dubbing.processing && (root.dubbing.normalizedAudioPath || "").length > 0
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }
            }
        }
    }
}
