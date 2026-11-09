#include "FabPreview.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 3 ||
        FabPreview::isImage(QUrl("https://media.fab.com.evil.test/image_previews/a.jpg")) ||
        !FabPreview::isListing(QUrl("https://www.fab.com/listings/28b5d05e-1237-44c3-a7f3-fae9b6d45796")) ||
        FabPreview::isListing(QUrl("https://www.fab.com/library")))
        return 2;
    QNetworkAccessManager network;
    QTimer::singleShot(0, &app, [&] {
        FabPreview::save(network, QUrl(args[1]), args[2], [&] {
            const auto file = FabPreview::pathFor(args[2]);
            QImageReader reader(file);
            const auto size = reader.size();
            const auto bytes = QFileInfo(file).size();
            qInfo("Preview %d x %d, %lld bytes", size.width(), size.height(), bytes);
            app.exit(size.isValid() && size.width() <= 256 && size.height() <= 256 &&
                     bytes > 0 && bytes < 64 * 1024 ? 0 : 1);
        });
    });
    return app.exec();
}
