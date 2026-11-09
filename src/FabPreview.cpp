#include "FabPreview.h"
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

namespace {
bool secureHost(const QUrl &url, const QString &host)
{
    return url.isValid() && url.scheme() == "https" && url.host() == host &&
           url.port(443) == 443 && url.userInfo().isEmpty();
}
}

bool FabPreview::isListing(const QUrl &url)
{
    static const QRegularExpression listing(
        "^/listings/[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}/?$",
        QRegularExpression::CaseInsensitiveOption);
    return (secureHost(url, "www.fab.com") || secureHost(url, "fab.com")) &&
           listing.match(url.path()).hasMatch();
}

bool FabPreview::isImage(const QUrl &url)
{
    return secureHost(url, "media.fab.com") &&
           url.path().startsWith("/image_previews/") &&
           (url.path().endsWith(".jpg", Qt::CaseInsensitive) ||
            url.path().endsWith(".jpeg", Qt::CaseInsensitive) ||
            url.path().endsWith(".png", Qt::CaseInsensitive) ||
            url.path().endsWith(".webp", Qt::CaseInsensitive));
}

QString FabPreview::pathFor(const QString &assetPath)
{
    return QFileInfo(assetPath).dir().filePath(".foundry-preview-" + QFileInfo(assetPath).fileName() + ".jpg");
}

void FabPreview::save(QNetworkAccessManager &network, const QUrl &url, const QString &assetPath,
                      std::function<void()> done)
{
    if (!isImage(url)) {
        done();
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    auto *reply = network.get(request);
    constexpr qsizetype maxBytes = 5 * 1024 * 1024;
    QTimer::singleShot(8000, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > maxBytes) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, assetPath] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || reply->bytesAvailable() > maxBytes ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            return;
        auto bytes = reply->readAll();
        QBuffer input(&bytes);
        input.open(QIODevice::ReadOnly);
        QImageReader reader(&input);
        const auto size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 24'000'000) return;
        const QSize outputSize = size.boundedTo(QSize(256, 256)) == size
            ? size : size.scaled(256, 256, Qt::KeepAspectRatio);
        reader.setScaledSize(outputSize);
        const auto thumbnail = reader.read();
        if (thumbnail.isNull()) return;
        QSaveFile file(pathFor(assetPath));
        if (file.open(QIODevice::WriteOnly) && thumbnail.save(&file, "JPG", 68))
            file.commit();
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, std::move(done));
}
