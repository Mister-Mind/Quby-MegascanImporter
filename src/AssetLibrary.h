#pragma once
#include <QObject>
#include <QProcess>
#include <QVariantList>

class AssetLibrary : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QStringList exportErrors READ exportErrors NOTIFY changed)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY changed)
    Q_PROPERTY(QString folder READ folder CONSTANT)
public:
    explicit AssetLibrary(QObject *parent = nullptr);
    bool busy() const;
    QString status() const;
    QStringList exportErrors() const;
    QString folder() const;
    QVariantList assets() const;
    Q_INVOKABLE void exportAsset(const QString &source);
    Q_INVOKABLE void exportAssets(const QStringList &sources);
    Q_INVOKABLE void removeAsset(const QString &path);
    Q_INVOKABLE void openFolder();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString localPath(const QVariant &value) const;
signals:
    void changed();
private:
    void startNextExport();
    void finishExport(const QString &success, const QString &error);
    QProcess worker;
    QString message;
    QByteArray logTail;
    QStringList pendingExports;
    QStringList errors;
    QString currentSource;
    QString blenderExecutable;
    QString exporterScript;
    int totalExports = 0;
    int succeeded = 0;
};
