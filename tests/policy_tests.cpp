#include <QtTest>
#include "Policy.h"

class PolicyTests : public QObject {
    Q_OBJECT
private slots:
    void blocksNativeAndInsecureNavigation() {
        QVERIFY(!Policy::allowNavigation(QUrl("file:///C:/Windows/System32")));
        QVERIFY(!Policy::allowNavigation(QUrl("javascript:alert(1)")));
        QVERIFY(!Policy::allowNavigation(QUrl("http://fab.com")));
        QVERIFY(!Policy::allowNavigation(QUrl("https://user:pass@fab.com")));
        QVERIFY(Policy::allowNavigation(QUrl("https://www.fab.com/search?q=rock")));
        QVERIFY(Policy::allowNavigation(QUrl("https://www.epicgames.com/id/login")));
    }
    void sanitizesServerNamesWithoutEscapingDirectory() {
        QCOMPARE(Policy::safeFileName("../../rock.zip"), QString("rock.zip"));
        QCOMPARE(Policy::safeFileName("C:\\evil\\rock.glb"), QString("rock.glb"));
        QCOMPARE(Policy::safeFileName("rock:stream.fbx"), QString("rock_stream.fbx"));
        QVERIFY(Policy::safeFileName("CON.zip").startsWith("asset_"));
        QVERIFY(Policy::safeFileName("nul.ZIP").startsWith("asset_"));
        QVERIFY(Policy::safeFileName("LPT1.fbx").startsWith("asset_"));
        QVERIFY(Policy::safeFileName("   ").isEmpty());
    }
    void rejectsExecutableAndUnknownDownloads() {
        QVERIFY(Policy::supportedDownload("rock.ZIP"));
        QVERIFY(Policy::supportedDownload("rock.glb"));
        QVERIFY(!Policy::supportedDownload("rock.zip.exe"));
        QVERIFY(!Policy::supportedDownload("script.py"));
        QVERIFY(!Policy::supportedDownload("rock.7z"));
    }
    void uniquePathsNeverOverwriteExistingFiles() {
        QTemporaryDir dir;
        QString error;
        const auto a = Policy::allocatePath(dir.path(), "../rock.zip", &error);
        const auto b = Policy::allocatePath(dir.path(), "rock.zip", &error);
        QVERIFY2(!a.isEmpty(), qPrintable(error));
        QVERIFY(a != b);
        QVERIFY(a.startsWith(dir.path() + "/"));
        QCOMPARE(QFileInfo(a).fileName(), QString("rock.zip"));
        QVERIFY(QFileInfo(QFileInfo(a).absolutePath()).isDir());
        QVERIFY(Policy::allocatePath(dir.path(), "bad.exe", &error).isEmpty());
    }
};
QTEST_GUILESS_MAIN(PolicyTests)
#include "policy_tests.moc"
