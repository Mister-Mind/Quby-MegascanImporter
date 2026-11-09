#include "AssetLibrary.h"
#include "FabPreview.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
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
            finishExport(success, {});
        else
            finishExport({}, failure.isEmpty() ? "Blender could not finish the export." : failure);
    });
    connect(&worker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finishExport({}, "Could not start Blender.");
    });
}

bool AssetLibrary::busy() const { return totalExports > 0; }
QString AssetLibrary::status() const { return message; }
QStringList AssetLibrary::exportErrors() const { return errors; }
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
    exportAssets({source});
}

void AssetLibrary::exportAssets(const QStringList &sources)
{
    if (busy()) return;
    errors.clear();
    pendingExports = sources;
    pendingExports.removeAll(QString());
    pendingExports.removeDuplicates();
    if (pendingExports.isEmpty()) return;
    const QDir app(QCoreApplication::applicationDirPath());
    exporterScript = app.filePath("blender/export_asset.py");
    blenderExecutable = QStandardPaths::findExecutable("blender.exe");
    const QString installed = "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe";
    if (QFileInfo::exists(installed)) blenderExecutable = installed;
    if (blenderExecutable.isEmpty() || !QFileInfo::exists(exporterScript)) {
        pendingExports.clear();
        message = "Export needs Blender 5.2 and the Quby export scripts.";
        emit changed();
        return;
    }
    totalExports = pendingExports.size();
    succeeded = 0;
    startNextExport();
}

void AssetLibrary::startNextExport()
{
    currentSource = pendingExports.takeFirst();
    const QFileInfo source(currentSource);
    if (!source.isFile()) {
        finishExport({}, "Source file is missing.");
        return;
    }
    QStringList args{"--background", "--disable-autoexec", "--python-exit-code", "1",
                     "--python", exporterScript, "--", source.absoluteFilePath(), folder()};
    const auto preview = FabPreview::pathFor(currentSource);
    if (QFileInfo(preview).isFile()) args << "--preview" << QFileInfo(preview).absoluteFilePath();
    logTail.clear();
    message = QString("Exporting %1 of %2 · %3")
                  .arg(totalExports - pendingExports.size()).arg(totalExports).arg(source.fileName());
    worker.start(blenderExecutable, args);
    emit changed();
}

void AssetLibrary::finishExport(const QString &success, const QString &error)
{
    if (currentSource.isEmpty()) return;
    if (error.isEmpty())
        ++succeeded;
    else
        errors.append(QFileInfo(currentSource).fileName() + ": " + error);
    currentSource.clear();
    if (!pendingExports.isEmpty()) {
        QTimer::singleShot(0, this, &AssetLibrary::startNextExport);
        return;
    }
    if (totalExports == 1) {
        message = error.isEmpty() ? "Exported to " + success + ". Refresh the Asset Browser in Blender." : error;
    } else {
        message = QString("Exported %1 of %2 assets.").arg(succeeded).arg(totalExports);
        if (!errors.isEmpty())
            message += QString(" %1 failed.").arg(errors.size());
        if (succeeded)
            message += " Refresh the Asset Browser in Blender.";
    }
    totalExports = 0;
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
