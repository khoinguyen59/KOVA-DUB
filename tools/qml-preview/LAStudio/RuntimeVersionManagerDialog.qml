import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    property string engineId: ""
    property string engineName: ""
    property string engineFamily: ""
    property string assetName: ""
    property string sourceUrl: ""
    property string defaultVersion: ""
    property var availableVersions: []
    property bool engineType: false
    property color accentColor: "#7c4dff"
    signal versionSelected(string runtimeId, string version)
    standardButtons: Dialog.Cancel

    contentItem: Column {
        spacing: 10
        padding: 20
        Label { text: root.engineName || qsTr("Runtime versions") }
        Label { text: qsTr("Preview only — runtime installation is disabled") }
        Button {
            text: qsTr("Use preview version")
            onClicked: { root.versionSelected(root.engineId, root.defaultVersion || "preview"); root.close() }
        }
    }
}
