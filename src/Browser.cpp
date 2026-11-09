#include "Browser.h"
#include "Policy.h"
#include "FabPreview.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QLocale>
#include <QDebug>
#include <QSet>
#include <QFile>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <windows.h>
#include <objbase.h>
#include <WebView2.h>
#include <wrl.h>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
const wchar_t *fabHome = L"https://www.fab.com/sellers/Quixel";
QString takeString(LPWSTR text)
{
    const QString value = text ? QString::fromWCharArray(text) : QString();
    CoTaskMemFree(text);
    return value;
}
QString formatBytes(qint64 bytes)
{
    return bytes < 0 ? QStringLiteral("Size unknown") : QLocale().formattedDataSize(bytes);
}
}

struct Browser::State {
    struct Job {
        QString path;
        QString state;
        qint64 bytes = 0;
        qint64 total = -1;
        ComPtr<ICoreWebView2DownloadOperation> operation;
        bool preparingPreview = false;
    };
    HWND host = nullptr;
    QRect bounds;
    bool visible = false;
    bool starting = false;
    bool loading = false;
    QString domain = "www.fab.com";
    QString error;
    QString source;
    QString name;
    QUrl listing;
    QNetworkAccessManager network;
    qint64 pendingBytes = -1;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> web;
    ComPtr<ICoreWebView2DownloadStartingEventArgs> pending;
    ComPtr<ICoreWebView2Deferral> deferral;
    std::vector<Job> jobs;
};

Browser::Browser(QQuickItem *parent) : QQuickItem(parent), d(std::make_unique<State>())
{
    timer.setInterval(200);
    connect(&timer, &QTimer::timeout, this, [this] {
        updateHost();
        pollDownloads();
    });
}
Browser::~Browser()
{
    if (d->pending) {
        d->pending->put_Cancel(TRUE);
        d->deferral->Complete();
    }
    for (auto &job : d->jobs)
        if (job.operation)
            job.operation->Cancel();
    if (d->controller)
        d->controller->Close();
    if (d->host)
        DestroyWindow(d->host);
}

QString Browser::domain() const { return d->domain; }
QString Browser::status() const
{
    if (!d->error.isEmpty()) return d->error;
    if (d->loading) return "Loading page";
    if (d->web) return "Browser ready";
    return d->starting ? "Starting browser" : "Browser paused";
}
bool Browser::ready() const { return bool(d->web); }
bool Browser::canBack() const
{
    BOOL available = FALSE;
    if (d->web) d->web->get_CanGoBack(&available);
    return available;
}
bool Browser::canForward() const
{
    BOOL available = FALSE;
    if (d->web) d->web->get_CanGoForward(&available);
    return available;
}
QString Browser::pendingName() const { return d->name; }
QString Browser::pendingSize() const { return formatBytes(d->pendingBytes); }
QString Browser::pendingSource() const { return d->source; }
QString Browser::downloadFolder() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath("Downloads");
}
QVariantList Browser::downloads() const
{
    QVariantList list;
    QSet<QString> sessionPaths;
    for (auto it = d->jobs.rbegin(); it != d->jobs.rend(); ++it) {
        const auto &job = *it;
        sessionPaths.insert(job.path);
        list.append(QVariantMap{
            {"path", job.path}, {"name", QFileInfo(job.path).fileName()},
            {"state", job.state == "Saved" && job.preparingPreview ? "Preparing preview" : job.state},
            {"active", bool(job.operation)},
            {"received", formatBytes(job.bytes)},
            {"progress", job.total > 0 ? double(job.bytes) / double(job.total) : 0.0}
        });
    }
    const QDir root(downloadFolder());
    for (const auto &directory : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot |
                                                     QDir::NoSymLinks, QDir::Time)) {
        const QDir jobFolder(directory.absoluteFilePath());
        if (!jobFolder.entryList({"*.crdownload"}, QDir::Files | QDir::Hidden).isEmpty())
            continue;
        for (const auto &file : jobFolder.entryInfoList(QDir::Files | QDir::NoSymLinks)) {
            if (!file.fileName().startsWith(".foundry-preview-") &&
                Policy::supportedDownload(file.fileName()) && file.size() > 0 &&
                !sessionPaths.contains(file.absoluteFilePath()))
                list.append(QVariantMap{{"path", file.absoluteFilePath()}, {"name", file.fileName()},
                    {"state", "Saved"}, {"active", false}, {"received", formatBytes(file.size())},
                    {"progress", 1.0}});
        }
    }
    return list;
}

