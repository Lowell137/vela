// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests of the player-to-window matching of the preview's media strip.
//
//   ctest --test-dir build -R mediamatch

#include "mediamatch.h"

#include <QTest>

namespace {

QVariantMap player(const QString& service, const QString& entry, const QString& status, const QString& title)
{
    return { { "service", "org.mpris.MediaPlayer2." + service }, { "desktopEntry", entry },
        { "status", status }, { "title", title } };
}

QString svc(const QVariantMap& m) { return m.value("service").toString(); }

} // namespace

class MediaMatchTest : public QObject {
    Q_OBJECT

private slots:
    void desktopEntry()
    {
        const QVariantList ps { player("spotify", "spotify", "Playing", "Song") };
        QCOMPARE(svc(MediaMatch::pick(ps, "spotify", "Spotify", 1)), QString("org.mpris.MediaPlayer2.spotify"));
        QCOMPARE(svc(MediaMatch::pick(ps, "Spotify.desktop", "x", 1)), QString("org.mpris.MediaPlayer2.spotify"));
        QVERIFY(MediaMatch::pick(ps, "firefox", "x", 1).isEmpty());
    }
    void busName()
    {
        const QVariantList ps { player("chromium.instance42", "", "Playing", "Clip") };
        QVERIFY(!MediaMatch::pick(ps, "chromium", "Clip - YouTube", 1).isEmpty());
        QVERIFY(MediaMatch::pick(ps, "brave", "Clip", 1).isEmpty());
    }
    void identity()
    {
        // YouTube Music (Electron): chromium.instanceNNN bus name, no
        // DesktopEntry, the window class as Identity.
        QVariantMap p = player("chromium.instance7", "", "Playing", "Song");
        p["identity"] = "com.github.th-ch.youtube-music";
        QVERIFY(!MediaMatch::pick({ p }, "com.github.th-ch.youtube-music", "Song", 1).isEmpty());
        QVERIFY(!MediaMatch::pick({ p }, "com.github.th_ch.youtube_music", "Song", 1).isEmpty());
        QVERIFY(MediaMatch::pick({ p }, "zen", "Song", 1).isEmpty());
    }
    void entryBeatsName()
    {
        const QVariantList ps { player("vlc", "", "Playing", "A"), player("x", "vlc", "Playing", "B") };
        QCOMPARE(MediaMatch::pick(ps, "vlc", "w", 1).value("title").toString(), QString("B"));
    }
    void severalWindows()
    {
        const QVariantList ps { player("firefox.instance1", "firefox", "Playing", "Tune") };
        QVERIFY(!MediaMatch::pick(ps, "firefox", "Tune - YouTube", 2).isEmpty());
        QVERIFY(MediaMatch::pick(ps, "firefox", "Docs", 2).isEmpty()); // the other window
        QVERIFY(!MediaMatch::pick(ps, "firefox", "Docs", 1).isEmpty()); // alone: a tab of it
    }
    void severalPlayers()
    {
        const QVariantList ps { player("chromium.instance1", "", "Playing", "One"), player("chromium.instance2", "", "Playing", "Two") };
        QCOMPARE(MediaMatch::pick(ps, "chromium", "Two - YouTube", 1).value("title").toString(), QString("Two"));
        QVERIFY(MediaMatch::pick(ps, "chromium", "Docs", 1).isEmpty());
    }
    void visibility()
    {
        auto one = [](const QString& status, const QString& title) {
            return MediaMatch::pick({ player("mpv", "mpv", status, title) }, "mpv", "mpv", 1);
        };
        QVERIFY(!one("Playing", "").isEmpty());
        QVERIFY(!one("Paused", "Track").isEmpty());
        QVERIFY(one("Paused", "").isEmpty());
        QVERIFY(one("Stopped", "Track").isEmpty());
        QVERIFY(MediaMatch::pick({}, "mpv", "mpv", 1).isEmpty());
    }
};

QTEST_APPLESS_MAIN(MediaMatchTest)
#include "mediamatch.moc"
