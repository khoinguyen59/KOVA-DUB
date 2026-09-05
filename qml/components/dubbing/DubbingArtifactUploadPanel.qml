import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import "../base"
import "../shared"
import LAStudio

// Manual handoff for a completed workflow task.  This is deliberately
// separate from Import/Download: a saved local output is accepted only by
// the selected task's controller allow-list and is copied into the project
// cache. Colab is optional for this path.
Rectangle {
    id: root

    required property var dubbing
    required property string nodeId
    // The dialog passes the exact contract object from its Repeater model.
    // Re-querying by a presentation id caused the panel to disappear for
    // combined STT/OCR and isolation contracts even though the summary was
    // populated. The controller remains authoritative for validation.
    property var contractSpec: ({})
    readonly property var artifactSpec: (root.contractSpec
                                          && root.contractSpec.nodeId !== undefined
                                          && root.contractSpec.nodeId !== "")
                                         ? root.contractSpec
                                         : (dubbing ? dubbing.workflowArtifactSpec(nodeId) : ({}))
    signal artifactAccepted()
    property string uploadStatus: ""
    property string selectedOutputPath: ""
    property string selectedVocalsPath: ""
    property string selectedBackgroundPath: ""
    readonly property bool isIsolation: (artifactSpec.nodeId || "") === "source-separate"
    readonly property bool importInProgress: root.dubbing
                                          && root.dubbing.workflowArtifactImportProcessing
                                          && root.dubbing.workflowArtifactImportNodeId === root.nodeId
    // `processing` and `currentStepId` make this binding refresh whenever the
    // active worker changes.  The controller remains the authority on whether
    // this task, and only this task, may replace the running worker output.
    readonly property var handoffState: {
        var processingState = root.dubbing ? root.dubbing.processing : false
        var activeStep = root.dubbing ? root.dubbing.currentStepId : ""
        return root.dubbing ? root.dubbing.workflowArtifactHandoffStatus(root.nodeId) : ({})
    }
    readonly property bool mayChooseOutput: {
        // Keep the aggregate processing dependency so this binding refreshes
        // when the sibling STT/OCR worker starts or stops. Colab is not a
        // prerequisite for selecting an already-saved local artifact.
        var aggregateBusy = root.dubbing ? root.dubbing.processing : false
        if (!root.dubbing) return false
        return !root.importInProgress && (!aggregateBusy
            || root.dubbing.canImportWorkflowArtifactNow(root.nodeId))
    }

    // The parent Repeater is created only from a validated non-empty artifact
    // contract.  Do not hide the panel based on a transient QVariant/JS map
    // conversion while the delegate bindings settle; doing so made the
    // dialog show its summary but removed every file-picker button.
    visible: true
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusSmall
    color: Qt.rgba(0.20, 0.55, 0.95, 0.07)
    border.color: Qt.rgba(0.20, 0.55, 0.95, 0.34)
    border.width: 1

    function allowedFilePatterns() {
        var extensions = root.artifactSpec.allowedExtensions || []
        var patterns = []
        for (var index = 0; index < extensions.length; ++index)
            patterns.push("*" + extensions[index])
        return patterns
    }

    function isolationStemName(stem) {
        return stem === "vocals" ? qsTr("vocals audio") : qsTr("background audio")
    }

    function acceptOutput(paths) {
        if (root.dubbing.startWorkflowArtifactImport(root.nodeId, paths)) {
            root.uploadStatus = qsTr("Validating and copying the selected file in the background…")
        } else {
            root.uploadStatus = root.dubbing.lastError
        }
    }

    Connections {
        target: root.dubbing
        function onWorkflowArtifactImportFinished(importNodeId, success) {
            if (importNodeId !== root.nodeId)
                return
            if (success) {
                root.uploadStatus = qsTr("Accepted. The next task is ready.")
                root.selectedOutputPath = ""
                root.selectedVocalsPath = ""
                root.selectedBackgroundPath = ""
                root.artifactAccepted()
            } else {
                root.uploadStatus = root.dubbing.lastError
            }
        }
    }

    function openOutputDialog(loader, purpose) {
        if (!root.mayChooseOutput) {
            root.uploadStatus = qsTr("This task is busy. Only its active worker output can be replaced.")
            return
        }
        root.uploadStatus = qsTr("Select the saved %1 file. Colab is optional.").arg(purpose)
        if (loader.active && loader.item)
            loader.item.open()
        else
            loader.active = true
    }

    function selectOutput(fileUrl) {
        var path = AppController.files.urlToLocalPath(fileUrl.toString())
        if (path === "") {
            root.uploadStatus = qsTr("No output file was selected.")
            return
        }
        root.selectedOutputPath = path
        root.uploadStatus = qsTr("Output selected. Press Use uploaded output and continue to validate and import it.")
    }

    function selectVocals(fileUrl) {
        var path = AppController.files.urlToLocalPath(fileUrl.toString())
        if (path === "") {
            root.uploadStatus = qsTr("No vocals file was selected.")
            return
        }
        root.selectedVocalsPath = path
        root.uploadStatus = qsTr("Vocals audio selected. Select Background audio next.")
    }

    function selectBackground(fileUrl) {
        var path = AppController.files.urlToLocalPath(fileUrl.toString())
        if (path === "") {
            root.uploadStatus = qsTr("No background file was selected.")
            return
        }
        root.selectedBackgroundPath = path
        root.uploadStatus = qsTr("Background selected. Press Use uploaded stems and continue to validate and import them.")
    }

    Component {
        id: artifactDialogComponent
        FileDialog {
            title: qsTr("Choose the saved output for %1").arg(root.artifactSpec.title || root.nodeId)
            fileMode: FileDialog.OpenFile
            // FileDialog expects shell patterns ("*.srt"); the controller
            // remains the authority for filename and extension validation.
            nameFilters: [qsTr("Allowed output (%1)").arg(root.allowedFilePatterns().join(" "))]
            Component.onCompleted: open()
            onAccepted: {
                root.selectOutput(selectedFile)
                artifactDialogLoader.active = false
            }
            onRejected: {
                artifactDialogLoader.active = false
                if (root.selectedOutputPath === "")
                    root.uploadStatus = qsTr("No output file was selected.")
            }
        }
    }
    Loader {
        id: artifactDialogLoader
        active: false
        sourceComponent: artifactDialogComponent
    }

    Component {
        id: vocalsDialogComponent
        FileDialog {
            title: qsTr("Choose the audio file for Vocals")
            fileMode: FileDialog.OpenFile
            nameFilters: [qsTr("Audio files (%1)").arg(root.allowedFilePatterns().join(" "))]
            Component.onCompleted: open()
            onAccepted: {
                root.selectVocals(selectedFile)
                vocalsDialogLoader.active = false
            }
            onRejected: {
                vocalsDialogLoader.active = false
                if (root.selectedVocalsPath === "")
                    root.uploadStatus = qsTr("No vocals file was selected.")
            }
        }
    }
    Loader {
        id: vocalsDialogLoader
        active: false
        sourceComponent: vocalsDialogComponent
    }

    Component {
        id: backgroundDialogComponent
        FileDialog {
            title: qsTr("Choose the audio file for Background")
            fileMode: FileDialog.OpenFile
            nameFilters: [qsTr("Audio files (%1)").arg(root.allowedFilePatterns().join(" "))]
            Component.onCompleted: open()
            onAccepted: {
                root.selectBackground(selectedFile)
                backgroundDialogLoader.active = false
            }
            onRejected: {
                backgroundDialogLoader.active = false
                if (root.selectedBackgroundPath === "")
                    root.uploadStatus = qsTr("No background file was selected.")
            }
        }
    }
    Loader {
        id: backgroundDialogLoader
        active: false
        sourceComponent: backgroundDialogComponent
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Upload completed %1 output").arg(root.artifactSpec.title || root.nodeId)
                color: Theme.textPrimary
                font.bold: true
            }
            PrimaryButton {
                objectName: "dubbingArtifactUploadButton"
                visible: !root.isIsolation
                text: qsTr("Choose output")
                iconName: "folder"
                enabled: root.mayChooseOutput
                onClicked: root.openOutputDialog(artifactDialogLoader, root.artifactSpec.title || root.nodeId)
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.artifactSpec.description || ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
        Rectangle {
            visible: root.handoffState.active === true
            Layout.fillWidth: true
            implicitHeight: colabHandoffStatus.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(0.95, 0.60, 0.10, 0.10)
            border.color: Qt.rgba(0.95, 0.60, 0.10, 0.36)
            border.width: 1
            ColumnLayout {
                id: colabHandoffStatus
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Colab worker is active: %1").arg(root.handoffState.status || qsTr("working"))
                    color: Theme.warning
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: root.handoffState.progressAvailable === true
                    text: qsTr("Measured artifact transfer: %1%").arg(root.handoffState.progress)
                    color: Theme.textSecondary
                    font.pixelSize: 10
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("You may import the completed local file(s) below. Once accepted, LA Studio cancels this task's automatic transfer and continues with the normal next task.")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Colab save folder: %1").arg(root.artifactSpec.colabFolder || "")
            color: Theme.textPrimary
            font.pixelSize: 10
            wrapMode: Text.WrapAnywhere
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Worker path: %1").arg(root.artifactSpec.workerPath || "")
            color: Theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
        ColumnLayout {
            visible: root.isIsolation
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            Text {
                Layout.fillWidth: true
                text: qsTr("Select two audio files: first Vocals for STT, then Background for the final mix. File names may be anything.")
                color: Theme.textPrimary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                PrimaryButton {
                    text: qsTr("Choose Vocals audio")
                    iconName: "folder"
                    Layout.fillWidth: true
                    enabled: root.mayChooseOutput
                    onClicked: root.openOutputDialog(vocalsDialogLoader, qsTr("Vocals audio"))
                }
                Text {
                    Layout.fillWidth: true
                    text: root.selectedVocalsPath === "" ? qsTr("Not selected") : root.selectedVocalsPath
                    color: root.selectedVocalsPath === "" ? Theme.textSecondary : Theme.success
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }
            RowLayout {
                Layout.fillWidth: true
                PrimaryButton {
                    text: qsTr("Choose Background audio")
                    iconName: "folder"
                    Layout.fillWidth: true
                    enabled: root.mayChooseOutput
                    onClicked: root.openOutputDialog(backgroundDialogLoader, qsTr("Background audio"))
                }
                Text {
                    Layout.fillWidth: true
                    text: root.selectedBackgroundPath === "" ? qsTr("Not selected") : root.selectedBackgroundPath
                    color: root.selectedBackgroundPath === "" ? Theme.textSecondary : Theme.success
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }
            PrimaryButton {
                objectName: "dubbingArtifactUploadIsolationContinueButton"
                Layout.fillWidth: true
                text: qsTr("Use uploaded stems and continue")
                iconName: "play"
                enabled: root.mayChooseOutput && root.selectedVocalsPath !== ""
                         && root.selectedBackgroundPath !== ""
                onClicked: root.acceptOutput([root.selectedVocalsPath, root.selectedBackgroundPath])
            }
        }
        ColumnLayout {
            visible: !root.isIsolation
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: root.selectedOutputPath === "" ? qsTr("No output selected")
                                                         : root.selectedOutputPath
                    color: root.selectedOutputPath === "" ? Theme.textSecondary : Theme.success
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
                PrimaryButton {
                    text: qsTr("Use uploaded output and continue")
                    iconName: "play"
                    enabled: root.mayChooseOutput && root.selectedOutputPath !== ""
                    onClicked: root.acceptOutput([root.selectedOutputPath])
                }
            }
            Text {
                Layout.fillWidth: true
            text: qsTr("Selecting a file does not start or stop a worker. If an automatic transfer is active, it stops only after you confirm this exact output.")
                color: Theme.textSecondary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.isIsolation
                  ? qsTr("Required roles: Vocals audio, Background audio\nAllowed audio formats: %1")
                        .arg(root.allowedFilePatterns().join(", "))
                  : qsTr("Required file name: %1\nAllowed format: %2")
                        .arg((root.artifactSpec.expectedFiles || []).join(", "))
                        .arg(root.allowedFilePatterns().join(", "))
            color: Theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: root.uploadStatus !== "" && root.uploadStatus !== root.dubbing.lastError
            text: root.uploadStatus
            color: root.uploadStatus.indexOf("Accepted") === 0 ? Theme.success : Theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
        ErrorGuidanceInline {
            visible: root.uploadStatus !== "" && root.uploadStatus === root.dubbing.lastError
            message: root.uploadStatus
            source: "Dubbing artifact handoff"
        }
    }
}
