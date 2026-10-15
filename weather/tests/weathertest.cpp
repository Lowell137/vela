// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What vela-weather makes of Open-Meteo's answers (saved real samples in
// tests/data), its tables and units, the places file, the cache and the model
// with a fake fetcher. Nothing here touches the network.

#include "weather.h"
#include "weathermodel.h"

#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

using namespace vela::weather;

namespace {
QByteArray sample(const char* name)
{
    QFile f(QStringLiteral(WEATHER_TEST_DATA "/") + QLatin1String(name));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QJsonObject json(const char* name) { return QJsonDocument::fromJson(sample(name)).object(); }

void write(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
} // namespace

class WeatherTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QLocale::setDefault(QLocale::c()); }

    // --- parsers ---
    void forecastSample()
    {
        const Forecast f = parseForecast(json("forecast.json"));
        QVERIFY(f.valid);
        QCOMPARE(f.timezone, QStringLiteral("Europe/Istanbul"));
        QCOMPARE(f.current.time, QStringLiteral("2026-10-15T05:45"));
        QCOMPARE(f.current.temp, 15.8);
        QCOMPARE(f.current.feels, 11.8);
        QCOMPARE(f.current.code, 61);
        QCOMPARE(f.current.humidity, 84);
        QCOMPARE(f.current.pressure, 1013.5);
        QCOMPARE(f.current.visibility, 27660.0);
        QVERIFY(!f.current.day); // is_day 0 at 05:45
        // The strip starts at the current hour and holds a day.
        QCOMPARE(int(f.hours.size()), 24);
        QCOMPARE(f.hours.front().time, QStringLiteral("2026-10-15T05:00"));
        QVERIFY(f.hours.front().precipChance >= 0);
        QCOMPARE(int(f.days.size()), 10);
        QCOMPARE(f.days.front().date, QStringLiteral("2026-10-15"));
        QCOMPARE(f.days.front().max, 16.7);
        QCOMPARE(f.days.front().sunrise, QStringLiteral("2026-10-15T07:15"));
        QCOMPARE(f.days.front().precipChance, 100);
    }

    void forecastGarbage()
    {
        QVERIFY(!parseForecast({}).valid);
        QVERIFY(!parseForecast(QJsonDocument::fromJson("{\"current\":{\"temperature_2m\":3}}").object()).valid);
        QVERIFY(!parseForecast(QJsonDocument::fromJson("{\"error\":true,\"reason\":\"x\"}").object()).valid);
    }

    void forecastWithoutOptionalFields()
    {
        // Only what's required: the rest falls back, nothing crashes.
        const auto doc = QJsonDocument::fromJson("{\"current\":{\"time\":\"2026-01-01T10:20\",\"temperature_2m\":-0.2,\"weather_code\":71},"
                                                 "\"daily\":{\"time\":[\"2026-01-01\"],\"temperature_2m_max\":[1],\"temperature_2m_min\":[-3],\"weather_code\":[71]}}");
        const Forecast f = parseForecast(doc.object());
        QVERIFY(f.valid);
        QCOMPARE(f.current.feels, -0.2);
        QCOMPARE(f.current.visibility, -1.0);
        QVERIFY(f.hours.empty());
        QCOMPARE(f.days.front().precipChance, -1);
    }

    void airQuality()
    {
        const AirQuality a = parseAirQuality(json("airquality.json"));
        QVERIFY(a.valid);
        QCOMPARE(a.aqi, 22);
        QCOMPARE(a.pm25, 5.2);
        QCOMPARE(a.pm10, 9.3);
        QVERIFY(!parseAirQuality({}).valid);
        QVERIFY(!parseAirQuality(QJsonDocument::fromJson("{\"current\":{\"european_aqi\":null,\"pm10\":3}}").object()).valid);
    }

