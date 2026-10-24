// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>
#include <vector>

class QNetworkAccessManager;

// The taskbar's weather (Open-Meteo, no API key). Off by default: with the
// widget off nothing is requested and no network object is created. The
// location is never guessed from the IP: the user searches a city in Settings
// and the coordinates are saved (taskbar/weather-*).
namespace vela::weather {

// Gets a URL; the callback receives the body, or an empty array on failure.
// Replaced in tests, so they never touch the network.
using Callback = std::function<void(const QByteArray&)>;
using Fetcher = std::function<void(const QUrl&, const Callback&)>;
Fetcher networkFetcher(QNetworkAccessManager* manager);

struct Hour {
    QString time; // "2026-10-14T15:00", local to the place
    double temp = 0; // always °C and km/h inside; converted for display
    int code = 0;
    bool day = true;
};
struct Day {
    QString date; // "2026-10-14"
    double min = 0;
    double max = 0;
    int code = 0;
};
struct Forecast {
    bool valid = false;
    double temp = 0, feels = 0, wind = 0;
    int humidity = 0, code = 0;
    bool day = true;
    std::vector<Hour> hours; // the next 12, from the current hour
    std::vector<Day> days; // today and the following days
};

// WMO weather code (as Open-Meteo reports it) -> freedesktop icon name and an
// English, translatable text.
QString iconName(int code, bool day);
QString description(int code);

QUrl forecastUrl(double lat, double lon);
QUrl searchUrl(const QString& query, const QString& language);
Forecast parseForecast(const QJsonObject& root);
// [{name, detail, lat, lon}] from a geocoding answer.
QVariantList parsePlaces(const QByteArray& body);

// {ok, lat, lon, city} from an IP geolocation answer (ipwho.is).
struct IpPlace {
    bool ok = false;
    double lat = 0, lon = 0;
    QString city;
};
IpPlace parseIpPlace(const QByteArray& body);

} // namespace vela::weather

class Weather : public QObject {
    Q_OBJECT
    // The widget is on and has a place: show it (stale or not).
    Q_PROPERTY(bool shown READ shown NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // The last refresh failed (or the data is old): the numbers come from the cache.
    Q_PROPERTY(bool stale READ stale NOTIFY changed)
    Q_PROPERTY(QString city READ city NOTIFY changed)
    Q_PROPERTY(QString unit READ unit NOTIFY changed)
    Q_PROPERTY(QString icon READ icon NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(int temperature READ temperature NOTIFY changed)
    Q_PROPERTY(int feelsLike READ feelsLike NOTIFY changed)
    Q_PROPERTY(int humidity READ humidity NOTIFY changed)
    Q_PROPERTY(int windSpeed READ windSpeed NOTIFY changed)
    Q_PROPERTY(QString windUnit READ windUnit NOTIFY changed)
    // [{time ("14:00"), temp, icon, text}]
    Q_PROPERTY(QVariantList hourly READ hourly NOTIFY changed)
    // [{day ("Today", "Thu"), min, max, icon, text}]
    Q_PROPERTY(QVariantList daily READ daily NOTIFY changed)
    Q_PROPERTY(QDateTime updated READ updated NOTIFY changed)

public:
    explicit Weather(QObject* parent = nullptr);

    // cache: file with the last answer; empty = the user's cache directory.
    void setCachePath(const QString& path) { m_cachePath = path; }
    void setFetcher(vela::weather::Fetcher fetcher) { m_fetcher = std::move(fetcher); }
    void setConfig(bool enabled, const QString& city, const QString& lat, const QString& lon, const QString& unit);
    Q_INVOKABLE void refresh();

    bool shown() const { return m_enabled && m_placed; }
    bool available() const { return m_forecast.valid; }
    bool stale() const { return m_stale; }
    QString city() const { return m_city; }
    QString unit() const { return m_fahrenheit ? QStringLiteral("f") : QStringLiteral("c"); }
    QString icon() const;
    QString text() const;
    int temperature() const { return display(m_forecast.temp); }
    int feelsLike() const { return display(m_forecast.feels); }
    int humidity() const { return m_forecast.humidity; }
    int windSpeed() const;
    QString windUnit() const { return m_fahrenheit ? QStringLiteral("mph") : QStringLiteral("km/h"); }
    QVariantList hourly() const;
    QVariantList daily() const;
    QDateTime updated() const { return m_updated; }

signals:
    void changed();

private:
    int display(double celsius) const;
    void loadCache();
    void fetched(const QByteArray& body);
    // No city chosen: the place is looked up once from the IP address.
    void locate();
    void apply(bool enabled, const QString& city, const QString& lat, const QString& lon, const QString& unit);

    bool m_enabled = false, m_placed = false, m_fahrenheit = false, m_stale = false;
    double m_lat = 0, m_lon = 0;
    QString m_city, m_cachePath;
    // What the config said, and the place found from the IP when it had none.
    bool m_cfgEnabled = false, m_located = false, m_locating = false;
    QString m_cfgCity, m_cfgLat, m_cfgLon, m_cfgUnit;
    double m_ipLat = 0, m_ipLon = 0;
    QString m_ipCity;
    QDateTime m_updated;
    vela::weather::Forecast m_forecast;
    vela::weather::Fetcher m_fetcher;
    QNetworkAccessManager* m_manager = nullptr;
    QTimer m_timer; // every 30 minutes, only while shown
    quint64 m_generation = 0; // answers for an old place are dropped
};

// The city search of Settings (geocoding).
class PlaceSearch : public QObject {
    Q_OBJECT
    // [{name, detail, lat, lon}]
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    // The last search failed (no network): different from "no results".
    Q_PROPERTY(bool failed READ failed NOTIFY changed)

public:
    explicit PlaceSearch(QObject* parent = nullptr) : QObject(parent) {}
    void setFetcher(vela::weather::Fetcher fetcher) { m_fetcher = std::move(fetcher); }
    Q_INVOKABLE void search(const QString& query);

    QVariantList results() const { return m_results; }
    bool busy() const { return m_busy; }
    bool failed() const { return m_failed; }

signals:
    void changed();

private:
    QVariantList m_results;
    bool m_busy = false, m_failed = false;
    vela::weather::Fetcher m_fetcher;
    QNetworkAccessManager* m_manager = nullptr;
    quint64 m_generation = 0;
};
