---
name: qt6-qml-expert
description: Master Qt 6, QML, Qt Quick Controls 2, responsive desktop layout, singleton Theme architecture, high-DPI scaling, and fluid state animations.
---

# Qt 6 & QML Desktop Application Engineering Skill

This skill provides production standards and battle-tested patterns for building high-performance, accessible, and stunning Qt 6 / QML desktop applications.

---

## 1. Core Principles

### 1.1 Responsive Desktop Layout & Zero Clipping
- **Never hardcode fixed window dimensions or rigid breakpoints.**
- Always use `Layout.fillWidth: true`, `Layout.fillHeight: true`, with dynamic bounds:
  ```qml
  Layout.preferredWidth: Math.max(minWidth, Math.min(maxWidth, parent.width * ratio))
  ```
- **Every scrollable container or variable-height list MUST be wrapped in a `ScrollView` or `Flickable` with explicit clipping**:
  ```qml
  ScrollView {
      Layout.fillWidth: true
      Layout.fillHeight: true
      clip: true
      contentWidth: availableWidth
      ScrollBar.vertical.policy: ScrollBar.AsNeeded
  }
  ```
- **Dialogs & Popups Safety Margin**:
  ```qml
  implicitHeight: Math.min(preferredHeight, Overlay.overlay ? Overlay.overlay.height - 48 : preferredHeight)
  ```

### 1.2 Contrast & Typography (WCAG 2.2 Level AA/AAA)
- Minimum contrast ratio for standard text: **$4.5:1$** (WCAG AA) / **$7.0:1$** (WCAG AAA).
- Primary headers & active labels: Pure or near-white (`#ffffff`, `#f3f1ff`).
- Secondary & description text: Bright lavender/neutral (`#dedaf5` or `#dcd7f5`), NEVER murky grays (`#555` or `#666`).
- Muted labels & timestamps: Minimum `#aea8d1` ($5.6:1$).
- Input placeholders: Minimum `#8d87b3` ($4.5:1$).

### 1.3 State Management & C++ Integration
- Expose controllers via `Q_PROPERTY` with `NOTIFY` signals for full 2-way data binding.
- Use `Q_INVOKABLE` or public slots for imperative actions.
- Avoid calling heavy operations directly in QML expressions or property bindings.
- Prefer `Connections` with specific target guards to prevent dangling signal handlers.

---

## 2. Component Design System

### 2.1 Singleton Theme Architecture
Always reference colors, spacing, and radiuses from a centralized `Theme.qml` singleton:
- **Surface Elevation Hierarchy**:
  - `Theme.background`: Deepest canvas (`#12111a`)
  - `Theme.surfaceLevel1`: Sidebars, header, footer (`#1c1b29`)
  - `Theme.surfaceLevel2`: Feature Cards, Workspaces (`#262438`)
  - `Theme.surfaceLevel3`: Input fields, Dropdowns (`#32304a`)
  - `Theme.surfaceLevel4`: Active/Hover states (`#423f60`)
- **Semantic Colors**:
  - `Theme.accent`: `#8b5cf6` (Primary action)
  - `Theme.success`: `#4ade80` (Pass / Finished)
  - `Theme.warning`: `#fbbf24` (Caution / Incomplete)
  - `Theme.danger`: `#f87171` (Destructive / Error)

### 2.2 Micro-Interactions & Hover Polish
- Add subtle hover feedback to interactive items:
  ```qml
  Behavior on color { ColorAnimation { duration: 140 } }
  Behavior on opacity { NumberAnimation { duration: 140 } }
  ```
- Interactive touch/click targets must be at least $36 \times 36$px (preferred $40 \times 40$px).

---

## 3. Performance & Memory Optimization
- **Delegates in Repeater/ListView**: Use `ListView` with `reuseItems: true` instead of `Repeater` for datasets larger than 20 items.
- **Image & Video Caching**: Set `asynchronous: true` on `Image` loaders.
- **Clip selectively**: `clip: true` forces an extra render pass / scissor test; only apply to true scrolling bounds.
