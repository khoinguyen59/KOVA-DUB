import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../base"
import "../shared"
import "../shared/settings"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property int selectedSegment: -1
    readonly property string sourceMediaPath: root.dubbing.sourceMediaPath || ""
    readonly property bool hasLoadedSource: root.sourceMediaPath.length > 0
    readonly property bool isVideoSource: root.hasLoadedSource && /\.(mp4|mkv|mov|webm|avi)$/i.test(root.sourceMediaPath)
    readonly property bool hasDubbedPreview: root.dubbing.dubbedVocalPath.length > 0
    property string previewMode: "source"
    readonly property bool showingDubbedMedia: root.previewMode === "dubbed" && root.hasDubbedPreview
    readonly property int playbackPosition: mediaPlayer.position
    readonly property int playbackDuration: mediaPlayer.duration
    readonly property bool isPlaying: mediaPlayer.playbackState === MediaPlayer.PlayingState
    property bool previewMuted: false
    property real vocalLevel: 1.0
    property real backgroundLevel: 1.0
    property real pendingPosition: -1
    property bool pendingPlayback: false
    property int sourceSwitchAttempts: 0
    // Keep the workspace fallback aligned with SubtitleOcrPipeline's lower
    // region preset. This leaves room for playback controls while keeping
    // the default caption inside the OCR scan area.
    property var draftOcrRoi: root.dubbing.dubbingOcrRoi || ({ x: 0.10, y: 0.72, width: 0.80, height: 0.22 })
    property bool ocrRoiDragging: false
    property bool ocrRoiEditMode: false
    // The Dubbing workspace can give the video canvas its own focused view
    // without changing the active project, workflow, or media source.
    property bool previewFocusMode: false
    // OCR handles belong to the Transcribe/OCR task, never to the global
    // preview.  DubbingPage supplies the task context explicitly.
    property bool showOcrTools: false
    // Link/download settings are useful before a source is chosen, but must not
    // consume the video canvas once an editor is working on an OCR scan area.
    property bool sourceSetupExpanded: true
    // A loaded source gets a compact, scrollable source setup area. This
    // leaves the canvas usable when an operator deliberately re-opens it.
    readonly property int sourceSetupMaximumHeight: root.hasLoadedSource ? 110 : 540
    // This is a display frame only. It never crops or stretches the source:
    // VideoOutput continues to preserve the source pixels inside the frame.
    readonly property string previewFrameMode: "16:9"
    readonly property real previewFrameAspectRatio: 16 / 9
    // Some Windows multimedia backends briefly switch back to Loading while
    // paused or seeking. Once a frame was available, the poster must never
    // cover the paused frame again.
    property bool mediaFrameWasAvailable: false
    readonly property bool thumbnailReady: root.hasLoadedSource && root.mediaFrameWasAvailable
    readonly property bool sourceThumbnailAvailable: root.dubbing.sourceThumbnailUrl
                                                      && root.dubbing.sourceThumbnailUrl.toString().length > 0
    readonly property var subtitleConfiguration: root.dubbing.subtitleConfiguration || ({})
    readonly property var subtitleStyle: root.subtitleConfiguration.style || ({})
    // Target text is the normal final caption, but a reviewed STT/OCR source
    // transcript must still be visible before Translate has produced targets.
    // This deliberately mirrors the explicit text-source choice in the
    // advanced subtitle panel instead of making the preview silently diverge.
    readonly property string subtitleTextSource: root.subtitleConfiguration.textSource || "target"
    readonly property int activeSubtitleIndex: root.selectedSegment
    readonly property string activeSubtitleText: {
        if (root.activeSubtitleIndex < 0) return ""
        var segment = root.dubbing.segments[root.activeSubtitleIndex]
        var preferredText = root.subtitleTextSource === "source"
                ? segment.sourceText : segment.targetText
        return (preferredText || segment.sourceText || segment.targetText || "").trim()
    }

    signal browseRequested()
    signal manualMediaFilesRequested()
    signal segmentSelected(int index)
    signal subtitleEditorRequested()
    // A caption is a direct editing affordance, like a timeline caption in a
    // video editor.  This is intentionally separate from the advanced
    // style/import dialog so normal review never opens a full-screen form.
    signal subtitleSegmentEditRequested(int index)
    signal previewFocusRequested(bool focused)

    Layout.fillWidth: true
    Layout.fillHeight: true
    // Keep a real canvas available for a 16:9 source and OCR ROI handles.  The
    // source/download controls below are scrollable after a media file exists.
    // Keep enough height for a useful OCR canvas at 1280×800, but let the
    // full-width timeline retain its own guaranteed editor space below.
    Layout.minimumHeight: root.isVideoSource ? 440 : 300
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    function formatTime(ms) {
        if (isNaN(ms) || ms < 0) return "00:00"
        var totalSec = Math.floor(ms / 1000)
        var hr = Math.floor(totalSec / 3600)
        var min = Math.floor((totalSec - hr * 3600) / 60)
        var sec = totalSec - hr * 3600 - min * 60
        var minStr = min < 10 ? "0" + min : min.toString()
        var secStr = sec < 10 ? "0" + sec : sec.toString()
        return hr > 0 ? (hr < 10 ? "0" + hr : hr.toString()) + ":" + minStr + ":" + secStr : minStr + ":" + secStr
    }

    function formatBytes(bytes) {
        if (bytes < 0) return ""
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KiB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MiB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GiB"
    }

    function localMediaUrl(path) {
        if (!path) return ""
        var normalized = path.replace(/\\/g, "/")
        var encoded = encodeURI(normalized).replace(/#/g, "%23")
        return Qt.platform.os === "windows" ? "file:///" + encoded : "file://" + encoded
    }

    function pause() { pauseAll() }
    function pauseAll() {
        mediaPlayer.pause()
        vocalPlayer.pause()
        backgroundPlayer.pause()
        // A temporary drift correction must never leak into the next play
        // session or make a paused preview resume at an altered speed.
        vocalPlayer.playbackRate = 1.0
        backgroundPlayer.playbackRate = 1.0
    }
    function seekAll(position) {
        mediaPlayer.position = position
        root.updateActiveSubtitle(position)
        if (root.showingDubbedMedia) {
            if (vocalPlayer.seekable) vocalPlayer.position = position
            if (backgroundPlayer.seekable) backgroundPlayer.position = position
        }
    }
    function playAll() {
        if (root.showingDubbedMedia) {
            if (root.dubbing.dubbedVocalPath.length > 0) {
                vocalPlayer.position = mediaPlayer.position
                vocalPlayer.playbackRate = 1.0
                vocalPlayer.play()
            }
            if (root.dubbing.backgroundPath.length > 0) {
                backgroundPlayer.position = mediaPlayer.position
                backgroundPlayer.playbackRate = 1.0
                backgroundPlayer.play()
            }
        }
        mediaPlayer.play()
    }
    function switchPreviewMode(mode) {
        if (mode === root.previewMode || (mode === "dubbed" && !root.hasDubbedPreview)) return
        root.pendingPosition = mediaPlayer.position
        root.pendingPlayback = mediaPlayer.playbackState === MediaPlayer.PlayingState
        pauseAll()
        root.previewMode = mode
        root.sourceSwitchAttempts = 0
        sourceSwitchFallback.restart()
    }
    function restoreAfterSourceSwitch() {
        if (root.pendingPosition < 0) return
        var restorePosition = root.pendingPosition
        var restorePlayback = root.pendingPlayback
        root.pendingPosition = -1
        root.pendingPlayback = false
        seekAll(restorePosition)
        if (restorePlayback) playAll()
    }
    function seekToSegment(index) {
        if (mediaPlayer.seekable && index >= 0 && index < root.dubbing.segments.length)
            seekAll(root.dubbing.segments[index].startMs)
    }

    function clampRoi(value, low, high) { return Math.max(low, Math.min(high, value)) }
    function commitDubbingOcrRoi() {
        if (!root.dubbing.setDubbingOcrRoi(draftOcrRoi))
            draftOcrRoi = root.dubbing.dubbingOcrRoi
        ocrRoiDragging = false
    }
    function resizeDubbingOcrRoi(mode, point, geometry) {
        var r = draftOcrRoi
        var left = geometry.x + r.x * geometry.width
        var right = left + r.width * geometry.width
        var top = geometry.y + r.y * geometry.height
        var bottom = top + r.height * geometry.height
        var x = clampRoi(point.x, geometry.x, geometry.x + geometry.width)
        var y = clampRoi(point.y, geometry.y, geometry.y + geometry.height)
        var minimum = 18
        if (mode.indexOf("l") !== -1) left = Math.min(x, right - minimum)
        if (mode.indexOf("r") !== -1) right = Math.max(x, left + minimum)
        if (mode.indexOf("t") !== -1) top = Math.min(y, bottom - minimum)
        if (mode.indexOf("b") !== -1) bottom = Math.max(y, top + minimum)
        draftOcrRoi = { x: (left - geometry.x) / geometry.width,
                        y: (top - geometry.y) / geometry.height,
                        width: (right - left) / geometry.width,
                        height: (bottom - top) / geometry.height }
    }
    function moveDubbingOcrRoi(point, grabX, grabY, geometry) {
        var width = draftOcrRoi.width * geometry.width
        var height = draftOcrRoi.height * geometry.height
        var left = clampRoi(point.x - grabX, geometry.x, geometry.x + geometry.width - width)
        var top = clampRoi(point.y - grabY, geometry.y, geometry.y + geometry.height - height)
        draftOcrRoi = { x: (left - geometry.x) / geometry.width,
                        y: (top - geometry.y) / geometry.height,
                        width: draftOcrRoi.width, height: draftOcrRoi.height }
    }

    function updateActiveSubtitle(position) {
        var cues = root.dubbing.segments || []
        if (root.selectedSegment >= 0 && root.selectedSegment < cues.length) {
            var current = cues[root.selectedSegment]
            if (position >= current.startMs && position <= current.endMs)
                return
        }
        // Cues are sorted by time. This keeps frame-position work O(log n)
        // rather than walking every subtitle cue on every multimedia update.
        var left = 0
        var right = cues.length - 1
        var match = -1
        while (left <= right) {
            var middle = Math.floor((left + right) / 2)
            var cue = cues[middle]
            if (position < cue.startMs)
                right = middle - 1
            else if (position > cue.endMs)
                left = middle + 1
            else {
                match = middle
                break
            }
        }
        if (match >= 0 && root.selectedSegment !== match) {
            root.selectedSegment = match
            root.segmentSelected(match)
        }
    }

    function qmlSmokeMediaControlsCheck() {
        return controlsAutoHide.qmlSmokeStateCheck()
                && controlsAutoHide.delayMs === 2000
                && previewToolbar.width > 0
                && previewToolbar.height === 40
                && previewModeSelector.y >= -1
                && previewModeSelector.y + previewModeSelector.height
                   <= previewToolbar.height + 1
                && previewControls.z > dubbingOcrRoiOverlay.z
                && subtitlePreviewOverlay.width > 0
                && previewFrame.width > 0
                && previewFrame.height > 0
                && (!subtitlePreviewOverlay.visible
                    || subtitlePreviewOverlay.y + subtitlePreviewOverlay.height
                       <= previewFrame.y + previewFrame.height
                          - subtitlePreviewOverlay.lowerControlsClearance + 1)
                && (!root.hasLoadedSource || !sourceSetupPanel.visible)
    }

    // The offscreen route test accepts a production file-picker fixture.  It
    // validates the post-selection layout contract rather than only checking
    // that source setup is present in the source text.
    function qmlSmokeLoadedSourceLayoutCheck() {
        if (!root.hasLoadedSource || sourceSetupPanel.visible)
            return false
        var landscapeRatio = previewFrame.height > 0
                ? previewFrame.width / previewFrame.height : 0
        return Math.abs(landscapeRatio - 16 / 9) < 0.01
    }

    function qmlSmokeWorkspaceContractCheck() {
        if (previewFrame.width <= 0 || previewFrame.height <= 0
                || Math.abs(previewFrame.width / previewFrame.height - 16 / 9) > 0.01)
            return false
        if (previewFrameMode !== "16:9")
            return false
        if (sourceSetupToggle.visible || openVideoButton.visible || previewFocusToggle.visible)
            return false
        if (previewFrameModeSelector.visible && previewFrameModeSelector.model.length !== 1)
            return false
        return previewModeSelector.visible
    }

    // Selection can originate from the native file dialog, a downloaded-media
    // row, or an automated preflight Fix action. Keep the post-selection
    // visual state deterministic instead of relying only on a later QML
    // property-notify turn to collapse the source setup area.
    function collapseSourceSetupAfterSelection() {
        root.sourceSetupExpanded = false
    }

    MediaControlsAutoHide {
        id: controlsAutoHide
        playing: mediaPlayer.playbackState === MediaPlayer.PlayingState
        controlsFocused: previewPlayButton.activeFocus || previewMuteButton.activeFocus
    }

    Connections {
        target: root.dubbing
        function onProjectChanged() {
            if (!root.ocrRoiDragging)
                root.draftOcrRoi = root.dubbing.dubbingOcrRoi
        }
    }

    onSourceMediaPathChanged: {
        // A newly selected/downloaded source should immediately receive the
        // canvas. Re-opening source setup remains an explicit user action.
        root.sourceSetupExpanded = !root.hasLoadedSource
        root.mediaFrameWasAvailable = false
        if (root.dubbing.requestSourceThumbnail)
            root.dubbing.requestSourceThumbnail()
    }
    onPreviewFocusModeChanged: {
        // A focused canvas must never be squeezed by optional download controls.
        if (root.previewFocusMode)
            root.sourceSetupExpanded = false
    }

    MediaPlayer {
        id: mediaPlayer
        source: root.showingDubbedMedia ? root.dubbing.playbackMediaUrl : root.dubbing.sourceMediaUrl
        audioOutput: AudioOutput {
            id: sourceAudioOutput
            volume: root.showingDubbedMedia || root.previewMuted ? 0 : 1
        }
        videoOutput: videoOutput
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                root.mediaFrameWasAvailable = true
                root.restoreAfterSourceSwitch()
            }
        }
    }
    MediaPlayer {
        id: vocalPlayer
        source: root.showingDubbedMedia
                ? root.localMediaUrl(root.dubbing.dubbedVocalPath) : ""
        audioOutput: AudioOutput {
            volume: root.showingDubbedMedia && !root.previewMuted ? root.vocalLevel : 0
        }
    }
    MediaPlayer {
        id: backgroundPlayer
        source: root.showingDubbedMedia
                ? root.localMediaUrl(root.dubbing.backgroundPath) : ""
        audioOutput: AudioOutput {
            // The rendered mix uses 35% background gain. A 100% slider value
            // therefore reproduces the rendered/exported balance.
            volume: root.showingDubbedMedia && !root.previewMuted
                    ? root.backgroundLevel * 0.35 : 0
        }
    }

    Connections {
        target: mediaPlayer
        function onSubtitleTracksChanged() {
            if (mediaPlayer.subtitleTracks.length > 0)
                mediaPlayer.activeSubtitleTrack = 0
        }
        function onPositionChanged() {
            if (mediaPlayer.playbackState !== MediaPlayer.PlayingState) return
            root.updateActiveSubtitle(mediaPlayer.position)
        }
    }
    onHasDubbedPreviewChanged: {
        if (root.hasDubbedPreview)
            root.switchPreviewMode("dubbed")
        else
            root.switchPreviewMode("source")
    }
    Component.onCompleted: {
        root.previewMode = root.hasDubbedPreview ? "dubbed" : "source"
        root.sourceSetupExpanded = !root.hasLoadedSource
        if (root.dubbing.requestSourceThumbnail)
            root.dubbing.requestSourceThumbnail()
    }
    Timer {
        id: sourceSwitchFallback
        interval: 120
        onTriggered: {
            if (mediaPlayer.duration > 0
                || mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia
                || mediaPlayer.mediaStatus === MediaPlayer.BufferedMedia
                || root.sourceSwitchAttempts >= 20) {
                root.restoreAfterSourceSwitch()
                return
            }
            root.sourceSwitchAttempts += 1
            restart()
        }
    }
    Timer {
        // A 1-second low-frequency correction avoids repeated hard seeks
        // while the video and local stem decoders settle independently.
        interval: 1000
        repeat: true
        running: root.showingDubbedMedia
                 && mediaPlayer.playbackState === MediaPlayer.PlayingState
        onTriggered: {
            if (vocalPlayer.seekable) {
                var vocalDrift = vocalPlayer.position - mediaPlayer.position
                if (Math.abs(vocalPlayer.position - mediaPlayer.position) > 1500) {
                    vocalPlayer.position = mediaPlayer.position
                    vocalPlayer.playbackRate = 1.0
                } else if (Math.abs(vocalPlayer.position - mediaPlayer.position) > 500) {
                    vocalPlayer.playbackRate = vocalDrift < 0 ? 1.02 : 0.98
                } else {
                    vocalPlayer.playbackRate = 1.0
                }
            }
            if (backgroundPlayer.seekable) {
                var backgroundDrift = backgroundPlayer.position - mediaPlayer.position
                if (Math.abs(backgroundPlayer.position - mediaPlayer.position) > 1500) {
                    backgroundPlayer.position = mediaPlayer.position
                    backgroundPlayer.playbackRate = 1.0
                } else if (Math.abs(backgroundPlayer.position - mediaPlayer.position) > 500) {
                    backgroundPlayer.playbackRate = backgroundDrift < 0 ? 1.02 : 0.98
                } else {
                    backgroundPlayer.playbackRate = 1.0
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall
        // All preview controls share one horizontal editor toolbar.  Keeping
        // Original/Dubbed on a second row made the preview feel detached and
        // permanently consumed vertical space.  The one toolbar scrolls as a
        // unit on narrow canvases rather than wrapping or painting controls
        // over the video frame.
        Flickable {
            id: previewToolbar
            objectName: "dubbingPreviewToolbar"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.minimumHeight: 40
            contentWidth: previewToolbarRow.implicitWidth
            contentHeight: height
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

                Row {
                id: previewToolbarRow
                height: previewToolbar.height
                spacing: Theme.paddingSmall
                Text {
                    width: implicitWidth
                    height: previewToolbar.height
                    text: qsTr("VIDEO PREVIEW")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    font.letterSpacing: 0.25
                    verticalAlignment: Text.AlignVCenter
                }
                AppComboBox {
                    id: previewFrameModeSelector
                    objectName: "dubbingPreviewFrameModeSelector"
                    width: 112
                    height: previewToolbar.height
                    model: [ { text: qsTr("Fit source"), value: "16:9" } ]
                    textRole: "text"
                    currentIndex: 0
                    enabled: false
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("The preview viewport stays 16:9; source pixels remain uncropped and unstretched.")
                }
                Rectangle {
                    id: previewModeSelector
                    objectName: "dubbingPreviewModeSelector"
                    width: 206
                    height: 32
                    anchors.verticalCenter: parent.verticalCenter
                    radius: Theme.radiusSmall
                    color: Qt.rgba(0, 0, 0, 0.18)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 3
                        spacing: 3
                        PreviewModeButton {
                            Layout.fillWidth: true
                            text: qsTr("Original")
                            iconName: "file"
                            selected: !root.showingDubbedMedia
                            enabled: root.dubbing.sourceMediaPath.length > 0
                            onClicked: root.switchPreviewMode("source")
                        }
                        PreviewModeButton {
                            Layout.fillWidth: true
                            text: qsTr("Dubbed")
                            iconName: "mic"
                            selected: root.showingDubbedMedia
                            enabled: root.hasDubbedPreview
                            onClicked: root.switchPreviewMode("dubbed")
                        }
                    }
                }
                /* Source management is deliberately outside the loaded-source
                   toolbar. The right task panel owns workflow actions. */
                Button {
                    id: sourceSetupToggle
                    objectName: "dubbingSourceSetupToggle"
                    height: previewToolbar.height
                    visible: false
                }
                PrimaryButton {
                    id: openVideoButton
                    objectName: "dubbingOpenVideoButton"
                    height: previewToolbar.height
                    visible: false
                }
                Button {
                    id: previewFocusToggle
                    objectName: "dubbingPreviewFocusToggle"
                    height: previewToolbar.height
                    visible: false
                }
            }

            ScrollBar.horizontal: ScrollBar {
                policy: previewToolbar.contentWidth > previewToolbar.width
                        ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.07) }

        // Show source setup by default only until a source exists. Keeping it
        // expanded after a video loads squeezed the preview to a thin strip,
        // which made OCR region editing impractical.
        ScrollView {
            id: sourceSetupPanel
            objectName: "dubbingSourceSetupScrollView"
            Layout.fillWidth: true
            visible: !root.hasLoadedSource || root.sourceSetupExpanded
            Layout.minimumHeight: 0
            Layout.maximumHeight: root.sourceSetupMaximumHeight
            Layout.preferredHeight: visible
                                  ? Math.min(sourceSetupContent.implicitHeight,
                                             root.sourceSetupMaximumHeight)
                                  : 0
            clip: true
            contentWidth: availableWidth
            contentHeight: sourceSetupContent.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: contentHeight > height
                                       ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

            ColumnLayout {
                id: sourceSetupContent
                width: sourceSetupPanel.availableWidth
                spacing: Theme.paddingSmall
                ColabMediaAcquisitionPanel {
                    Layout.fillWidth: true
                    dubbing: root.dubbing
                    compact: root.hasLoadedSource
                    onLocalFilesRequested: root.manualMediaFilesRequested()
                    onLibraryRequested: mediaQueueDialog.open()
                }

            }
        }
        Text {
            Layout.fillWidth: true
            visible: root.dubbing.mediaQueueDownloading || root.dubbing.mediaQueueProcessing
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            text: root.dubbing.mediaQueueStatus
        }

        Rectangle {
            id: previewSurface
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Preserve a practical canvas for 16:9 video and OCR handles even
            // on high-DPI displays. The surrounding panel may grow further.
            Layout.minimumHeight: root.isVideoSource
                                  ? Math.min(520, Math.max(400, width * 0.50))
                                  : 220
            Layout.preferredHeight: root.isVideoSource
                                    ? Math.min(660, Math.max(500, width * 0.58))
                                    : 260
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.30)
            border.color: Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
            clip: true

            Column {
                anchors.centerIn: parent
                width: parent.width - Theme.paddingXL * 2
                spacing: Theme.paddingSmall
                visible: root.dubbing.sourceMediaPath.length === 0
                LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "folder"; color: Theme.accentLight; width: 38; height: 38 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Add source media"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("WAV, MP3, MP4 or MKV"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            }
            Item {
                id: previewFrame
                objectName: "dubbingPreviewFrame"
                anchors.centerIn: parent
                width: root.previewFrameAspectRatio > 0
                       ? Math.min(previewSurface.width,
                                  previewSurface.height * root.previewFrameAspectRatio)
                       : previewSurface.width
                height: root.previewFrameAspectRatio > 0
                        ? Math.min(previewSurface.height,
                                   previewSurface.width / root.previewFrameAspectRatio)
                        : previewSurface.height

                Rectangle {
                    anchors.fill: parent
                    color: "#11121a"
                    radius: Theme.radiusSmall
                    visible: root.isVideoSource
                }
                VideoOutput {
                    id: videoOutput
                    anchors.fill: parent
                    visible: root.isVideoSource
                    fillMode: VideoOutput.PreserveAspectFit
                }
                Rectangle {
                    id: thumbnailPoster
                    objectName: "dubbingVideoThumbnail"
                    anchors.fill: parent
                    // The thumbnail is only the loading fallback. Once the
                    // media is loaded, VideoOutput owns the canvas so pause
                    // (and a user stop implemented as pause) keeps the exact
                    // frame where the operator stopped instead of reverting
                    // to the first-frame poster.
                    visible: root.isVideoSource && !root.thumbnailReady
                    z: 6
                    color: "#11121a"
                    Image {
                        id: sourceThumbnailImage
                        objectName: "dubbingVideoThumbnailImage"
                        anchors.fill: parent
                        source: root.dubbing.sourceThumbnailUrl
                        visible: root.sourceThumbnailAvailable && status === Image.Ready
                        asynchronous: true
                        cache: true
                        fillMode: Image.PreserveAspectFit
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: Theme.paddingSmall
                        visible: !sourceThumbnailImage.visible
                        LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "video"; color: Theme.accentLight; width: 36; height: 36 }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Loading preview thumbnail…")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }
            }
            Rectangle {
                id: dubbingOcrRoiOverlay
                objectName: "dubbingSubtitleOcrRoiOverlay"
                // The source content rect is the coordinate space used by
                // OCR metadata. Keep the complete source frame here: when
                // editing, the ROI must be draggable across the playback
                // controls instead of being artificially capped above them.
                readonly property rect sourceContent: Qt.rect(
                    previewFrame.x + videoOutput.contentRect.x,
                    previewFrame.y + videoOutput.contentRect.y,
                    videoOutput.contentRect.width,
                    videoOutput.contentRect.height)
                readonly property rect content: sourceContent
                visible: root.showOcrTools && root.isVideoSource && root.dubbing.dubbingOcrRoiVisible
                         && content.width > 0 && content.height > 0
                x: content.x + root.draftOcrRoi.x * content.width
                y: content.y + root.draftOcrRoi.y * content.height
                width: root.draftOcrRoi.width * content.width
                height: root.draftOcrRoi.height * content.height
                color: root.ocrRoiEditMode
                       ? Qt.rgba(0.45, 0.20, 1.0, 0.22)
                       : Qt.rgba(0.45, 0.20, 1.0, 0.11)
                border.color: root.ocrRoiEditMode ? Theme.accentLight : Theme.primary
                border.width: root.ocrRoiEditMode ? 3 : 2
                // Keep timeline input above ROI editing in every mode. A
                // drag that starts on the ROI retains its grab while crossing
                // the controls, but a drag begun on the seek strip scrubs.
                z: 8
                MouseArea {
                    anchors.fill: parent
                    property real grabX: 0
                    property real grabY: 0
                    enabled: root.ocrRoiEditMode && !root.dubbing.processing
                    preventStealing: true
                    cursorShape: Qt.SizeAllCursor
                    onPressed: function(mouse) { grabX = mouse.x; grabY = mouse.y; root.ocrRoiDragging = true }
                    onPositionChanged: function(mouse) {
                        if (pressed) root.moveDubbingOcrRoi(parent.mapToItem(dubbingOcrRoiOverlay.parent, mouse.x, mouse.y),
                                                             grabX, grabY, dubbingOcrRoiOverlay.content)
                    }
                    onReleased: root.commitDubbingOcrRoi()
                    onCanceled: root.commitDubbingOcrRoi()
                }
                Rectangle {
                    visible: root.ocrRoiEditMode || root.dubbing.processing
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 4
                    implicitWidth: scanAreaLabel.implicitWidth + 12
                    implicitHeight: scanAreaLabel.implicitHeight + 6
                    radius: 5
                    color: root.dubbing.processing ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.92)
                                                   : Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.92)
                    Text {
                        id: scanAreaLabel
                        anchors.centerIn: parent
                        text: root.dubbing.processing ? qsTr("OCR scan area locked while running")
                                                       : qsTr("Drag scan area · resize handles")
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
                Repeater {
                    model: [ { key: "tl", x: 0, y: 0 }, { key: "tr", x: 1, y: 0 },
                             { key: "bl", x: 0, y: 1 }, { key: "br", x: 1, y: 1 },
                             { key: "l", x: 0, y: 0.5 }, { key: "r", x: 1, y: 0.5 },
                             { key: "t", x: 0.5, y: 0 }, { key: "b", x: 0.5, y: 1 } ]
                    delegate: Rectangle {
                        objectName: "dubbingSubtitleOcrRoiHandle_" + modelData.key
                        visible: root.ocrRoiEditMode && !root.dubbing.processing
                        width: 16; height: 16; radius: 8
                        x: modelData.x * parent.width - width / 2
                        y: modelData.y * parent.height - height / 2
                        color: Theme.primary; border.color: Theme.textPrimary; border.width: 1
                        z: 10
                        MouseArea {
                            anchors.fill: parent
                            enabled: !root.dubbing.processing
                            preventStealing: true
                            cursorShape: Qt.SizeFDiagCursor
                            onPressed: root.ocrRoiDragging = true
                            onPositionChanged: function(mouse) {
                                if (pressed) root.resizeDubbingOcrRoi(modelData.key,
                                    parent.mapToItem(dubbingOcrRoiOverlay.parent, mouse.x, mouse.y),
                                    dubbingOcrRoiOverlay.content)
                            }
                            onReleased: root.commitDubbingOcrRoi()
                            onCanceled: root.commitDubbingOcrRoi()
                        }
                    }
                }
            }
            FontLoader {
                id: subtitlePreviewFont
                source: root.subtitleStyle.fontFile || ""
            }
            Rectangle {
                id: subtitlePreviewOverlay
                objectName: "dubbingSubtitlePreviewOverlay"
                readonly property string alignment: root.subtitleStyle.alignment || "bottom"
                readonly property real safeMargin: Number(root.subtitleStyle.safeMargin || 0.06)
                // Keep captions above the seek/playback controls even when
                // the OCR ROI was placed at the very bottom of the source.
                // The extra padding prevents glyph descenders from being
                // visually swallowed by the control gradient.
                readonly property real lowerControlsClearance:
                    previewControls.height + Theme.paddingMedium
                readonly property bool followsOcrRegion: root.showOcrTools
                                                        && root.dubbing.dubbingOcrRoiVisible
                                                        && root.isVideoSource
                visible: root.isVideoSource && root.activeSubtitleText.length > 0
                width: followsOcrRegion
                       ? Math.max(80, dubbingOcrRoiOverlay.content.width * 0.92)
                       : Math.max(80, previewFrame.width * Number(root.subtitleStyle.maxWidth || 0.82))
                height: subtitlePreviewText.implicitHeight + Theme.paddingSmall * 2
                x: followsOcrRegion
                   ? dubbingOcrRoiOverlay.content.x + (dubbingOcrRoiOverlay.content.width - width) / 2
                   : alignment === "custom"
                   ? previewFrame.x + previewFrame.width * Number(root.subtitleStyle.positionX || 0.5) - width / 2
                   : previewFrame.x + (previewFrame.width - width) / 2
                y: followsOcrRegion
                  ? Math.max(
                        previewFrame.y + Theme.paddingSmall,
                        Math.min(
                            dubbingOcrRoiOverlay.content.y
                                + dubbingOcrRoiOverlay.content.height - height
                                - Theme.paddingMedium,
                            previewFrame.y + previewFrame.height
                                - height - lowerControlsClearance))
                  : alignment === "top" ? previewFrame.y + previewFrame.height * safeMargin
                    : alignment === "custom" ? previewFrame.y + previewFrame.height * Number(root.subtitleStyle.positionY || 0.90) - height / 2
                                               : previewFrame.y + previewFrame.height - height
                                                   - Math.max(previewFrame.height * safeMargin,
                                                              lowerControlsClearance)
                radius: Theme.radiusSmall
                color: "transparent"
                // The caption stays above the translucent OCR editor fill;
                // resize handles remain above both layers for editing.
                z: 9
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: root.subtitleStyle.backgroundColor || "#00000000"
                    opacity: Math.max(0, Math.min(1, Number(root.subtitleStyle.backgroundOpacity || 0)))
                }
                Text {
                    id: subtitlePreviewShadow
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingSmall + Number(root.subtitleStyle.shadowOffset || 0)
                    anchors.rightMargin: Theme.paddingSmall - Number(root.subtitleStyle.shadowOffset || 0)
                    anchors.topMargin: Theme.paddingSmall + Number(root.subtitleStyle.shadowOffset || 0)
                    anchors.bottomMargin: Theme.paddingSmall - Number(root.subtitleStyle.shadowOffset || 0)
                    text: root.activeSubtitleText
                    color: root.subtitleStyle.shadowColor || "#99000000"
                    font.family: subtitlePreviewFont.status === FontLoader.Ready ? subtitlePreviewFont.name : (root.subtitleStyle.fontFamily || "Arial")
                    font.pixelSize: Number(root.subtitleStyle.fontSize || 42)
                    font.weight: Number(root.subtitleStyle.fontWeight || 600)
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap; lineHeight: Number(root.subtitleStyle.lineSpacing || 1.0); lineHeightMode: Text.ProportionalHeight
                }
                Text {
                    id: subtitlePreviewText
                    anchors.fill: parent
                    anchors.margins: Theme.paddingSmall
                    text: root.activeSubtitleText
                    color: root.subtitleStyle.textColor || "#FFFFFFFF"
                    font.family: subtitlePreviewFont.status === FontLoader.Ready ? subtitlePreviewFont.name : (root.subtitleStyle.fontFamily || "Arial")
                    font.pixelSize: Number(root.subtitleStyle.fontSize || 42)
                    font.weight: Number(root.subtitleStyle.fontWeight || 600)
                    style: Text.Outline
                    styleColor: root.subtitleStyle.outlineColor || "#D9000000"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap; lineHeight: Number(root.subtitleStyle.lineSpacing || 1.0); lineHeightMode: Text.ProportionalHeight
                }
                MouseArea {
                    id: subtitlePreviewEditArea
                    objectName: "dubbingSubtitlePreviewEditArea"
                    anchors.fill: parent
                    enabled: root.activeSubtitleIndex >= 0
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: root.subtitleSegmentEditRequested(root.activeSubtitleIndex)
                }
                ToolTip.visible: subtitlePreviewEditArea.containsMouse
                ToolTip.text: qsTr("Click this subtitle to edit its text")
            }
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0.06, 0.06, 0.09, 0.95)
                visible: root.dubbing.sourceMediaPath.length > 0 && !root.isVideoSource
                Column {
                    anchors.centerIn: parent
                    width: parent.width - Theme.paddingXL * 2
                    spacing: Theme.paddingSmall
                    LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "volume"; color: Theme.accentLight; width: 42; height: 42 }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.dubbing.sourceMediaPath.split(/[\\/]/).pop(); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; elide: Text.ElideMiddle; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Audio track playing"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                }
            }
            MouseArea { anchors.fill: parent; enabled: root.dubbing.sourceMediaPath.length === 0; cursorShape: Qt.PointingHandCursor; onClicked: root.browseRequested() }

            HoverHandler {
                id: previewHoverHandler
                enabled: root.dubbing.sourceMediaPath.length > 0
                onHoveredChanged: controlsAutoHide.pointerInsideSurface = hovered
            }
            Rectangle {
                id: previewControls
                objectName: "dubbingSharedMediaControls"
                anchors.left: previewFrame.left; anchors.right: previewFrame.right; anchors.bottom: previewFrame.bottom
                height: 44
                // OCR ROI editing is intentionally above the video but never
                // above the player controls. Without an explicit z-order, a
                // lower ROI can steal the seek drag from seekArea.
                z: 20
                visible: root.dubbing.sourceMediaPath.length > 0 && (opacity > 0 || controlsAutoHide.controlsVisible)
                opacity: controlsAutoHide.controlsVisible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: Qt.rgba(0.06, 0.06, 0.09, 0.92) }
                }

                Item {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: -8
                    height: 16
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        color: Qt.rgba(255, 255, 255, 0.2)
                        Rectangle {
                            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                            color: Theme.accentLight
                            width: mediaPlayer.duration > 0 ? mediaPlayer.position / mediaPlayer.duration * parent.width : 0
                        }
                    }
                    MouseArea {
                        id: seekArea
                        anchors.fill: parent
                        enabled: mediaPlayer.duration > 0
                        hoverEnabled: true
                        property bool wasPlaying: false
                        function updatePosition(x) { if (mediaPlayer.duration > 0) root.seekAll(Math.max(0, Math.min(1, x / width)) * mediaPlayer.duration) }
                        onPressed: {
                            controlsAutoHide.interactionActive = true
                            controlsAutoHide.noteInteraction()
                            wasPlaying = mediaPlayer.playbackState === MediaPlayer.PlayingState
                            if (wasPlaying) root.pauseAll()
                            updatePosition(mouseX)
                        }
                        onPositionChanged: if (pressed) {
                            updatePosition(mouseX)
                            controlsAutoHide.noteInteraction()
                        }
                        onReleased: {
                            if (wasPlaying) root.playAll()
                            controlsAutoHide.interactionActive = false
                            controlsAutoHide.noteInteraction()
                        }
                        ToolTip.visible: containsMouse && mediaPlayer.duration > 0
                        ToolTip.text: root.formatTime(Math.max(0, Math.min(1, mouseX / width)) * mediaPlayer.duration)
                    }
                }
                RowLayout {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.leftMargin: Theme.paddingMedium; anchors.rightMargin: Theme.paddingMedium
                    height: 40
                    spacing: Theme.paddingMedium
                    Button {
                        id: previewPlayButton
                        implicitWidth: 34; implicitHeight: 34
                        flat: true
                        background: Rectangle {
                            radius: 17
                            color: previewPlayButton.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Qt.rgba(1, 1, 1, 0.14)
                            border.color: previewPlayButton.hovered ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.25)
                            border.width: 1
                        }
                        contentItem: LineIcon {
                            anchors.centerIn: parent
                            name: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                            color: "#ffffff"
                            width: 16
                            height: 16
                        }
                        onClicked: {
                            mediaPlayer.playbackState === MediaPlayer.PlayingState ? root.pauseAll() : root.playAll()
                            controlsAutoHide.noteInteraction()
                        }
                    }
                    Button {
                        id: previewMuteButton
                        implicitWidth: 32; implicitHeight: 32
                        flat: true
                        background: Rectangle {
                            radius: 16
                            color: previewMuteButton.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                        }
                        contentItem: LineIcon { anchors.centerIn: parent; name: "volume"; color: root.previewMuted ? Theme.textSecondary : Theme.textPrimary; width: 16; height: 16 }
                        onClicked: {
                            root.previewMuted = !root.previewMuted
                            controlsAutoHide.noteInteraction()
                        }
                    }

                    Item { Layout.fillWidth: true }
                    Text { text: "%1 / %2".arg(root.formatTime(mediaPlayer.position)).arg(root.formatTime(mediaPlayer.duration)); color: Theme.textSecondary; font.pixelSize: 11; font.family: "Monospace" }
                }
            }
        }
        Flow {
            Layout.fillWidth: true
            visible: root.showOcrTools && root.dubbing.dubbingOcrRoiVisible
            spacing: Theme.paddingSmall
            Button {
                text: root.ocrRoiEditMode ? qsTr("Done editing scan area") : qsTr("Edit OCR scan area")
                enabled: !root.dubbing.processing && root.isVideoSource
                onClicked: {
                    // OCR editing always prioritizes the canvas over optional
                    // download controls, so the complete scan box is visible.
                    root.sourceSetupExpanded = false
                    root.ocrRoiEditMode = !root.ocrRoiEditMode
                }
            }
            Button { text: qsTr("Preset lower region"); enabled: !root.dubbing.processing && root.isVideoSource; onClicked: root.dubbing.presetDubbingOcrLowerRegion() }
            Button { text: qsTr("Reset region"); enabled: !root.dubbing.processing && root.isVideoSource; onClicked: root.dubbing.resetDubbingOcrRoi() }
            Button { text: qsTr("Preview crop"); enabled: !root.dubbing.processing && root.isVideoSource; onClicked: root.dubbing.previewDubbingOcrCrop(mediaPlayer.position) }
            Text { visible: root.isVideoSource && root.dubbing.processing; text: qsTr("OCR locked"); color: Theme.warning; topPadding: 7; font.pixelSize: 10 }
            Text { visible: root.isVideoSource; text: qsTr("ROI: x %1, y %2, w %3, h %4").arg(Number(root.draftOcrRoi.x).toFixed(3)).arg(Number(root.draftOcrRoi.y).toFixed(3)).arg(Number(root.draftOcrRoi.width).toFixed(3)).arg(Number(root.draftOcrRoi.height).toFixed(3)); color: Theme.textSecondary; topPadding: 7; font.pixelSize: 10 }
            Text { visible: !root.isVideoSource; text: qsTr("Choose a video to set the OCR scan region."); color: Theme.textSecondary; topPadding: 7; font.pixelSize: 10 }
        }
        Rectangle {
            Layout.fillWidth: true
            visible: root.showOcrTools && root.dubbing.dubbingOcrRoiVisible
                     && AppController.subtitleOcr.cropPreviewUrl.toString() !== ""
            Layout.preferredHeight: visible ? 180 : 0
            color: Theme.surfaceAlt
            radius: Theme.radiusSmall
            Image {
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                source: AppController.subtitleOcr.cropPreviewUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            visible: root.showingDubbedMedia
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.025)
            border.color: Qt.rgba(1, 1, 1, 0.07)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium
                LineIcon {
                    name: "volume"
                    color: Theme.accentLight
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }
                Text {
                    text: qsTr("MIX")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1
                }
                MixerControl {
                    Layout.fillWidth: true
                    label: qsTr("Vocal")
                    value: root.vocalLevel
                    onMoved: root.vocalLevel = value
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 30; color: Qt.rgba(1, 1, 1, 0.08) }
                MixerControl {
                    Layout.fillWidth: true
                    label: qsTr("Background")
                    value: root.backgroundLevel
                    enabled: root.dubbing.backgroundPath.length > 0
                    onMoved: root.backgroundLevel = value
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !root.hasLoadedSource
            FieldProxy { Layout.fillWidth: true; text: root.dubbing.sourceMediaPath; placeholderText: qsTr("Media file path") }
            PrimaryButton { text: qsTr("Browse"); iconName: "folder"; quiet: true; enabled: !root.dubbing.processing; onClicked: root.browseRequested() }
        }
    }

    component FieldProxy: TextField {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        readOnly: true
        leftPadding: Theme.paddingMedium
        rightPadding: Theme.paddingMedium
        background: Rectangle { radius: Theme.radiusSmall; color: Qt.rgba(1, 1, 1, 0.035); border.color: parent.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.09); border.width: parent.activeFocus ? 2 : 1 }
    }

    component PreviewModeButton: Button {
        id: modeButton
        property bool selected: false
        property string iconName: ""
        implicitHeight: 26
        padding: 0
        contentItem: Row {
            anchors.centerIn: parent
            spacing: 5
            LineIcon {
                name: modeButton.iconName
                color: modeButton.enabled
                       ? (modeButton.selected ? Theme.textPrimary : Theme.textSecondary)
                       : Qt.rgba(0.56, 0.56, 0.69, 0.42)
                width: 13
                height: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: modeButton.text
                color: modeButton.enabled
                       ? (modeButton.selected ? Theme.textPrimary : Theme.textSecondary)
                       : Qt.rgba(0.56, 0.56, 0.69, 0.42)
                font.pixelSize: 11
                font.bold: modeButton.selected
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        background: Rectangle {
            radius: 6
            color: modeButton.selected ? Theme.surfaceAlt
                                       : (modeButton.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
            border.color: modeButton.selected ? Qt.rgba(0.64, 0.49, 1, 0.5) : "transparent"
        }
        HoverHandler { cursorShape: modeButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }

    component MixerControl: RowLayout {
        id: mixerControl
        property string label: ""
        property alias value: levelSlider.value
        signal moved()
        spacing: Theme.paddingSmall
        Text {
            text: mixerControl.label
            color: mixerControl.enabled ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 11
            Layout.preferredWidth: 66
        }
        ParameterSlider {
            id: levelSlider
            Layout.fillWidth: true
            Layout.minimumWidth: 72
            from: 0
            to: 1
            value: 1
            stepSize: 0.01
            enabled: mixerControl.enabled
            onMoved: mixerControl.moved()
        }
        Text {
            text: Math.round(levelSlider.value * 100) + "%"
            color: mixerControl.enabled ? Theme.textSecondary : Qt.rgba(0.56, 0.56, 0.69, 0.5)
            font.pixelSize: 10
            font.family: "Monospace"
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 34
        }
    }

    DubbingMediaQueueDialog {
        id: mediaQueueDialog
        dubbing: root.dubbing
    }

}
