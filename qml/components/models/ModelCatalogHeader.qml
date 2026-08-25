import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    property string searchText: ""
    property string selectedCategory: "all"

    signal searchChanged(string query)
    signal categoryChanged(string cat)

    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border
    border.width: 1
    implicitHeight: 64

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        TextField {
            Layout.preferredWidth: 280
            placeholderText: qsTr("Tìm kiếm mô hình AI...")
            onTextChanged: root.searchChanged(text)
        }

        RowLayout {
            spacing: 6

            AppButton {
                text: qsTr("Tất Cả")
                checkable: true
                checked: root.selectedCategory === "all"
                onClicked: root.categoryChanged("all")
            }
            AppButton {
                text: qsTr("Text-To-Speech")
                checkable: true
                checked: root.selectedCategory === "tts"
                onClicked: root.categoryChanged("tts")
            }
            AppButton {
                text: qsTr("Speech-To-Text")
                checkable: true
                checked: root.selectedCategory === "stt"
                onClicked: root.categoryChanged("stt")
            }
            AppButton {
                text: qsTr("AI Translation / LLM")
                checkable: true
                checked: root.selectedCategory === "llm"
                onClicked: root.categoryChanged("llm")
            }
            AppButton {
                text: qsTr("Vocal Separation / OCR")
                checkable: true
                checked: root.selectedCategory === "separation"
                onClicked: root.categoryChanged("separation")
            }
        }

        Item { Layout.fillWidth: true }
    }
}
