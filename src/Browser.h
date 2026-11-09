#pragma once
#include <QQuickItem>
#include <QTimer>
#include <QVariantList>
#include <memory>

class Browser : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QString domain READ domain NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
    Q_PROPERTY(bool canForward READ canForward NOTIFY changed)
    Q_PROPERTY(QString pendingName READ pendingName NOTIFY changed)
    Q_PROPERTY(QString pendingSize READ pendingSize NOTIFY changed)
    Q_PROPERTY(QString pendingSource READ pendingSource NOTIFY changed)
    Q_PROPERTY(QString downloadFolder READ downloadFolder NOTIFY changed)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
public:
    explicit Browser(QQuickItem *parent = nullptr);
    ~Browser() override;
    QString domain() const;
    QString status() const;
    bool ready() const;
    bool canBack() const;
    bool canForward() const;
    QString pendingName() const;
    QString pendingSize() const;
    QString pendingSource() const;
    QString downloadFolder() const;
    QVariantList downloads() const;
    Q_INVOKABLE void start();
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void home();
    Q_INVOKABLE void acceptDownload();
    Q_INVOKABLE void rejectDownload();
    Q_INVOKABLE void cancelDownload(const QString &path);
    Q_INVOKABLE QString deleteDownload(const QString &path);
    Q_INVOKABLE void openFolder(const QString &path = {});
signals:
    void changed();
    void downloadsChanged();
private:
    void updateHost();
    void pollDownloads();
    void capturePreview(const QString &path, const QUrl &listing);
    struct State;
    std::unique_ptr<State> d;
    QTimer timer;
};