void Browser::start()
{
    if (d->starting || d->web || !window()) return;
    d->error.clear();
    d->starting = true;
    emit changed();
    if (!d->host) {
        d->host = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_CLIPCHILDREN,
            0, 0, 0, 0, reinterpret_cast<HWND>(window()->winId()), nullptr,
            GetModuleHandleW(nullptr), nullptr);
    }
    const QString profile = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath("FabProfile");
    if (!d->host || !QDir().mkpath(profile)) {
        d->starting = false;
        d->error = "Could not start the browser or create its local profile.";
        emit changed();
        return;
    }
    QPointer<Browser> self(this);
    const auto fail = [self](QString message, HRESULT result) {
        if (!self) return;
        self->d->starting = false;
        self->d->error = message + QString(" (0x%1)").arg(quint32(result), 8, 16, QLatin1Char('0'));
        emit self->changed();
    };
    const auto completed = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [self, fail](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
            if (!self) return S_OK;
            if (FAILED(result) || !env) {
                fail("WebView2 could not start", result);
                return S_OK;
            }
            const auto created = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [self, fail](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
                    if (!self) {
                        if (controller) controller->Close();
                        return S_OK;
                    }
                    if (FAILED(result) || !controller) {
                        fail("Could not open the embedded browser", result);
                        return S_OK;
                    }
                    auto &state = *self->d;
                    state.controller = controller;
                    controller->put_IsVisible(FALSE);
                    result = controller->get_CoreWebView2(&state.web);
                    if (FAILED(result) || !state.web) {
                        controller->Close();
                        state.controller.Reset();
                        fail("Could not initialize the browser", result);
                        return S_OK;
                    }
                    auto web = state.web;
                    ComPtr<ICoreWebView2Settings> settings;
                    if (SUCCEEDED(web->get_Settings(&settings))) {
                        settings->put_IsWebMessageEnabled(FALSE);
                        settings->put_AreHostObjectsAllowed(FALSE);
                        settings->put_AreDevToolsEnabled(FALSE);
                    }
                    EventRegistrationToken token{};
                    web->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [self](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) {
                            if (!self) return S_OK;
                            LPWSTR uri = nullptr;
                            args->get_Uri(&uri);
                            const QUrl url(takeString(uri));
                            if (!Policy::allowNavigation(url)) {
                                args->put_Cancel(TRUE);
                                return S_OK;
                            }
                            self->d->loading = true;
                            self->d->error.clear();
                            emit self->changed();
                            return S_OK;
                        }).Get(), &token);
                    web->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
                        [self](ICoreWebView2 *sender, ICoreWebView2SourceChangedEventArgs *) {
                            if (!self) return S_OK;
                            LPWSTR uri = nullptr;
                            sender->get_Source(&uri);
                            self->d->domain = QUrl(takeString(uri)).host();
                            emit self->changed();
                            return S_OK;
                        }).Get(), &token);
                    web->add_HistoryChanged(Callback<ICoreWebView2HistoryChangedEventHandler>(
                        [self](ICoreWebView2 *, IUnknown *) {
                            if (self) emit self->changed();
                            return S_OK;
                        }).Get(), &token);
                    web->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [self](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) {
                            if (!self) return S_OK;
                            BOOL ok = FALSE;
                            COREWEBVIEW2_WEB_ERROR_STATUS error{};
                            args->get_IsSuccess(&ok);
                            args->get_WebErrorStatus(&error);
                            self->d->loading = false;
                            if (!ok && error != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED
                                && error != COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED) {
                                self->d->error = QString("Page could not load (%1). Try Reload.").arg(int(error));
                            }
                            qInfo().noquote() << "Browser navigation" << self->d->domain
                                              << (ok ? "loaded" : "failed")
                                              << int(error);
                            emit self->changed();
                            return S_OK;
                        }).Get(), &token);
                    web->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                        [self](ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args) {
                            args->put_Handled(TRUE);
                            if (!self) return S_OK;
                            BOOL userInitiated = FALSE;
                            args->get_IsUserInitiated(&userInitiated);
                            LPWSTR uri = nullptr;
                            args->get_Uri(&uri);
                            const QString target = takeString(uri);
                            if (userInitiated && Policy::allowNavigation(QUrl(target)))
                                sender->Navigate(reinterpret_cast<LPCWSTR>(target.utf16()));
                            return S_OK;
                        }).Get(), &token);
                    web->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
                        [](ICoreWebView2 *, ICoreWebView2PermissionRequestedEventArgs *args) {
                            return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
                        }).Get(), &token);
                    ComPtr<ICoreWebView2_4> downloadApi;
                    result = web.As(&downloadApi);
                    if (SUCCEEDED(result)) {
                        result = downloadApi->add_DownloadStarting(
                            Callback<ICoreWebView2DownloadStartingEventHandler>(
                                [self](ICoreWebView2 *sender, ICoreWebView2DownloadStartingEventArgs *args) {
                                    args->put_Handled(TRUE);
                                    if (!self || self->d->pending) {
                                        args->put_Cancel(TRUE);
                                        return S_OK;
                                    }
                                    LPWSTR raw = nullptr;
                                    args->get_ResultFilePath(&raw);
                                    const QString name = Policy::safeFileName(takeString(raw));
                                    ComPtr<ICoreWebView2DownloadOperation> operation;
                                    args->get_DownloadOperation(&operation);
                                    if (!operation || !Policy::supportedDownload(name)) {
                                        args->put_Cancel(TRUE);
                                        self->d->error = "Download blocked. Choose a supported asset file.";
                                        emit self->changed();
                                        return S_OK;
                                    }
                                    auto &s = *self->d;
                                    if (FAILED(args->GetDeferral(&s.deferral)) || !s.deferral) {
                                        args->put_Cancel(TRUE);
                                        s.error = "Could not pause the download for confirmation.";
                                        emit self->changed();
                                        return S_OK;
                                    }
                                    s.pending = args;
                                    s.name = name;
                                    s.pendingBytes = -1;
                                    operation->get_TotalBytesToReceive(&s.pendingBytes);
                                    LPWSTR uri = nullptr;
                                    operation->get_Uri(&uri);
                                    s.source = QUrl(takeString(uri)).host();
                                    LPWSTR page = nullptr;
                                    sender->get_Source(&page);
                                    s.listing = QUrl(takeString(page));
                                    emit self->changed();
                                    return S_OK;
                                }).Get(), &token);
                    }
                    if (FAILED(result)) {
                        state.web.Reset();
                        controller->Close();
                        state.controller.Reset();
                        fail("Could not enable managed downloads", result);
                        return S_OK;
                    }
                    state.starting = false;
                    self->timer.start();
                    self->updateHost();
                    self->home();
                    emit self->changed();
                    return S_OK;
                });
            result = env->CreateCoreWebView2Controller(self->d->host, created.Get());
            if (FAILED(result)) fail("Could not create the browser window", result);
            return S_OK;
        });
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(nullptr,
        reinterpret_cast<LPCWSTR>(profile.utf16()), nullptr, completed.Get());
    if (FAILED(result)) fail("WebView2 Runtime could not start", result);
}