    void geocodingSample()
    {
        const auto places = parsePlaces(sample("geocoding.json"));
        QCOMPARE(int(places.size()), 3);
        QCOMPARE(places[0].name, QStringLiteral("Springfield"));
        QCOMPARE(places[0].detail(), QStringLiteral("Missouri, United States"));
        QCOMPARE(places[0].timezone, QStringLiteral("America/Chicago"));
        // No administrative area in the answer: just the country.
        QCOMPARE(places[2].detail(), QStringLiteral("United States"));
        QVERIFY(parsePlaces("not json").empty());
        QVERIFY(parsePlaces("{\"generationtime_ms\":0.4}").empty()); // no "results" when nothing matches
    }

    void geocodingDuplicatesAndGaps()
    {
        const QByteArray body = R"({"results":[
            {"name":"Paris","latitude":48.85,"longitude":2.35,"country":"France","admin1":"Île-de-France"},
            {"name":"Paris","latitude":48.86,"longitude":2.34,"country":"France","admin1":"Île-de-France"},
            {"name":"Paris","latitude":33.66,"longitude":-95.55,"country":"United States","admin1":"Texas"},
            {"name":"Nowhere","country":"X"},
            {"latitude":1,"longitude":2},
            {"name":"Far","latitude":95,"longitude":2},
            {"name":"Luxembourg","latitude":49.6,"longitude":6.1,"country":"Luxembourg","admin1":"Luxembourg"}]})";
        const auto places = parsePlaces(body);
        QCOMPARE(int(places.size()), 3); // the near twin, the one without coordinates, name or sane latitude are gone
        QCOMPARE(places[1].region, QStringLiteral("Texas"));
        QCOMPARE(places[2].detail(), QStringLiteral("Luxembourg")); // no "Luxembourg, Luxembourg"
    }

    // --- tables ---
    void everyDocumentedCodeHasIconAndText()
    {
        const int codes[] = { 0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99 };
        for (const int code : codes) {
            QVERIFY2(knownCode(code), qPrintable(QString::number(code)));
            QVERIFY(iconName(code, true).startsWith(QLatin1String("weather-")));
            QVERIFY(iconName(code, true) != QLatin1String("weather-none-available"));
            QVERIFY(iconName(code, false) != QLatin1String("weather-none-available"));
            QVERIFY(description(code) != QLatin1String("Unknown"));
            QVERIFY(!description(code).isEmpty());
        }
        QVERIFY(!knownCode(4));
        QCOMPARE(iconName(4, true), QStringLiteral("weather-none-available"));
        QCOMPARE(description(4), QStringLiteral("Unknown"));
        QCOMPARE(iconName(0, true), QStringLiteral("weather-clear"));
        QCOMPARE(iconName(0, false), QStringLiteral("weather-clear-night"));
        QVERIFY(skyOf(0) == Sky::Clear && skyOf(65) == Sky::Rain && skyOf(75) == Sky::Snow && skyOf(95) == Sky::Storm
            && skyOf(45) == Sky::Fog && skyOf(3) == Sky::Cloudy);
    }

    // --- units ---
    void conversions()
    {
        QCOMPARE(convertTemperature(100, TemperatureUnit::Fahrenheit), 212.0);
        QCOMPARE(convertTemperature(-40, TemperatureUnit::Fahrenheit), -40.0);
        QCOMPARE(convertTemperature(21.5, TemperatureUnit::Celsius), 21.5);
        QVERIFY(qAbs(convertWind(36, WindUnit::Ms) - 10) < 1e-9);
        QVERIFY(qAbs(convertWind(100, WindUnit::Mph) - 62.1371) < 1e-3);
        QVERIFY(qAbs(convertWind(100, WindUnit::Knots) - 53.9957) < 1e-3);
        QVERIFY(qAbs(convertPrecipitation(25.4, PrecipitationUnit::Inch) - 1) < 1e-9);
        QVERIFY(qAbs(convertPressure(1013.25, PressureUnit::InHg) - 29.92) < 0.01);
    }

    void formatting()
    {
        Units u;
        QCOMPARE(formatTemperature(15.8, u), QStringLiteral("16°"));
        QCOMPARE(formatTemperature(-0.3, u), QStringLiteral("0°")); // never "-0"
        u.temperature = TemperatureUnit::Fahrenheit;
        QCOMPARE(formatTemperature(15.8, u), QStringLiteral("60°"));
        QCOMPARE(formatWind(34, u), QStringLiteral("34 km/h"));
        u.wind = WindUnit::Ms;
        QCOMPARE(formatWind(36, u), QStringLiteral("10.0 m/s"));
        u.wind = WindUnit::Mph;
        QCOMPARE(formatWind(100, u), QStringLiteral("62 mph"));
        u.wind = WindUnit::Knots;
        QCOMPARE(formatWind(100, u), QStringLiteral("54 kn"));
        QCOMPARE(formatPrecipitation(0.14, u), QStringLiteral("0.1 mm"));
        u.precipitation = PrecipitationUnit::Inch;
        QCOMPARE(formatPrecipitation(2.54, u), QStringLiteral("0.10 in"));
        QCOMPARE(formatPressure(1013.5, u), QStringLiteral("1014 hPa"));
        u.pressure = PressureUnit::InHg;
        QCOMPARE(formatPressure(1013.25, u), QStringLiteral("29.92 inHg"));
    }

    void clockAndCompass()
    {
        QCOMPARE(formatClock(QStringLiteral("2026-10-15T07:15"), true, QLocale::c()), QStringLiteral("07:15"));
        QCOMPARE(formatClock(QStringLiteral("2026-10-15T18:24"), false, QLocale::c()), QStringLiteral("6:24 PM"));
        QCOMPARE(formatClock(QStringLiteral("2026-10-15T00:05"), false, QLocale::c()), QStringLiteral("12:05 AM"));
        QVERIFY(formatClock(QStringLiteral("garbage"), true).isEmpty());
        QCOMPARE(compass(0), QStringLiteral("N"));
        QCOMPARE(compass(354), QStringLiteral("N"));
        QCOMPARE(compass(46), QStringLiteral("NE"));
        QCOMPARE(compass(180), QStringLiteral("S"));
        QCOMPARE(compass(359.9), QStringLiteral("N"));
        QCOMPARE(compass(-90), QStringLiteral("W"));
    }

    void bands()
    {
        QCOMPARE(aqiBand(0), 0);
        QCOMPARE(aqiBand(20), 0);
        QCOMPARE(aqiBand(21), 1);
        QCOMPARE(aqiBand(100), 4);
        QCOMPARE(aqiBand(101), 5);
        QCOMPARE(aqiBand(400), 5);
        QCOMPARE(uvClass(0), 0);
        QCOMPARE(uvClass(2.9), 0);
        QCOMPARE(uvClass(3), 1);
        QCOMPARE(uvClass(7.5), 2);
        QCOMPARE(uvClass(10), 3);
        QCOMPARE(uvClass(11), 4);
    }

    void sun()
    {
        const QString rise = QStringLiteral("2026-10-15T07:00"), set = QStringLiteral("2026-10-15T19:00");
        QCOMPARE(sunProgress(QStringLiteral("2026-10-15T13:00"), rise, set), 0.5);
        QVERIFY(sunProgress(QStringLiteral("2026-10-15T05:45"), rise, set) < 0);
        QVERIFY(sunProgress(QStringLiteral("2026-10-15T20:00"), rise, set) > 1);
        QCOMPARE(sunProgress(QStringLiteral("2026-10-15T13:00"), QString(), set), -1.0);
        // Polar night: Open-Meteo gives the same time twice or none.
        QCOMPARE(sunProgress(QStringLiteral("2026-12-15T13:00"), rise, rise), -1.0);
    }

    // --- places ---
    void placesRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/places.json"));
        PlacesStore a(path);
        QCOMPARE(a.load(), PlacesStore::Load::Missing);
        QVERIFY(a.add({ QStringLiteral("İstanbul"), QStringLiteral("İstanbul"), QStringLiteral("Türkiye"), QStringLiteral("Europe/Istanbul"), 41.01, 28.98 }));
        QVERIFY(a.add({ QStringLiteral("Roma"), QStringLiteral("Lazio"), QStringLiteral("Italia"), QStringLiteral("Europe/Rome"), 41.89, 12.48 }));
        QVERIFY(!a.add({ QStringLiteral("roma"), {}, {}, {}, 41.9, 12.5 })); // the same place
        a.setDefault(1);
        QVERIFY(a.save());

        PlacesStore b(path);
        QCOMPARE(b.load(), PlacesStore::Load::Ok);
        QCOMPARE(int(b.places().size()), 2);
        QCOMPARE(b.defaultIndex(), 1);
        QCOMPARE(b.places()[0].name, QStringLiteral("İstanbul"));
        QCOMPARE(b.places()[1].timezone, QStringLiteral("Europe/Rome"));
        QCOMPARE(b.places()[1].lat, 41.89);

        b.remove(0); // the default follows its place
        QCOMPARE(b.defaultIndex(), 0);
        QCOMPARE(b.places()[0].name, QStringLiteral("Roma"));
        b.remove(0);
        QVERIFY(b.places().empty());
        QCOMPARE(b.defaultIndex(), 0);
    }

    void placesCorruptFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("places.json"));
        write(path, "{ this is not json");
        PlacesStore s(path);
        QCOMPARE(s.load(), PlacesStore::Load::Corrupt);
        QVERIFY(s.places().empty());
        QVERIFY(QFile::exists(path + QStringLiteral(".corrupt"))); // kept for the user
        QVERIFY(!QFile::exists(path));
        // It starts clean and saves normally afterwards.
        QVERIFY(s.add({ QStringLiteral("Roma"), {}, {}, {}, 41.89, 12.48 }));
        QVERIFY(s.save());
        PlacesStore again(path);
        QCOMPARE(again.load(), PlacesStore::Load::Ok);
        QCOMPARE(int(again.places().size()), 1);

        // Valid JSON with entries that make no sense: those are skipped.
        write(path, R"({"default":7,"places":[{"name":"A","lat":1,"lon":2},{"name":"B","lat":999,"lon":2},{"lat":1,"lon":1},5]})");
        PlacesStore odd(path);
        QCOMPARE(odd.load(), PlacesStore::Load::Ok);
        QCOMPARE(int(odd.places().size()), 1);
        QCOMPARE(odd.defaultIndex(), 0);
    }

    void shellConfigPlace()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("vela-shell.conf"));
        QVERIFY(!placeFromShellConfig(path).has_value());
        write(path, "[taskbar]\nweather=on\nweather-city=Milano\nweather-lat=45.4642\nweather-lon=9.1900\n");
        const auto p = placeFromShellConfig(path);
        QVERIFY(p.has_value());
        QCOMPARE(p->name, QStringLiteral("Milano"));
        QCOMPARE(p->lat, 45.4642);
        write(path, "[taskbar]\nweather-city=Milano\n");
        QVERIFY(!placeFromShellConfig(path).has_value());
    }

    // --- cache ---
    void cacheRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = cacheFile(dir.filePath(QStringLiteral("cache")), 41.0138, 28.9784);
        QVERIFY(path.endsWith(QLatin1String("41.014_28.978.json")));
        QVERIFY(!readCache(path).has_value());
        const QDateTime t = QDate(2026, 10, 15).startOfDay(QTimeZone::UTC).addSecs(14 * 3600 + 32 * 60);
        QVERIFY(writeCache(path, { t, json("forecast.json"), json("airquality.json") }));
        const auto e = readCache(path);
        QVERIFY(e.has_value());
        QCOMPARE(e->updated, t);
        QVERIFY(parseForecast(e->forecast).valid);
        QVERIFY(parseAirQuality(e->air).valid);
        write(path, "garbage");
        QVERIFY(!readCache(path).has_value());
    }

    void staleness()
    {
        const QDateTime t = QDate(2026, 10, 15).startOfDay(QTimeZone::UTC).addSecs(14 * 3600 + 32 * 60);
        QVERIFY(!isStale(t, t));
        QVERIFY(!isStale(t, t.addSecs(14 * 60 + 59)));
        QVERIFY(isStale(t, t.addSecs(15 * 60)));
        QVERIFY(isStale(t, t.addDays(2)));
        QVERIFY(isStale(t, t.addSecs(-60))); // the clock went back
        QVERIFY(isStale(QDateTime(), t));
    }

    // --- the model, with a fetcher that answers from the samples ---
    void modelRefreshAndCache()
    {
        QTemporaryDir dir;
        const QString places = dir.filePath(QStringLiteral("places.json"));
        const QString cache = dir.filePath(QStringLiteral("cache"));
        PlacesStore s(places);
        s.add({ QStringLiteral("İstanbul"), QStringLiteral("İstanbul"), QStringLiteral("Türkiye"), {}, 41.0138, 28.9784 });
        QVERIFY(s.save());

        int requests = 0;
        bool airFails = false, forecastFails = false;
        const Fetcher fetch = [&](const QUrl& url, const Callback& done) {
            ++requests;
            const QString host = url.host();
            if (host.startsWith(QLatin1String("air-quality"))) {
                done(airFails ? QByteArray() : sample("airquality.json"));
            } else if (host.startsWith(QLatin1String("geocoding"))) {
                done(sample("geocoding.json"));
            } else {
                done(forecastFails ? QByteArray() : sample("forecast.json"));
            }
        };
        const QDateTime t0 = QDateTime::currentDateTime();

        {
            WeatherModel m(places, cache, QString(), nullptr, fetch);
            m.setClock([t0] { return t0; });
            QVERIFY(m.hasPlace());
            QVERIFY(!m.hasData());
            m.refresh();
            QVERIFY(m.hasData());
            QVERIFY(!m.offline());
            QVERIFY(!m.stale());
            QVERIFY(!m.loading());
            QCOMPARE(m.now().value(QStringLiteral("temp")).toString(), QStringLiteral("16°"));
            QCOMPARE(m.now().value(QStringLiteral("icon")).toString(), QStringLiteral("weather-showers-scattered"));
            QCOMPARE(m.hourly().size(), 24);
            QCOMPARE(m.hourly().front().toMap().value(QStringLiteral("time")).toString(), QStringLiteral("Now"));
            QCOMPARE(m.daily().size(), 10);
            QCOMPARE(m.daily().front().toMap().value(QStringLiteral("day")).toString(), QStringLiteral("Today"));
            QCOMPARE(m.details().size(), 8);
            QVERIFY(m.air().value(QStringLiteral("available")).toBool());
            QCOMPARE(m.air().value(QStringLiteral("text")).toString(), QStringLiteral("Fair"));
            QVERIFY(m.night());
            QCOMPARE(m.sky(), QStringLiteral("rain"));
            QCOMPARE(m.sun().value(QStringLiteral("rise")).toString(), QStringLiteral("07:15"));
            QVERIFY(m.sun().value(QStringLiteral("progress")).toDouble() < 0); // before the sunrise

            // Units change what's shown, not what's stored.
            m.setTemperatureUnit(QStringLiteral("f"));
            m.setHour24(false);
            QCOMPARE(m.now().value(QStringLiteral("temp")).toString(), QStringLiteral("60°"));
            QCOMPARE(m.sun().value(QStringLiteral("rise")).toString(), QStringLiteral("7:15 AM"));

            // The air quality failing is not the page failing.
            airFails = true;
            m.refresh();
            QVERIFY(!m.offline());
            QVERIFY(m.air().value(QStringLiteral("available")).toBool());

            // The forecast failing keeps the last one, marked.
            forecastFails = true;
            m.refresh();
            QVERIFY(m.offline());
            QVERIFY(m.hasData());
            QVERIFY(m.stale());
            forecastFails = false;
            m.refresh();
            QVERIFY(!m.offline());
        }

        // A new start: the cache is there before any answer, stale after 15 minutes.
        forecastFails = true;
        WeatherModel again(places, cache, QString(), nullptr, fetch);
        QVERIFY(again.hasData());
        QVERIFY(again.air().value(QStringLiteral("available")).toBool());
        again.setClock([t0] { return t0.addSecs(60); });
        QVERIFY(!again.stale());
        again.setClock([t0] { return t0.addSecs(3600); });
        QVERIFY(again.stale());
        QVERIFY(again.updatedText().startsWith(QLatin1String("Updated")));
        again.refresh(); // offline: the old data stays
        QVERIFY(again.offline());
        QVERIFY(again.hasData());
        QVERIFY(requests > 0);
    }

    void modelOfflineWithoutCache()
    {
        QTemporaryDir dir;
        const QString places = dir.filePath(QStringLiteral("places.json"));
        PlacesStore s(places);
        s.add({ QStringLiteral("Roma"), {}, {}, {}, 41.89, 12.48 });
        QVERIFY(s.save());
        WeatherModel m(places, dir.filePath(QStringLiteral("c")), QString(), nullptr, [](const QUrl&, const Callback& done) { done({}); });
        m.refresh();
        QVERIFY(m.offline());
        QVERIFY(!m.hasData());
        QVERIFY(m.hourly().isEmpty());
        QVERIFY(m.details().isEmpty());
        QVERIFY(!m.air().value(QStringLiteral("available")).toBool());
    }

    void modelSearchAddRemove()
    {
        QTemporaryDir dir;
        const QString places = dir.filePath(QStringLiteral("places.json"));
        const QString shell = dir.filePath(QStringLiteral("vela-shell.conf"));
        write(shell, "[taskbar]\nweather-city=Milano\nweather-lat=45.4642\nweather-lon=9.19\n");
        const Fetcher fetch = [](const QUrl& url, const Callback& done) {
            done(url.host().startsWith(QLatin1String("geocoding")) ? sample("geocoding.json")
                                                                   : url.host().startsWith(QLatin1String("air")) ? sample("airquality.json")
                                                                                                                 : sample("forecast.json"));
        };
        WeatherModel m(places, dir.filePath(QStringLiteral("c")), shell, nullptr, fetch);
        // First run: the taskbar widget's place is offered, never written back.
        QCOMPARE(m.places().size(), 1);
        QCOMPARE(m.places().front().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Milano"));
        QVERIFY(QFile::exists(places));

        m.search(QStringLiteral("x")); // too short: no request
        QVERIFY(!m.searching());
        m.search(QStringLiteral("Springfield"));
        QCOMPARE(m.searchResults().size(), 3);
        QVERIFY(m.addResult(1));
        QVERIFY(m.searchResults().isEmpty());
        QCOMPARE(m.places().size(), 2);
        QCOMPARE(m.placeIndex(), 1);
        QCOMPARE(m.now().value(QStringLiteral("place")).toString(), QStringLiteral("Springfield"));
        QVERIFY(m.hasData()); // the answer arrived right away: the fake is synchronous

        m.search(QStringLiteral("Springfield"));
        QVERIFY(m.searchResults().at(1).toMap().value(QStringLiteral("saved")).toBool());
        m.addResult(1); // again: no second copy
        QCOMPARE(m.places().size(), 2);

        m.remove(1); // the chosen one: back to the default
        QCOMPARE(m.places().size(), 1);
        QCOMPARE(m.placeIndex(), 0);

        // It's all on disk.
        PlacesStore disk(places);
        QCOMPARE(disk.load(), PlacesStore::Load::Ok);
        QCOMPARE(int(disk.places().size()), 1);
    }
};

QTEST_GUILESS_MAIN(WeatherTest)
#include "weathertest.moc"
