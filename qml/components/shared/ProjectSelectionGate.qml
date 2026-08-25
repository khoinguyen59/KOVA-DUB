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
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                LineIcon { anchors.centerIn: parent; name: "folder"; color: Theme.accentLight; width: 21; height: 21 }
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
                    text: qsTr("%1 sẽ sẵn sàng sau khi bạn chọn hoặc tạo một dự án làm việc.").arg(root.requestedFeatureLabel)
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

            // Quick Create Bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.40)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingMedium

                    LineIcon { name: "plus-circle"; color: Theme.accentLight; width: 24; height: 24 }

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
                            text: qsTr("Lưu tự động vào thư mục projects/ của ứng dụng mà không cần chọn đường dẫn.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontXSmall
                        }
                    }

                    PrimaryButton {
                        objectName: "globalProjectCreateButton"
                        text: qsTr("Create new project")
                        iconName: "plus"
                        Layout.preferredWidth: 160
                        onClicked: root.createAutoProject("")
                    }
                }
            }

            // Recent Projects Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                LineIcon { name: "clock"; color: Theme.textSecondary; width: 16; height: 16 }
                Text {
                    text: qsTr("Open existing project") + " (" + (AppController.dubbing && AppController.dubbing.history ? AppController.dubbing.history.length : 0) + ")"
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: Theme.fontMedium
                }
                Item { Layout.fillWidth: true }
                PrimaryButton {
                    text: qsTr("Làm Mới")
                    iconName: "refresh-cw"
                    quiet: true
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
                        height: 58
                        radius: Theme.radiusSmall
                        color: itemMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Qt.rgba(1, 1, 1, 0.03)
                        border.color: itemMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.50) : Qt.rgba(1, 1, 1, 0.06)

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
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                radius: Theme.radiusSmall
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                LineIcon { anchors.centerIn: parent; name: "file-text"; color: Theme.accentLight; width: 18; height: 18 }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: Theme.paddingSmall
                                    Text {
                                        text: modelData.projectName || modelData.fileName || qsTr("Dự án không tên")
                                        color: Theme.textPrimary
                                        font.bold: true
                                        font.pixelSize: Theme.fontMedium
                                        elide: Text.ElideRight
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
                                            text: qsTr("%1 câu thoại").arg(modelData.segmentCount || 0)
                                            color: Theme.accentLight
                                            font.pixelSize: 10
                                        }
                                    }
                                }

                                RowLayout {
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
                                    }
                                }
                            }

                            PrimaryButton {
                                text: qsTr("Mở")
                                iconName: "play"
                                Layout.preferredWidth: 80
                                onClicked: {
                                    var path = modelData.projectPath || modelData.path || modelData.id
                                    if (path) root.openProject(path)
                                }
                            }

                            PrimaryButton {
                                text: qsTr("Xóa")
                                iconName: "trash"
                                quiet: true
                                Layout.preferredWidth: 70
                                onClicked: {
                                    var id = modelData.id || modelData.projectPath || modelData.path
                                    if (id && AppController.dubbing) AppController.dubbing.deleteHistoryItem(id)
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
                            text: qsTr("Chưa có dự án nào được tạo trước đó.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMedium
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Nhấn 'Tạo Mới Ngay' ở trên để bắt đầu dự án đầu tiên của bạn.")
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
                    text: qsTr("Mở file từ vị trí khác...")
                    iconName: "folder"
                    quiet: true
                    onClicked: openProjectFileDialog.open()
                }

                Item { Layout.fillWidth: true }

                PrimaryButton {
                    objectName: "globalProjectLeaveButton"
                    text: qsTr("Về Trang Chủ")
                    iconName: "arrow-left"
                    quiet: true
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
