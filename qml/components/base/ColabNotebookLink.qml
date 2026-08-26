import QtQuick
import QtQuick.Layouts
import LAStudio
import ".."
import "colabNotebookUrls.js" as ColabNotebookUrls

ColumnLayout {
    id: root

    property string notebookFile: ""
    // Internal builds publish these notebooks to the GitHub branch used by
    // this application. Colab opens that exact file directly; no worker token
    // is ever part of this URL.
    readonly property string colabNotebookUrl: ColabNotebookUrls.forNotebookFile(notebookFile)

    Layout.fillWidth: true
    spacing: Theme.paddingSmall

    Text {
        Layout.fillWidth: true
        visible: root.notebookFile !== ""
        text: qsTr("Notebook: %1").arg(root.notebookFile)
        color: Theme.textSecondary
        font.pixelSize: 11
        elide: Text.ElideMiddle
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.paddingSmall

        PrimaryButton {
            Layout.fillWidth: true
            text: qsTr("Open this notebook in Colab")
            iconName: "cloud"
            quiet: true
            onClicked: Qt.openUrlExternally(root.colabNotebookUrl)
        }
        PrimaryButton {
            Layout.fillWidth: true
            text: qsTr("Open notebook folder")
            iconName: "folder"
            quiet: true
            onClicked: AppController.openColabNotebooksDirectory()
        }
    }
}
