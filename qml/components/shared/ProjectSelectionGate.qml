import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LAStudio
import "../base"

// Project selection is global application chrome.
// Automatically manages projects in a dedicated default workspace folder
// and presents interactive recent projects list for instant opening.
Dialog {
    id: root

    property string requestedFeatureLabel: ""
    property string actionError: ""
    signal projectReady()
    signal leaveRequested()

    function openFor(featureLabel) {
        requestedFeatureLabel = featureLabel || qsTr("this feature")
        actionError = ""
        if (AppController.dubbing)
            AppController.dubbing.refreshHistory()
        open()
    }

    function createAutoProject(name) {
        actionError = ""
        if (!AppController.dubbing) {
            actionError = qsTr("Dubbing engine is initializing.")
            return
        }
        if (!AppController.dubbing.createAutoProject(name || "")) {
            actionError = AppController.dubbing.lastError || qsTr("LA Studio could not create the project.")
            return
        }
        close()
        projectReady()
    }

    function createProject(url) {
        if (!url || url.toString() === "") {
            createAutoProject("")
            return
        }
        actionError = ""
        if (!AppController.dubbing) {
            actionError = qsTr("Dubbing engine is initializing.")
            return
        }
        if (!AppController.dubbing.newProject(url.toString())) {
            actionError = AppController.dubbing.lastError || qsTr("LA Studio could not create the project.")
            return
        }
        close()
        projectReady()
    }

    function openProject(urlOrPath) {
        actionError = ""
        if (!AppController.dubbing) {
            actionError = qsTr("Dubbing engine is initializing.")
            return
        }
        if (!AppController.dubbing.openProject(urlOrPath.toString())) {
            actionError = AppController.dubbing.lastError || qsTr("LA Studio could not open the project.")
            return
        }
        close()
        projectReady()
    }

    objectName: "globalProjectGate"
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    width: Math.min(760, Math.max(560, parent ? parent.width - Theme.paddingXL * 2 : 760))
    height: Math.min(620, Math.max(480, parent ? parent.height - Theme.paddingXL * 2 : 620))

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.64)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: Theme.radiusMedium
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)
                border.width: 1
                LineIcon { anchors.centerIn: parent; name: "folder"; color: Theme.accentLight; width: 22; height: 22 }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: qsTr("Choose an LA Studio project first")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontXLarge
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 will be ready once you select or create a working project.").arg(root.requestedFeatureLabel)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        // Main content area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            // Quick Create Card
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingMedium
                    anchors.rightMargin: Theme.paddingMedium
                    spacing: Theme.paddingMedium

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 18
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                        LineIcon { anchors.centerIn: parent; name: "plus-circle"; color: Theme.accentLight; width: 20; height: 20 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Create new project")
                            color: Theme.textPrimary
                            font.bold: true
                            font.pixelSize: Theme.fontMedium
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Automatically saves to the application projects folder without choosing a path.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontXSmall
                            elide: Text.ElideRight
                        }
                    }

                    PrimaryButton {
                        objectName: "globalProjectCreateButton"
                        text: qsTr("Create new project")
                        iconName: "plus"
                        Layout.preferredWidth: 165
                        onClicked: root.createAutoProject("")
                    }
                }
            }

            // Recent Projects Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                LineIcon { name: "clock"; color: Theme.accentLight; width: 16; height: 16 }
                Text {
                    text: qsTr("Open existing project (%1)").arg(AppController.dubbing && AppController.dubbing.history ? AppController.dubbing.history.length : 0)
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: Theme.fontMedium
                }
                Item { Layout.fillWidth: true }
                PrimaryButton {
                    text: qsTr("Refresh")
                    iconName: "refresh-cw"
                    quiet: true
                    Layout.preferredWidth: 95
                    onClicked: if (AppController.dubbing) AppController.dubbing.refreshHistory()
                }
            }

            // Recent Projects List
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusSmall
                color: Qt.rgba(0, 0, 0, 0.25)
                border.color: Qt.rgba(1, 1, 1, 0.10)
                clip: true

                ListView {
                    id: historyList
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4
                    model: AppController.dubbing && AppController.dubbing.history ? AppController.dubbing.history : []

                    delegate: Rectangle {
                        id: itemDelegate
                        width: historyList.width
                        height: 60
                        radius: Theme.radiusSmall
                        color: itemMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : Qt.rgba(1, 1, 1, 0.03)
                        border.color: itemMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45) : Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var path = modelData.projectPath || modelData.path || modelData.id
                                if (path) root.openProject(path)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingMedium
                            anchors.rightMargin: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: Theme.radiusSmall
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                LineIcon { anchors.centerIn: parent; name: "file-text"; color: Theme.accentLight; width: 18; height: 18 }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall
                                    Text {
                                        text: modelData.projectName || modelData.fileName || qsTr("Untitled Project")
                                        color: Theme.textPrimary
                                        font.bold: true
                                        font.pixelSize: Theme.fontMedium
                                        elide: Text.ElideRight
                                        Layout.maximumWidth: 340
                                    }
                                    Rectangle {
                                        visible: !!modelData.segmentCount && modelData.segmentCount > 0
                                        Layout.preferredHeight: 18
                                        implicitWidth: segText.implicitWidth + 8
                                        radius: 3
                                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                                        Text {
                                            id: segText
                                            anchors.centerIn: parent
                                            text: qsTr("%1 cues").arg(modelData.segmentCount || 0)
                                            color: Theme.accentLight
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingMedium
                                    Text {
                                        text: modelData.timestamp || modelData.lastOpened || ""
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontXSmall
                                    }
                                    Text {
                                        visible: !!modelData.sourceName && modelData.sourceName !== ""
                                        text: qsTr("Video: %1").arg(modelData.sourceName || "")
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontXSmall
                                        elide: Text.ElideRight
                                        Layout.maximumWidth: 260
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                spacing: Theme.paddingSmall

                                PrimaryButton {
                                    text: qsTr("Open")
                                    iconName: "play"
                                    Layout.preferredWidth: 80
                                    onClicked: {
                                        var path = modelData.projectPath || modelData.path || modelData.id
                                        if (path) root.openProject(path)
                                    }
                                }

                                PrimaryButton {
                                    text: qsTr("Delete")
                                    iconName: "trash"
                                    quiet: true
                                    Layout.preferredWidth: 72
                                    onClicked: {
                                        var id = modelData.id || modelData.projectPath || modelData.path
                                        if (id && AppController.dubbing) AppController.dubbing.deleteHistoryItem(id)
                                    }
                                }
                            }
                        }
                    }

                    // Empty state
                    ColumnLayout {
                        anchors.centerIn: parent
                        visible: historyList.count === 0
                        spacing: Theme.paddingSmall
                        LineIcon { Layout.alignment: Qt.AlignHCenter; name: "folder"; color: Theme.textMuted; width: 36; height: 36 }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No recent projects found.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMedium
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Click 'Create new project' above to start your first project.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }
            }

            // Error display
            Text {
                visible: root.actionError !== ""
                Layout.fillWidth: true
                text: root.actionError
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            // Bottom Action Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                PrimaryButton {
                    objectName: "globalProjectBrowseButton"
                    text: qsTr("Open file from other location...")
                    iconName: "folder"
                    quiet: true
                    Layout.preferredWidth: 200
                    onClicked: openProjectFileDialog.open()
                }

                Item { Layout.fillWidth: true }

                PrimaryButton {
                    objectName: "globalProjectLeaveButton"
                    text: qsTr("Back to Home")
                    iconName: "arrow-left"
                    quiet: true
                    Layout.preferredWidth: 140
                    onClicked: {
                        root.close()
                        root.leaveRequested()
                    }
                }
            }
        }
    }

    FileDialog {
        id: openProjectFileDialog
        objectName: "globalProjectOpenFileDialog"
        title: qsTr("Open LA Studio project")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("LA Studio project (*.ladub.json)"), qsTr("All files (*)")]
        onAccepted: root.openProject(selectedFile)
    }
}
