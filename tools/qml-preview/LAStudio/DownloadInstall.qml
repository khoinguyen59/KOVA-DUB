import QtQuick

QtObject {
    signal installStatesChanged()
    function enqueueRecommendedSetup() { return true }
    function enqueueModelFile() { return true }
    function enqueueRuntime() { return true }
}
