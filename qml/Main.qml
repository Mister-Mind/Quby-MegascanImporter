import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs
import QtQuick.Layouts
import Quby.Native

ApplicationWindow {
    id: app
    width: 1320
    height: 820
    minimumWidth: 850
    minimumHeight: 570
    visible: true
    title: "Quby - MegascanImporter"
    color: "#171917"
    font.family: "Segoe UI"
    font.pixelSize: 13

    readonly property color canvas: "#171917"
    readonly property color panel: "#21231f"
    readonly property color edge: "#3c3e36"
    readonly property color ink: "#f0ebdf"
    readonly property color muted: "#b5b4a5"
    readonly property color brass: "#d6b465"
    property int page: 0
    property bool browserRequested: false
    property string exportSource: ""
    property string deletePath: ""
    onClosing: function(close) {
        if (assetLibrary.busy)
            close.accepted = false
    }

    NativeDialogs.FileDialog {
        id: sourceDialog
        title: "Choose an asset file"
        nameFilters: ["Asset files (*.zip *.fbx *.glb *.gltf *.png *.jpg *.jpeg *.webp *.exr *.tif *.tiff)"]
        onAccepted: app.prepareExport(selectedFile)
    }
    function prepareExport(file) {
        exportSource = assetLibrary.localPath(file)
        exportDialog.open()
    }
    Dialog {
        id: exportDialog
        title: "Export to Blender"
        anchors.centerIn: parent
        width: Math.min(450, app.width - 40)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            app.page = 3
            assetLibrary.exportAsset(app.exportSource)
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Export to the Megascans library. Quby uses the saved Fab preview when available or prepares a thumbnail in Blender."
            }
        }
    }
    Dialog {
        id: deleteDialog
        title: "Remove from Blender library?"
        anchors.centerIn: parent
        width: Math.min(370, app.width - 40)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: assetLibrary.removeAsset(app.deletePath)
        contentItem: Label {
            wrapMode: Text.WordWrap
            text: "Move the exported Blender file to the Recycle Bin."
        }
    }
    Component.onCompleted: {
        if (Qt.application.arguments.indexOf("--open-fab") !== -1)
            Qt.callLater(openFab)
    }
    palette.window: canvas
    palette.windowText: ink
    palette.text: ink
    palette.buttonText: ink
    palette.highlight: brass
    palette.highlightedText: canvas

    component ToolButton: Button {
        id: control
        property bool selected: false
        property bool accent: false
        implicitHeight: 34
        horizontalPadding: 12
        contentItem: Text {
            text: control.text
            font: control.font
            color: control.enabled ? (control.accent ? app.canvas : control.selected ? app.brass : app.ink) : "#818378"
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }
        background: Rectangle {
            color: control.accent && control.enabled ? (control.hovered ? "#e7c782" : app.brass)
                  : control.down || control.selected ? "#37372e" : control.hovered ? "#30322b" : app.panel
            border.color: control.activeFocus || control.selected ? app.brass : app.edge
        }
    }

    component Caption: Label {
        color: app.muted
        font.pixelSize: 12
    }
    component Headline: Label {
        color: app.ink
        font.pixelSize: 24
        font.weight: Font.DemiBold
    }
    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: app.edge
    }

    function openFab() {
        page = 0
        browserRequested = true
        browser.start()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: app.panel
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 19
                anchors.rightMargin: 20
                spacing: 13
                Image {
                    source: "qrc:/quby/icon.png"
                    Layout.preferredWidth: 35
                    Layout.preferredHeight: 35
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                Label { text: "Quby"; color: app.ink; font.pixelSize: 21; font.weight: Font.DemiBold }
                Rectangle { width: 1; height: 24; color: app.edge }
                Caption { text: "MegascanImporter" }
                Item { Layout.fillWidth: true }
                Caption { text: "Blender workstation"; color: app.brass }
            }
        }
        Divider {}
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 158
                color: app.panel
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 13
                    spacing: 8
                    ToolButton { Layout.fillWidth: true; text: "Browse Fab"; selected: app.page === 0; onClicked: app.page = 0 }
                    ToolButton { Layout.fillWidth: true; text: "Downloads"; selected: app.page === 1; onClicked: app.page = 1 }
                    ToolButton { Layout.fillWidth: true; text: "Blender Library"; selected: app.page === 3; onClicked: { app.page = 3; assetLibrary.refresh() } }
                    ToolButton { Layout.fillWidth: true; text: "Settings"; selected: app.page === 2; onClicked: app.page = 2 }
                    Item { Layout.fillHeight: true }
                    Caption { text: "Development build"; Layout.leftMargin: 4 }
                    Caption { text: "0.2.0"; Layout.leftMargin: 4 }
                }
            }
            Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: app.edge }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    visible: app.page === 0
                    spacing: 0
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 49
                        color: app.canvas
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 13
                            anchors.rightMargin: 13
                            spacing: 6
                            ToolButton { text: "Back"; enabled: browser.canBack; onClicked: browser.back() }
                            ToolButton { text: "Forward"; enabled: browser.canForward; onClicked: browser.forward() }
                            ToolButton { text: "Reload"; enabled: app.browserRequested; onClicked: browser.reload() }
                            ToolButton { text: "Quixel"; enabled: browser.ready; onClicked: browser.home() }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                color: app.panel
                                border.color: app.edge
                                Caption {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: browser.domain || "Fab"
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                    Divider {}
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 52, 430)
                            spacing: 12
                            visible: !browser.ready
                            Headline { text: "Browse Quixel on Fab" }
                            Caption {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: "Search the Quixel catalog, sign in through Fab and choose your asset format and quality. Quby asks before saving a download."
                            }
                            ToolButton {
                                text: app.browserRequested ? "Retry Fab" : "Open Fab"
                                accent: true
                                enabled: browser.status !== "Starting browser"
                                onClicked: app.openFab()
                            }
                            Caption {
                                visible: app.browserRequested
                                text: browser.status
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        NativeBrowser {
                            id: browser
                            anchors.fill: parent
                            visible: app.page === 0 && ready
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    visible: app.page === 1
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Headline { text: "Downloads" }
                        Item { Layout.fillWidth: true }
                        ToolButton { text: "Open folder"; onClicked: browser.openFolder("") }
                    }
                    Caption { text: "Downloaded asset files." }
                    Divider {}
                    Label {
                        visible: browser.downloads.length === 0
                        text: "No downloads yet"
                        color: app.muted
                        Layout.topMargin: 12
                    }
                    ToolButton {
                        visible: browser.downloads.length === 0
                        text: "Browse Fab"
                        onClicked: app.openFab()
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: browser.downloads
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 74
                            color: app.panel
                            border.color: app.edge
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 11
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Label { Layout.fillWidth: true; text: modelData.name; color: app.ink; elide: Text.ElideMiddle }
                                    Caption { text: modelData.state + "  /  " + modelData.received }
                                    ProgressBar { Layout.fillWidth: true; visible: modelData.active; value: modelData.progress }
                                }
                                ToolButton {
                                    text: modelData.active ? "Cancel" : "Show folder"
                                    onClicked: modelData.active ? browser.cancelDownload(modelData.path) : browser.openFolder(modelData.path)
                                }
                                ToolButton {
                                    visible: modelData.state === "Saved"
                                    enabled: !assetLibrary.busy
                                    text: "Export to Blender"
                                    onClicked: app.prepareExport(modelData.path)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    visible: app.page === 3
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Headline { text: "Blender Library" }
                        Item { Layout.fillWidth: true }
                        ToolButton { text: "Open folder"; onClicked: assetLibrary.openFolder() }
                        ToolButton { text: "Export file…"; accent: true; enabled: !assetLibrary.busy; onClicked: sourceDialog.open() }
                    }
                    Divider {}
                    Caption {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: assetLibrary.status || "Export a download to add it to Blender’s Asset Browser."
                        textFormat: Text.PlainText
                    }
                    ProgressBar { visible: assetLibrary.busy; indeterminate: true; Layout.fillWidth: true }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: assetLibrary.assets
                        spacing: 6
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 54
                            color: app.panel
                            border.color: app.edge
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 9
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    textFormat: Text.PlainText
                                    elide: Text.ElideMiddle
                                }
                                ToolButton {
                                    text: "Remove"
                                    enabled: !assetLibrary.busy
                                    onClicked: { app.deletePath = modelData.path; deleteDialog.open() }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    visible: app.page === 2
                    spacing: 12
                    Headline { text: "Settings" }
                    Divider {}
                    Label { text: "Download folder"; color: app.ink }
                    TextField {
                        Layout.fillWidth: true
                        readOnly: true
                        text: browser.downloadFolder
                        selectByMouse: true
                        color: app.ink
                        background: Rectangle { color: app.panel; border.color: app.edge }
                    }
                    ToolButton { text: "Open folder"; onClicked: browser.openFolder("") }
                    Caption {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Fab sign-in is managed on Fab. Quby keeps its browser session in a separate profile."
                    }
                    Item { Layout.fillHeight: true }
                }
            }
            Rectangle {
                visible: browser.pendingName.length > 0
                Layout.fillHeight: true
                Layout.preferredWidth: 242
                color: app.panel
                border.color: app.edge
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    Headline { text: "Save asset"; font.pixelSize: 20 }
                    Label {
                        Layout.fillWidth: true
                        text: browser.pendingName
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        color: app.ink
                    }
                    Caption {
                        Layout.fillWidth: true
                        text: browser.pendingSize
                        wrapMode: Text.WordWrap
                    }
                    Caption {
                        Layout.fillWidth: true
                        text: browser.pendingSource
                        wrapMode: Text.WrapAnywhere
                    }
                    ToolButton { Layout.fillWidth: true; text: "Save to Downloads"; accent: true; onClicked: browser.acceptDownload() }
                    ToolButton { Layout.fillWidth: true; text: "Cancel"; onClicked: browser.rejectDownload() }
                    Item { Layout.fillHeight: true }
                }
            }
        }
        Divider {}
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 29
            color: app.panel
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 13
                anchors.rightMargin: 13
                Caption { text: browser.status; elide: Text.ElideRight; Layout.fillWidth: true }
                Caption { text: browser.downloads.length + (browser.downloads.length === 1 ? " download" : " downloads") }
            }
        }
    }
}