void Browser::updateHost()
{
    if (!d->controller || !window()) return;
    const qreal scale = window()->devicePixelRatio();
    const auto origin = mapToScene(QPointF(0, 0)) * scale;
    const QRect bounds(qRound(origin.x()), qRound(origin.y()),
                       qRound(width() * scale), qRound(height() * scale));
    if (bounds != d->bounds) {
        d->bounds = bounds;
        SetWindowPos(d->host, nullptr, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
                     SWP_NOACTIVATE | SWP_NOZORDER);
        RECT client{0, 0, bounds.width(), bounds.height()};
        d->controller->put_Bounds(client);
        d->controller->NotifyParentWindowPositionChanged();
    }
    const bool show = isVisible() && window()->isVisible() && !bounds.isEmpty();
    if (show != d->visible) {
        d->visible = show;
        ShowWindow(d->host, show ? SW_SHOWNA : SW_HIDE);
        d->controller->put_IsVisible(show);
    }
}

void Browser::back() { if (canBack()) d->web->GoBack(); }
void Browser::forward() { if (canForward()) d->web->GoForward(); }
void Browser::reload() { if (d->web) d->web->Reload(); else start(); }
void Browser::home() { if (d->web) d->web->Navigate(fabHome); }

void Browser::acceptDownload()
{
    if (!d->pending) return;
    QString error;
    const auto path = Policy::allocatePath(downloadFolder(), d->name, &error);
    ComPtr<ICoreWebView2DownloadOperation> operation;
    d->pending->get_DownloadOperation(&operation);
    const auto native = QDir::toNativeSeparators(path);
    if (path.isEmpty() || !operation ||
        FAILED(d->pending->put_ResultFilePath(reinterpret_cast<LPCWSTR>(native.utf16())))) {
        d->error = error.isEmpty() ? "Could not set the download location." : error;
        rejectDownload();
        return;
    }
    d->jobs.push_back({path, "Downloading", 0, d->pendingBytes, operation});
    capturePreview(path, d->listing);
    auto deferral = std::move(d->deferral);
    d->pending.Reset();
    d->name.clear();
    deferral->Complete();
    emit changed();
    emit downloadsChanged();
}

