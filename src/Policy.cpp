#include "Policy.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

bool Policy::allowNavigation(const QUrl &url)
{
    return url.isValid() && url.scheme() == "https" && !url.host().isEmpty()
           && url.userInfo().isEmpty();
}

QString Policy::safeFileName(QString name)
{
    name.replace('\\', '/');
    name = name.section('/', -1).trimmed();
    name.replace(QRegularExpression(R"([<>:"|?*\x00-\x1f])"), "_");
    while (name.endsWith('.') || name.endsWith(' '))
        name.chop(1);
    const auto stem = name.section('.', 0, 0).trimmed().toUpper();
    static const QRegularExpression device("^(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])$");
    if (device.match(stem).hasMatch())
        name.prepend("asset_");
    return name.size() <= 180 ? name : QString();
}

bool Policy::supportedDownload(const QString &name)
{
    static const QSet<QString> extensions{
        "zip", "fbx", "gltf", "glb", "png", "jpg", "jpeg", "exr"
    };
    return extensions.contains(QFileInfo(name).suffix().toLower());
}

bool Policy::isManagedDownloadPath(const QString &root, const QString &path)
{
    const QFileInfo file(path);
    const QFileInfo jobFolder(file.absolutePath());
    const auto canonicalRoot = QFileInfo(root).canonicalFilePath();
    return file.isAbsolute() && !file.isSymLink() && !jobFolder.isSymLink() &&
           !canonicalRoot.isEmpty() && jobFolder.dir().canonicalPath() == canonicalRoot &&
           jobFolder.canonicalFilePath() == QDir(canonicalRoot).filePath(jobFolder.fileName()) &&
           !QUuid::fromString(jobFolder.fileName()).isNull() &&
           !file.fileName().startsWith(".foundry-preview-", Qt::CaseInsensitive) &&
           supportedDownload(file.fileName()) && (!file.exists() || file.isFile());
}

QString Policy::allocatePath(const QString &directory, const QString &name, QString *error)
{
    const QString safe = safeFileName(name);
    if (!supportedDownload(safe)) {
        *error = "This file type is not supported.";
        return {};
    }
    QDir root(directory);
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(root.absolutePath()) || !root.mkdir(id)) {
        *error = "Could not create a folder for this download.";
        return {};
    }
    return root.absoluteFilePath(id + '/' + safe);
}
