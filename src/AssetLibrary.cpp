#include "AssetLibrary.h"
#include "FabPreview.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {
QString managedName(const QString &fileName)
{
    static const QRegularExpression pattern("^(.+)--foundry-[a-f0-9]{32}\\.blend$");
    return pattern.match(fileName).captured(1);
}
}

AssetLibrary::AssetLibrary(QObject *parent) : QObject(parent)
{
    worker.setProcessChannelMode(QProcess::MergedChannels);
    const auto collect = [this] {
        logTail = (logTail + worker.readAll()).right(16384);
    };
    connect(&worker, &QProcess::readyRead, this, collect);
    connect(&worker, &QProcess::finished, this, [this, collect](int code, QProcess::ExitStatus exit) {
        collect();
        QString success;
        QString failure;
        for (const auto &line : QString::fromUtf8(logTail).split('\n')) {
            if (line.startsWith("FOUNDRY_OK ")) success = line.mid(11).trimmed();
            if (line.startsWith("FOUNDRY_ERROR ")) failure = line.mid(14).trimmed();
        }
        if (exit == QProcess::NormalExit && code == 0 && !success.isEmpty())
            message = "Exported to " + success + ". Refresh the Asset Browser in Blender.";
        else
            message = failure.isEmpty() ? "Blender could not finish the export." : failure;
        emit changed();
    });
    connect(&worker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            message = "Could not start Blender.";
            emit changed();
        }
    });
}

bool AssetLibrary::busy() const { return worker.state() != QProcess::NotRunning; }
QString AssetLibrary::status() const { return message; }
QString AssetLibrary::folder() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath("Blender Assets");
}

QVariantList AssetLibrary::assets() const
{
    QVariantList result;
    const QDir root(folder());
    for (const auto &file : root.entryInfoList({"*.blend"}, QDir::Files | QDir::NoSymLinks, QDir::Time)) {
        const auto name = managedName(file.fileName());
        if (!name.isEmpty())
            result.append(QVariantMap{{"name", name}, {"path", file.absoluteFilePath()}});
    }
    return result;
}

void AssetLibrary::exportAsset(const QString &source)
{
    if (busy()) return;
    const QDir app(QCoreApplication::applicationDirPath());
    const QString script = app.filePath("blender/export_asset.py");
    QString blender = QStandardPaths::findExecutable("blender.exe");
    const QString installed = "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe";
    if (QFileInfo::exists(installed)) blender = installed;
    if (blender.isEmpty() || !QFileInfo::exists(script) || !QFileInfo(source).isFile()) {
        message = "Export needs Blender 5.2, the Quby export scripts, and a local asset file.";
        emit changed();
        return;
    }
    QStringList args{"--background", "--disable-autoexec", "--python-exit-code", "1",
                     "--python", script, "--", QFileInfo(source).absoluteFilePath(), folder()};
    const auto preview = FabPreview::pathFor(source);
    if (QFileInfo(preview).isFile()) args << "--preview" << QFileInfo(preview).absoluteFilePath();
    logTail.clear();
    message = "Exporting asset and preparing its Blender thumbnail…";
    worker.start(blender, args);
    emit changed();
}

void AssetLibrary::removeAsset(const QString &path)
{
    if (busy()) return;
    const QFileInfo info(path);
    const auto root = QFileInfo(folder()).canonicalFilePath();
    if (root.isEmpty() || info.isSymLink() || !info.isFile() ||
        managedName(info.fileName()).isEmpty() || info.dir().canonicalPath() != root) {
        message = "Only files in Quby’s managed library can be removed here.";
    } else if (QFile::moveToTrash(info.absoluteFilePath())) {
        message = "Asset moved to the Recycle Bin. Refresh the Asset Browser in Blender.";
    } else {
        message = "Could not move the asset to the Recycle Bin.";
    }
    emit changed();
}

void AssetLibrary::openFolder()
{
    if (QDir().mkpath(folder())) QDesktopServices::openUrl(QUrl::fromLocalFile(folder()));
}

void AssetLibrary::refresh() { emit changed(); }

QString AssetLibrary::localPath(const QVariant &value) const
{
    if (value.metaType() == QMetaType::fromType<QUrl>())
        return value.toUrl().toLocalFile();
    return value.toString();
}
