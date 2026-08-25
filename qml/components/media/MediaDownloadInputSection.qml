import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    property string inputUrl: ""
    property string selectedQuality: "best"
    property bool downloading: false

    signal downloadRequested(string url, string quality)

    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border
    border.width: 1
    implicitHeight: 120

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            TextField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: qsTr("Dán liên kết YouTube, Douyin, TikTok, Facebook Video tại đây...")
                text: root.inputUrl
                onTextChanged: root.inputUrl = text
            }

            ComboBox {
                Layout.preferredWidth: 140
                model: [
                    { text: qsTr("Chất lượng cao nhất"), value: "best" },
                    { text: qsTr("1080p Full HD"), value: "1080p" },
                    { text: qsTr("720p HD"), value: "720p" },
                    { text: qsTr("Chỉ tải Âm thanh"), value: "audio" }
                ]
                textRole: "text"
                valueRole: "value"
                onActivated: function(index) {
                    root.selectedQuality = model[index].value
                }
            }

            AppButton {
                text: root.downloading ? qsTr("Đang Tải...") : qsTr("Tải Xuống")
                primary: true
                enabled: urlField.text.trim().length > 0 && !root.downloading
                onClicked: {
                    root.downloadRequested(urlField.text.trim(), root.selectedQuality)
                    urlField.text = ""
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Hỗ trợ tự động phân tích định dạng, âm thanh không nén và trích xuất video trực tiếp.")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
