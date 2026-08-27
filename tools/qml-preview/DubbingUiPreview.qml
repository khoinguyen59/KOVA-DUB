import QtQuick
import LAStudio
import "../../qml"

// The preview uses the production application shell and route. Only the
// AppController singleton is replaced by the local preview shim.
Main {
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    initialPreviewRouteId: "studio-dubbing"
}
