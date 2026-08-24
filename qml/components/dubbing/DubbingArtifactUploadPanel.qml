import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import "../base"
import LAStudio

// Manual handoff for a completed Direct Colab task.  This is deliberately
// separate from Import/Download: a worker output is accepted only by the
// selected task's controller allow-list and is copied into the project cache.
Rectangle {
    id: root

    required property var dubbing
    required property string nodeId
    // Do not assign this property after accepting an artifact: that would
    // remove its binding and leave the panel showing a stale task contract.
    readonly property var artifactSpec: dubbing ? dubbing.workflowArtifactSpec(nodeId) : ({})
    property string uploadStatus: ""
    property string selectedOutputPath: ""
    property string selectedVocalsPath: ""
    property string selectedBackgroundPath: ""
    readonly property bool isIsolation: (artifactSpec.nodeId || "") === "source-separate"
    // `processing` and `currentStepId` make this binding refresh whenever the
    // active worker changes.  The controller remains the authority on whether
    // this task, and only this task, may replace the running worker output.
    readonly property var handoffState: {
        var processingState = root.dubbing ? root.dubbing.processing : false
        var activeStep = root.dubbing ? root.dubbing.currentStepId : ""
        return root.dubbing ? root.dubbing.workflowArtifactHandoffStatus(root.nodeId) : ({})
    }
    readonly property bool mayChooseOutput: !dubbing.processing || handoffState.canOverride === true

    visible: artifactSpec && artifactSpec.nodeId !== undefined && artifactSpec.nodeId !== ""
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
        var expected = root.artifactSpec.expectedFiles || []
        for (var index = 0; index < expected.length; ++index) {
            var name = String(expected[index])
            if (name.toLowerCase().indexOf(stem + ".") === 0)
                return name
        }
        var extensions = root.artifactSpec.allowedExtensions || []
        var extension = extensions.length > 0 ? String(extensions[0]) : ".flac"
        return stem + extension
    }

    function acceptOutput(paths) {
        if (root.dubbing.importWorkflowArtifactFiles(root.nodeId, paths)) {
            root.uploadStatus = qsTr("Accepted. The automatic transfer for this task was stopped and the next task is ready.")
            root.selectedOutputPath = ""
            root.selectedVocalsPath = ""
            root.selectedBackgroundPath = ""
        } else {
            root.uploadStatus = root.dubbing.lastError
        }
    }

    function openOutputDialog(loader, purpose) {
        if (!root.mayChooseOutput) {
            root.uploadStatus = qsTr("This task is busy. Only its active worker output can be replaced.")
            return
        }
        root.uploadStatus = qsTr("Select %1 saved from the completed Colab job.").arg(purpose)
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
        root.uploadStatus = qsTr("Vocals selected. Select background.wav or background.flac next.")
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
            title: qsTr("Choose the Colab output for %1").arg(root.artifactSpec.title || root.nodeId)
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
            title: qsTr("Choose %1 from the completed Colab job").arg(root.isolationStemName("vocals"))
            fileMode: FileDialog.OpenFile
            nameFilters: [qsTr("Vocals output (%1)").arg(root.isolationStemName("vocals"))]
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
            title: qsTr("Choose %1 from the completed Colab job").arg(root.isolationStemName("background"))
            fileMode: FileDialog.OpenFile
            nameFilters: [qsTr("Background output (%1)").arg(root.isolationStemName("background"))]
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
                    text: qsTr("You may import the exact completed file(s) below. Once accepted, LA Studio cancels this task's automatic Cloudflare transfer and continues with the normal next task.")
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
                text: qsTr("Select both stems separately. LA Studio will not accept source.wav or an arbitrary audio file.")
                color: Theme.textPrimary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                PrimaryButton {
                    text: qsTr("Choose %1").arg(root.isolationStemName("vocals"))
                    iconName: "folder"
                    Layout.fillWidth: true
                    enabled: root.mayChooseOutput
                    onClicked: root.openOutputDialog(vocalsDialogLoader, root.isolationStemName("vocals"))
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
                    text: qsTr("Choose %1").arg(root.isolationStemName("background"))
                    iconName: "folder"
                    Layout.fillWidth: true
                    enabled: root.mayChooseOutput
                    onClicked: root.openOutputDialog(backgroundDialogLoader, root.isolationStemName("background"))
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
                text: qsTr("Selecting a file does not interrupt Colab. The automatic Cloudflare transfer stops only after you confirm this exact output.")
                color: Theme.textSecondary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Required name: %1\nAllowed format: %2")
                  .arg((root.artifactSpec.expectedFiles || []).join(", "))
                  .arg(root.allowedFilePatterns().join(", "))
            color: Theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: root.uploadStatus !== ""
            text: root.uploadStatus
            color: root.uploadStatus.indexOf("Accepted") === 0 ? Theme.success : Theme.error
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
