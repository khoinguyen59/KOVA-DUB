import QtQuick
import LAStudio

// Compatibility wrapper for older AppButton call sites.  Keep one shared
// implementation for hit targets, loading state, typography, and a11y.
PrimaryButton {
    id: root

    property bool primary: false

    buttonColor: primary ? Theme.accent : Theme.surfaceAlt
    textColor: primary ? "#ffffff" : Theme.textPrimary
    quiet: !primary
}
