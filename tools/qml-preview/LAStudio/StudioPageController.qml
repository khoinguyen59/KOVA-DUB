import QtQuick

QtObject {
    id: root

    property string capabilityId: ""
    property bool autoLoadOnSync: false
    readonly property string selectedFamilyId: familiesModel.selectedFamilyId
    readonly property string runtimeId: familiesModel.runtimeId
    readonly property string runtimeVersion: familiesModel.runtimeVersion
    readonly property var selectedFiles: familiesModel.selectedFiles
    readonly property bool selectionCommitted: true
    readonly property bool studioReady: true
    readonly property int state: 0
    readonly property string statusText: qsTr("Preview model ready")
    readonly property string statusDetail: qsTr("Production UI preview — no inference is running")
    readonly property real cpuUsage: 0
    readonly property string estimatedRamUsage: "—"
    readonly property string estimatedVramUsage: "—"
    readonly property string loadedModelName: "Preview model"
    readonly property var loadedModelFiles: []
    readonly property var loadedModels: []
    readonly property string activeModelId: ""
    readonly property int inferenceElapsedMs: 0
    readonly property string inferenceElapsedText: "00:00"
    readonly property bool modelActive: false
    readonly property bool canProcess: true
    readonly property string activeSignature: ""
    readonly property string pendingSignature: ""
    readonly property string lastError: ""
    readonly property string studioHeaderTitle: qsTr("Model configuration")
    readonly property string modalSelectionTitle: qsTr("Preview model")
    readonly property string modalSelectionValue: qsTr("Built-in preview")
    readonly property string modalSelectionDetail: qsTr("No runtime is executed")
    readonly property string runtimeDisplayText: qsTr("Preview runtime")
    readonly property var families: familiesModel.families
    readonly property QtObject familiesModel: QtObject {
        property string capability: ""
        property string selectedFamilyId: "preview"
        property string runtimeId: "preview-runtime"
        property string runtimeVersion: "1.0"
        property var selectedFiles: ({})
        readonly property var families: [
            { familyId: "preview", name: qsTr("Preview model"), description: qsTr("UI-only preview model"), selectedFiles: ({}) }
        ]

        function setCapability(value) { capability = value || "" }
        function setSelectedFamilyId(value) { selectedFamilyId = value || "preview" }
        function setInitialSelectedFiles() {}
        function itemForFamily(value) { return value === "preview" ? families[0] : null }
        function configurationForFamily(value) {
            return { familyId: value || "preview", runtimeId: runtimeId, runtimeVersion: runtimeVersion, selectedFiles: ({}) }
        }
        function recommendedConfiguration() { return configurationForFamily(selectedFamilyId) }
        function firstFamilyId() { return "preview" }
        function saveSelectionForFamily() {}
    }

    signal selectionChanged()
    signal studioContextChanged(string familyId, string runtimeId, string runtimeVersion)
    signal configurationDialogClosed()
    signal configurationGalleryRequestReset()

    function openConfiguration() {}
    function selectFamily(value) { familiesModel.setSelectedFamilyId(value); selectionChanged() }
    function selectRuntime(value, version) { familiesModel.runtimeId = value || "preview-runtime"; familiesModel.runtimeVersion = version || "1.0"; selectionChanged() }
    function commitSelection() { selectionChanged() }
    function loadSelectedConfiguration() {}
    function unload() {}
    function reload() {}
    function activateLoadedModel() {}
    function unloadLoadedModel() {}
    function syncSelectionFromSettings() {}
    function commitConfigurationSelection() { commitSelection() }
    function saveConfigurationSelection() { commitSelection() }
    function autoLoadIfReady() {}
    function reloadRequested() { reload() }
    function ejectRequested() { unload() }
    function modelSwitchRequested(nextFamilyId) { selectFamily(nextFamilyId) }
    function runtimeSwitchRequested(nextRuntimeId) { selectRuntime(nextRuntimeId, "1.0") }
}
