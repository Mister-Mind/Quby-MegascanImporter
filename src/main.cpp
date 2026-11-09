#include "Browser.h"
#include "AssetLibrary.h"
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <windows.h>
#include <objbase.h>

int main(int argc, char *argv[])
{
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result;
    {
        QApplication app(argc, argv);
        // Retain the storage identity used by existing downloads and Blender libraries.
        app.setApplicationName("Foundry");
        app.setApplicationDisplayName("Quby - MegascanImporter");
        app.setOrganizationName("Foundry");
        app.setWindowIcon(QIcon(":/quby/icon.png"));
        QQuickStyle::setStyle("Basic");
        qmlRegisterType<Browser>("Quby.Native", 1, 0, "NativeBrowser");
        AssetLibrary library;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("assetLibrary", &library);
        engine.load(QUrl("qrc:/quby/Main.qml"));
        result = engine.rootObjects().isEmpty() ? 1 : app.exec();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
