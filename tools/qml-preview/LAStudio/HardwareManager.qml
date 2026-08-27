pragma Singleton
import QtQuick

QtObject {
    readonly property string cpuName: "Preview CPU"
    readonly property string cpuArchitecture: "x64"
    readonly property string cpuFlags: ""
    readonly property real cpuUsage: 0
    readonly property real ramTotal: 16
    readonly property real ramUsed: 0
    readonly property var gpus: []
    readonly property real vramTotal: 0
    readonly property real vramUsed: 0
}
