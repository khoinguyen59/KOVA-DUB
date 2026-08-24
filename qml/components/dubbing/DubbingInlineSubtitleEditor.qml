pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

// Small, context-first caption editor.  It is opened only after the operator
// clicks a rendered subtitle in the preview; the full DubbingSubtitleEditor
// remains the explicit place for style/import/export configuration.
Popup {
    id: root

    required property var dubbing
    property int segmentIndex: -1
    property bool editingTargetText: false

    modal: false
    focus: true
    parent: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(540, parent ? parent.width - Theme.paddingXL * 2 : 540)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.max(Theme.paddingLarge, Math.round((parent.height - height) * 0.62)) : 0
    padding: Theme.paddingMedium

    readonly property var segment: segmentIndex >= 0 && segmentIndex < dubbing.segments.length
                                   ? dubbing.segments[segmentIndex] : ({})
    readonly property string fieldLabel: editingTargetText
                                         ? qsTr("Translated caption") : qsTr("Reviewed source caption")

    function openForSegment(index) {
        if (index < 0 || index >= dubbing.segments.length)
            return
        segmentIndex = index
        editingTargetText = (dubbing.segments[index].targetText || "").trim().length > 0
        captionText.text = editingTargetText
                ? (dubbing.segments[index].targetText || "")
                : (dubbing.segments[index].sourceText || "")
        open()
        captionText.forceActiveFocus()
        captionText.selectAll()
    }

    background: Rectangle {
        color: Theme.surfaceAlt
        radius: Theme.radiusMedium
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.65)
        border.width: 1
    }

    contentItem: ColumnLayout {
        width: parent.width
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Edit subtitle")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }
            Text {
                text: root.segmentIndex >= 0
                      ? qsTr("Segment %1").arg(root.segmentIndex + 1) : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.fieldLabel
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
        TextArea {
            id: captionText
            objectName: "dubbingInlineSubtitleText"
            Layout.fillWidth: true
            Layout.minimumHeight: 90
            Layout.preferredHeight: Math.max(90, contentHeight + Theme.paddingMedium * 2)
            wrapMode: Text.Wrap
            selectByMouse: true
            color: Theme.textPrimary
            placeholderText: qsTr("Type the caption shown at this moment")
            placeholderTextColor: Theme.textSecondary
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Qt.rgba(1, 1, 1, 0.035)
                border.color: captionText.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.12)
                border.width: captionText.activeFocus ? 2 : 1
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            PrimaryButton {
                objectName: "dubbingInlineSubtitleSave"
                text: qsTr("Save subtitle")
                enabled: root.segmentIndex >= 0 && captionText.text.trim().length > 0
                onClicked: {
                    var patch = root.editingTargetText
                            ? { targetText: captionText.text.trim() }
                            : { sourceText: captionText.text.trim() }
                    root.dubbing.updateSegment(root.segmentIndex, patch)
                    root.close()
                }
            }
        }
    }
}
