pragma Singleton
import QtQuick

QtObject {
    signal requestShowDownloads()

    readonly property color background: "#13121d"
    readonly property color surfaceLevel1: "#1a1928"
    readonly property color surfaceLevel2: "#242236"
    readonly property color surfaceLevel3: "#302d47"
    readonly property color surfaceLevel4: "#403d5e"
    readonly property color surface: surfaceLevel2
    readonly property color surfaceAlt: surfaceLevel3
    readonly property color border: "#4f4b73"
    readonly property color borderSubtle: Qt.rgba(1, 1, 1, 0.10)
    readonly property color borderFocus: "#a27eff"

    readonly property color accent: "#7c4dff"
    readonly property color primary: accent
    readonly property color accentLight: "#a27eff"
    readonly property color accentHover: "#b89eff"
    readonly property color accentMuted: Qt.rgba(0.49, 0.30, 1.0, 0.18)
    readonly property color danger: "#f87171"
    readonly property color error: danger
    readonly property color success: "#4ade80"
    readonly property color warning: "#fbbf24"

    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#c7c2dc"
    readonly property color textSecondaryBright: "#f3f1ff"
    readonly property color textMuted: "#aea8d1"
    readonly property color textPlaceholder: "#8d87b3"
    readonly property color textOnAccent: "#ffffff"

    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 16
    readonly property int paddingSmall: 8
    readonly property int paddingMedium: 12
    readonly property int paddingLarge: 16
    readonly property int paddingXL: 24
    readonly property int fontXSmall: 11
    readonly property int fontSmall: 12
    readonly property int fontMedium: 14
    readonly property int fontLarge: 18
    readonly property int fontTitle: 24
    readonly property int fontXLarge: fontTitle
    readonly property int sidebarWidth: 220
    readonly property int iconSize: 20
}