void Browser::capturePreview(const QString &path, const QUrl &listing)
{
    if (!d->web || !FabPreview::isListing(listing)) return;
    d->jobs.back().preparingPreview = true;
    QPointer<Browser> self(this);
    const auto done = [self, path] {
        if (!self) return;
        for (auto &job : self->d->jobs) {
            if (job.path == path && job.preparingPreview) {
                job.preparingPreview = false;
                emit self->downloadsChanged();
                break;
            }
        }
    };
    QTimer::singleShot(10000, this, done);
    const auto started = d->web->ExecuteScript(LR"JS((() => {
        const card = document.querySelector('meta[property="og:image"]')?.content;
        const gallery = document.querySelector('img[alt^="Media 1 for listing "]')?.src;
        return [location.href, card || gallery || ""];
    })())JS", Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [self, path, listing, done](HRESULT result, LPCWSTR json) {
            if (!self || FAILED(result) || !json) {
                done();
                return S_OK;
            }
            const auto fields = QJsonDocument::fromJson(QString::fromWCharArray(json).toUtf8()).array();
            if (fields.size() != 2) {
                done();
                return S_OK;
            }
            const QUrl page(fields[0].toString());
            if (FabPreview::isListing(page) && page.host() == listing.host() &&
                page.path() == listing.path())
                FabPreview::save(self->d->network, QUrl(fields[1].toString()), path, done);
            else
                done();
            return S_OK;
        }).Get());
    if (FAILED(started)) done();
}

void Browser::rejectDownload()
{
    if (!d->pending) return;
    d->pending->put_Cancel(TRUE);
    auto deferral = std::move(d->deferral);
    d->pending.Reset();
    d->name.clear();
    deferral->Complete();
    emit changed();
}

void Browser::pollDownloads()
{
    bool changed = false;
    for (auto &job : d->jobs) {
        if (!job.operation) continue;
        qint64 bytes = 0;
        COREWEBVIEW2_DOWNLOAD_STATE state{};
        job.operation->get_BytesReceived(&bytes);
        if (job.bytes != bytes) {
            job.bytes = bytes;
            changed = true;
        }
        if (SUCCEEDED(job.operation->get_State(&state)) &&
            state != COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) {
            COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON reason{};
            job.operation->get_InterruptReason(&reason);
            if (state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED)
                job.state = "Saved";
            else if (reason == COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_CANCELED)
                job.state = "Canceled";
            else
                job.state = QString("Interrupted (%1)").arg(int(reason));
            job.operation.Reset();
            changed = true;
        }
    }
    if (changed) emit downloadsChanged();
}

void Browser::cancelDownload(const QString &path)
{
    for (const auto &job : d->jobs)
        if (job.path == path && job.operation) job.operation->Cancel();
}

QString Browser::deleteDownload(const QString &path)
{
    if (!Policy::isManagedDownloadPath(downloadFolder(), path))
        return "Quby can only delete files in its managed download queue.";
    const auto job = std::find_if(d->jobs.begin(), d->jobs.end(),
                                  [&path](const auto &entry) { return entry.path == path; });
    if (job != d->jobs.end() && (job->operation || job->preparingPreview))
        return "Wait for the download and its preview to finish.";
    if (QFile::exists(path) && !QFile::moveToTrash(path))
        return "Could not move the download to the Recycle Bin.";

    QString message = "Download removed.";
    const QFileInfo preview(FabPreview::pathFor(path));
    if (preview.isFile() && !preview.isSymLink() && !QFile::moveToTrash(preview.absoluteFilePath()))
        message = "Download removed. Its cached preview could not be moved to the Recycle Bin.";
    if (job != d->jobs.end())
        d->jobs.erase(job);
    const auto folder = QFileInfo(path).absolutePath();
    QDir().rmdir(folder); // Only remove an empty job folder.
    emit downloadsChanged();
    return message;
}

void Browser::openFolder(const QString &path)
{
    const QString folder = path.isEmpty() ? downloadFolder() : QFileInfo(path).absolutePath();
    if (QDir().mkpath(folder)) QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}
