import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, Math.max(420, parent ? parent.width - 48 : 640))
    height: Math.min(Math.max(340, parent ? Math.round(parent.height * 0.72) : 520),
                     parent ? parent.height - 32 : 720)
    modal: true
    focus: true
    padding: Theme.paddingLarge
    closePolicy: Popup.NoAutoClose

    property bool technicalDetailsExpanded: false
    property string reportStatus: ""
    readonly property var currentError: AppController.currentError
    readonly property bool hasRouteAction: !!currentError
        && (currentError.actionRoute || "") !== ""

    function dismissCurrent() {
        if (AppController.pendingErrorCount > 0)
            AppController.clearError()
        root.close()
    }

    function copyTechnicalDetails() {
        const details = currentError.technicalDetails || currentError.message || ""
        if (details !== "") {
            AppController.copyToClipboard(details)
            root.reportStatus = qsTr("Đã sao chép chi tiết kỹ thuật.")
        }
    }

    function createSupportReport() {
        const reportPath = AppController.createProblemReport()
        root.reportStatus = reportPath !== ""
            ? qsTr("Đã tạo báo cáo hỗ trợ: %1").arg(reportPath)
            : qsTr("Chưa tạo được báo cáo hỗ trợ. Hãy kiểm tra thư mục log.")
    }

    signal actionRequested(string route)

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.danger
        border.width: 2

        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            color: "#00000025"
            radius: Theme.radiusMedium + 8
            z: -1
        }
    }

    contentItem: ColumnLayout {
        id: contentLayout
        focus: true
        spacing: Theme.paddingMedium

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape) {
                event.accepted = true
                root.dismissCurrent()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingMedium

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
                border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.40)
                border.width: 1

                LineIcon {
                    anchors.centerIn: parent
                    name: "alert"
                    color: Theme.danger
                    width: 20
                    height: 20
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: currentError.title || qsTr("Tác vụ chưa hoàn tất")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: AppController.pendingErrorCount > 1
                          ? qsTr("Còn %1 lỗi đang chờ xử lý").arg(AppController.pendingErrorCount - 1)
                          : qsTr("Lỗi đã được ghi vào log kỹ thuật")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: currentError.summary || currentError.message || qsTr("Không có mô tả lỗi.")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.surfaceAlt
        }

        ScrollView {
            id: guidanceScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: guidanceScroll.availableWidth
                spacing: Theme.paddingSmall

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Cách xử lý")
                    color: Theme.accentLight
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: currentError.guidance || qsTr("Kiểm tra cấu hình, tệp đầu vào và chạy lại bước hiện tại.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }

                Button {
                    Layout.fillWidth: true
                    checkable: true
                    checked: root.technicalDetailsExpanded
                    text: root.technicalDetailsExpanded
                          ? qsTr("Ẩn chi tiết kỹ thuật")
                          : qsTr("Xem chi tiết kỹ thuật")
                    Accessible.name: text
                    onToggled: root.technicalDetailsExpanded = checked

                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                        border.color: Qt.rgba(1, 1, 1, 0.10)
                        border.width: 1
                        radius: Theme.radiusSmall
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.technicalDetailsExpanded
                    implicitHeight: technicalDetailsText.implicitHeight + Theme.paddingSmall * 2
                    color: Qt.rgba(0, 0, 0, 0.20)
                    radius: Theme.radiusSmall
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Text {
                        id: technicalDetailsText
                        anchors.fill: parent
                        anchors.margins: Theme.paddingSmall
                        text: currentError.technicalDetails || currentError.message || ""
                        color: Theme.textMuted
                        font.family: "Consolas"
                        font.pixelSize: Theme.fontXSmall
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.reportStatus !== ""
            text: root.reportStatus
            color: Theme.textSecondary
            font.pixelSize: Theme.fontXSmall
            wrapMode: Text.WrapAnywhere
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            PrimaryButton {
                visible: root.hasRouteAction
                text: currentError.actionLabel || qsTr("Mở cấu hình")
                iconName: "settings"
                onClicked: root.actionRequested(currentError.actionRoute)
            }

            Item { Layout.fillWidth: true }

            PrimaryButton {
                text: qsTr("Sao chép lỗi")
                iconName: "copy"
                quiet: true
                onClicked: root.copyTechnicalDetails()
            }

            PrimaryButton {
                text: qsTr("Tạo báo cáo")
                iconName: "file"
                quiet: true
                onClicked: root.createSupportReport()
            }

            PrimaryButton {
                id: dismissButton
                text: AppController.pendingErrorCount > 1 ? qsTr("Bỏ qua & tiếp tục") : qsTr("Đóng")
                iconName: "close"
                onClicked: root.dismissCurrent()
            }
        }
    }

    onOpened: Qt.callLater(function() { dismissButton.forceActiveFocus() })
    onClosed: root.technicalDetailsExpanded = false
}
