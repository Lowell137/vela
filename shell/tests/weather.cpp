// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The weather widget: WMO codes, parsing Open-Meteo's answer, the
// taskbar/weather* keys, "off means no request" and the cache when the network
// is gone. A fake fetcher stands in for the network.

#include "config.h"
#include "weather.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace vela::weather;

namespace {
const char kAnswer[] = R"({
  "current": {"time": "2026-10-14T15:00", "temperature_2m": 18.4, "apparent_temperature": 16.6,
              "relative_humidity_2m": 62, "weather_code": 3, "wind_speed_10m": 20.0, "is_day": 1},
  "hourly": {"time": [], "temperature_2m": [], "weather_code": [], "is_day": []},
  "daily": {"time": ["2026-10-14", "2026-10-15"], "weather_code": [3, 63],
            "temperature_2m_max": [19.0, 14.2], "temperature_2m_min": [9.5, 8.0]}
})";

QByteArray answer()
{
    // 24 hourly points of 2026-10-14, 0:00 to 23:00: the current hour is 15.
    QJsonObject root = QJsonDocument::fromJson(kAnswer).object();
    QJsonArray time, temp, code, day;
    for (int h = 0; h < 24; ++h) {
        time.append(QStringLiteral("2026-10-14T%1:00").arg(h, 2, 10, QLatin1Char('0')));
        temp.append(h);
        code.append(h < 20 ? 3 : 0);
        day.append(h >= 6 && h < 19 ? 1 : 0);
    }
    QJsonObject hourly { { "time", time }, { "temperature_2m", temp }, { "weather_code", code }, { "is_day", day } };
    root["hourly"] = hourly;
    return QJsonDocument(root).toJson();
}

struct FakeNetwork {
    int requests = 0;
    QUrl last;
    QByteArray reply;
    Fetcher fetcher()
    {
        return [this](const QUrl& url, const Callback& done) {
            ++requests;
            last = url;
            done(reply);
        };
    }
};
} // namespace

class WeatherTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Vela"));
        QCoreApplication::setApplicationName(QStringLiteral("vela-shell-test"));
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_dir.path());
    }

    void wmoCodes()
    {
        QCOMPARE(iconName(0, true), QStringLiteral("weather-clear"));
        QCOMPARE(iconName(0, false), QStringLiteral("weather-clear-night"));
        QCOMPARE(iconName(2, false), QStringLiteral("weather-few-clouds-night"));
        QCOMPARE(iconName(3, true), QStringLiteral("weather-overcast"));
        QCOMPARE(iconName(45, true), QStringLiteral("weather-fog"));
        QCOMPARE(iconName(61, true), QStringLiteral("weather-showers-scattered"));
        QCOMPARE(iconName(65, true), QStringLiteral("weather-showers"));
        QCOMPARE(iconName(67, true), QStringLiteral("weather-freezing-rain"));
        QCOMPARE(iconName(75, true), QStringLiteral("weather-snow"));
        QCOMPARE(iconName(96, true), QStringLiteral("weather-storm"));
        QCOMPARE(iconName(1234, true), QStringLiteral("weather-none-available"));
        QCOMPARE(description(0), QStringLiteral("Clear"));
        QCOMPARE(description(95), QStringLiteral("Thunderstorm"));
        QCOMPARE(description(1234), QStringLiteral("Unknown"));
    }

    void parsing()
    {
        const Forecast f = parseForecast(QJsonDocument::fromJson(answer()).object());
        QVERIFY(f.valid);
        QCOMPARE(f.humidity, 62);
        QCOMPARE(f.code, 3);
        QCOMPARE(int(f.hours.size()), 9); // 15:00 .. 23:00, there are no more points
        QCOMPARE(f.hours.front().time, QStringLiteral("2026-10-14T15:00"));
        QVERIFY(!f.hours.back().day);
        QCOMPARE(int(f.days.size()), 2);
        QCOMPARE(f.days[1].max, 14.2);
        QVERIFY(!parseForecast(QJsonDocument::fromJson("{\"error\":true}").object()).valid);
    }

    void places()
    {
        const QVariantList p = parsePlaces(R"({"results":[{"name":"Izmir","latitude":38.4189,"longitude":27.12872,
            "admin1":"Izmir","country":"Turkey"},{"name":"bad"}]})");
        QCOMPARE(p.size(), 1);
        QCOMPARE(p[0].toMap()["lat"].toString(), QStringLiteral("38.4189"));
        QCOMPARE(p[0].toMap()["detail"].toString(), QStringLiteral("Izmir, Turkey"));
        QVERIFY(parsePlaces("not json").isEmpty());
        QVERIFY(searchUrl(QStringLiteral("São Paulo"), QStringLiteral("tr")).toString().contains(QLatin1String("geocoding-api.open-meteo.com")));
    }

    void configKeys()
    {
        QSettings().clear();
        {
            Config config;
            QVERIFY(!config.weather());
            QCOMPARE(config.weatherUnit(), QStringLiteral("c"));
        }
        {
            QSettings s;
            s.setValue(QStringLiteral("taskbar/weather"), QStringLiteral("on"));
            s.setValue(QStringLiteral("taskbar/weather-city"), QStringLiteral("Izmir"));
            s.setValue(QStringLiteral("taskbar/weather-lat"), QStringLiteral("38.4189"));
            s.setValue(QStringLiteral("taskbar/weather-lon"), QStringLiteral("27.1287"));
            s.setValue(QStringLiteral("taskbar/weather-unit"), QStringLiteral("F"));
        }
        Config config;
        QVERIFY(config.weather());
        QCOMPARE(config.weatherCity(), QStringLiteral("Izmir"));
        QCOMPARE(config.weatherLat(), QStringLiteral("38.4189"));
        QCOMPARE(config.weatherUnit(), QStringLiteral("f"));
        QSettings().setValue(QStringLiteral("taskbar/weather"), QStringLiteral("off"));
        Config off;
        QVERIFY(!off.weather());
    }

    void offMakesNoRequest()
    {
        FakeNetwork net;
        net.reply = answer();
        Weather w;
        w.setCachePath(m_dir.filePath(QStringLiteral("off.json")));
        w.setFetcher(net.fetcher());
        w.setConfig(false, QStringLiteral("Izmir"), QStringLiteral("38.4"), QStringLiteral("27.1"), QStringLiteral("c"));
        w.refresh();
        QCOMPARE(net.requests, 0);
        // On, but without a place (never guessed): still nothing.
        w.setConfig(true, QString(), QString(), QString(), QStringLiteral("c"));
        w.refresh();
        QCOMPARE(net.requests, 0);
        QVERIFY(!w.shown());
        // Out-of-range coordinates are not a place.
        w.setConfig(true, QStringLiteral("x"), QStringLiteral("123"), QStringLiteral("27.1"), QStringLiteral("c"));
        QCOMPARE(net.requests, 0);
    }

    void fetchUnitsAndCache()
    {
        const QString cache = m_dir.filePath(QStringLiteral("weather.json"));
        FakeNetwork net;
        net.reply = answer();
        {
            Weather w;
            w.setCachePath(cache);
            w.setFetcher(net.fetcher());
            w.setConfig(true, QStringLiteral("Izmir"), QStringLiteral("38.4189"), QStringLiteral("27.1287"), QStringLiteral("c"));
            QCOMPARE(net.requests, 1);
            QCOMPARE(net.last.host(), QStringLiteral("api.open-meteo.com"));
            QVERIFY(w.shown() && w.available() && !w.stale());
            QCOMPARE(w.temperature(), 18);
            QCOMPARE(w.feelsLike(), 17);
            QCOMPARE(w.windSpeed(), 20);
            QCOMPARE(w.icon(), QStringLiteral("weather-overcast"));
            QCOMPARE(w.hourly().size(), 9);
            QCOMPARE(w.daily().size(), 2);

            // Fahrenheit and mph: converted here, no new request.
            w.setConfig(true, QStringLiteral("Izmir"), QStringLiteral("38.4189"), QStringLiteral("27.1287"), QStringLiteral("f"));
            QCOMPARE(net.requests, 1);
            QCOMPARE(w.temperature(), 65);
            QCOMPARE(w.windUnit(), QStringLiteral("mph"));
            QCOMPARE(w.windSpeed(), 12);

            // The network goes: the last value stays, marked old.
            net.reply.clear();
            w.refresh();
            QCOMPARE(net.requests, 2);
            QVERIFY(w.available() && w.stale());

            // Off: nothing else is asked.
            w.setConfig(false, QStringLiteral("Izmir"), QStringLiteral("38.4189"), QStringLiteral("27.1287"), QStringLiteral("f"));
            w.refresh();
            QCOMPARE(net.requests, 2);
        }
        // A new session without network: the cache, marked old.
        Weather again;
        again.setCachePath(cache);
        again.setFetcher(net.fetcher());
        again.setConfig(true, QStringLiteral("Izmir"), QStringLiteral("38.4189"), QStringLiteral("27.1287"), QStringLiteral("c"));
        QVERIFY(again.available() && again.stale());
        QCOMPARE(again.temperature(), 18);
        // Another place does not use that cache.
        Weather other;
        other.setCachePath(cache);
        other.setFetcher(net.fetcher());
        other.setConfig(true, QStringLiteral("Rome"), QStringLiteral("41.9"), QStringLiteral("12.5"), QStringLiteral("c"));
        QVERIFY(!other.available());
    }

    void search()
    {
        FakeNetwork net;
        net.reply = R"({"results":[{"name":"Izmir","latitude":38.4,"longitude":27.1,"country":"Turkey"}]})";
        PlaceSearch search;
        search.setFetcher(net.fetcher());
        search.search(QStringLiteral("i")); // too short: no request
        QCOMPARE(net.requests, 0);
        search.search(QStringLiteral("Izmir"));
        QCOMPARE(net.requests, 1);
        QCOMPARE(search.results().size(), 1);
        QVERIFY(!search.failed());
        net.reply.clear();
        search.search(QStringLiteral("Izmir"));
        QVERIFY(search.failed());
    }
};

QTEST_GUILESS_MAIN(WeatherTest)
#include "weather.moc"
