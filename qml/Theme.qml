pragma Singleton
import QtQuick

QtObject {
    signal requestShowDownloads()

    // === 1. ELEVATED SURFACES (TẦNG NỀN CÓ CHIỀU SÂU) ===
    readonly property color background:     "#13121d"  // Canvas nền sâu nhất
    readonly property color surfaceLevel1:  "#1a1928"  // Sidebar, thanh trạng thái
    readonly property color surfaceLevel2:  "#242236"  // Feature Cards, Studio workspace
    readonly property color surfaceLevel3:  "#302d47"  // Input fields, Dropdowns, Toolbars
    readonly property color surfaceLevel4:  "#403d5e"  // Active, Hover states

    // Backward compatibility aliases
    readonly property color surface:        surfaceLevel2
    readonly property color surfaceAlt:     surfaceLevel3
    readonly property color border:         "#4f4b73"
    readonly property color borderSubtle:   Qt.rgba(1, 1, 1, 0.10)
    readonly property color borderFocus:    "#a27eff"

    // === 2. ACCENT & SEMANTIC COLORS (ĐIỂM NHẤN & TRẠNG THÁI) ===
    readonly property color accent:         "#7c4dff"
    readonly property color primary:        accent
    readonly property color accentLight:    "#a27eff"
    readonly property color accentHover:    "#b89eff"
    readonly property color accentMuted:    Qt.rgba(0.49, 0.30, 1.0, 0.18)

    readonly property color danger:         "#f87171"
    readonly property color error:          danger
    readonly property color success:        "#4ade80"
    readonly property color warning:        "#fbbf24"

    // === 3. HIGH-CONTRAST TYPOGRAPHY (CHỮ TƯƠNG PHẢN CAO WCAG AAA) ===
    readonly property color textPrimary:         "#ffffff"  // Trắng tinh (15:1)
    readonly property color textSecondary:       "#dedaf5"  // Trắng tím sáng (9.2:1)
    readonly property color textSecondaryBright: "#f3f1ff"  // Trắng sáng nổi bật (13:1)
    readonly property color textMuted:           "#aea8d1"  // Chữ phụ (5.6:1 - WCAG AA)
    readonly property color textPlaceholder:     "#8d87b3"  // Chữ gợi ý input (4.5:1)
    readonly property color textOnAccent:        "#ffffff"  // Chữ trên nền tím/xanh

    // === 4. SPACING & RADII (KHOẢNG CÁCH & BO GÓC CÔNG THÁI HỌC) ===
    readonly property int radiusSmall:   8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge:  16

    readonly property int paddingSmall:   8
    readonly property int paddingMedium: 12
    readonly property int paddingLarge:  16
    readonly property int paddingXL:     24

    readonly property int fontSmall:  12
    readonly property int fontMedium: 14
    readonly property int fontLarge:  18
    readonly property int fontTitle:  24
    readonly property int fontXLarge: fontTitle

    readonly property int sidebarWidth: 220
    readonly property int iconSize:     20
}

