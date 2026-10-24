// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weather.h"

#include "../../common/privatefile.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>

#include <cmath>

namespace vela::weather {

namespace {
QString tr(const char* text) { return QCoreApplication::translate("Weather", text); }
constexpr int kRefreshMs = 30 * 60 * 1000;
constexpr int kHours = 12;
} // namespace

Fetcher networkFetcher(QNetworkAccessManager* manager)
{
    return [manager](const QUrl& url, const Callback& done) {
        QNetworkRequest request(url);
        request.setTransferTimeout(15000);
        QNetworkReply* reply = manager->get(request);
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done] {
            done(reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray());
            reply->deleteLater();
        });
    };
}

QString iconName(int code, bool day)
{
    switch (code) {
    case 0: return day ? QStringLiteral("weather-clear") : QStringLiteral("weather-clear-night");
    case 1:
    case 2: return day ? QStringLiteral("weather-few-clouds") : QStringLiteral("weather-few-clouds-night");
    case 3: return QStringLiteral("weather-overcast");
    case 45:
    case 48: return QStringLiteral("weather-fog");
    case 51: case 53: case 55: case 61: case 80: return QStringLiteral("weather-showers-scattered");
    case 56: case 57: case 66: case 67: return QStringLiteral("weather-freezing-rain");
    case 63: case 65: case 81: case 82: return QStringLiteral("weather-showers");
    case 71: case 73: case 75: case 77: case 85: case 86: return QStringLiteral("weather-snow");
    case 95: case 96: case 99: return QStringLiteral("weather-storm");
    default: return QStringLiteral("weather-none-available");
    }
}

QString description(int code)
{
    switch (code) {
    case 0: return tr("Clear");
    case 1: return tr("Mostly clear");
    case 2: return tr("Partly cloudy");
    case 3: return tr("Overcast");
    case 45: case 48: return tr("Fog");
    case 51: case 53: case 55: return tr("Drizzle");
    case 56: case 57: return tr("Freezing drizzle");
    case 61: return tr("Light rain");
    case 63: return tr("Rain");
    case 65: return tr("Heavy rain");
    case 66: case 67: return tr("Freezing rain");
    case 71: return tr("Light snow");
    case 73: return tr("Snow");
    case 75: return tr("Heavy snow");
    case 77: return tr("Snow grains");
    case 80: case 81: case 82: return tr("Rain showers");
    case 85: case 86: return tr("Snow showers");
    case 95: return tr("Thunderstorm");
    case 96: case 99: return tr("Thunderstorm with hail");
    default: return tr("Unknown");
    }
}

QUrl forecastUrl(double lat, double lon)
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(lon, 'f', 4));
    query.addQueryItem(QStringLiteral("current"),
        QStringLiteral("temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"));
    query.addQueryItem(QStringLiteral("hourly"), QStringLiteral("temperature_2m,weather_code,is_day"));
    query.addQueryItem(QStringLiteral("daily"), QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min"));
    query.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("5"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);
    return url;
}

QUrl searchUrl(const QString& text, const QString& language)
{
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), text);
    query.addQueryItem(QStringLiteral("count"), QStringLiteral("8"));
    query.addQueryItem(QStringLiteral("language"), language);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}

Forecast parseForecast(const QJsonObject& root)
{
    Forecast f;
    const QJsonObject current = root.value(QLatin1String("current")).toObject();
    const QJsonObject hourly = root.value(QLatin1String("hourly")).toObject();
    const QJsonObject daily = root.value(QLatin1String("daily")).toObject();
    if (!current.contains(QLatin1String("temperature_2m")) || !current.contains(QLatin1String("weather_code"))) {
        return f;
    }
    f.temp = current.value(QLatin1String("temperature_2m")).toDouble();
    f.feels = current.value(QLatin1String("apparent_temperature")).toDouble(f.temp);
    f.humidity = qRound(current.value(QLatin1String("relative_humidity_2m")).toDouble());
    f.wind = current.value(QLatin1String("wind_speed_10m")).toDouble();
    f.code = current.value(QLatin1String("weather_code")).toInt();
    f.day = current.value(QLatin1String("is_day")).toInt(1) != 0;

    const QString now = current.value(QLatin1String("time")).toString();
    const QJsonArray times = hourly.value(QLatin1String("time")).toArray();
    const QJsonArray temps = hourly.value(QLatin1String("temperature_2m")).toArray();
    const QJsonArray codes = hourly.value(QLatin1String("weather_code")).toArray();
    const QJsonArray days = hourly.value(QLatin1String("is_day")).toArray();
    const int n = std::min({ int(times.size()), int(temps.size()), int(codes.size()) });
    for (int i = 0; i < n && int(f.hours.size()) < kHours; ++i) {
        const QString t = times[i].toString();
        // Both are "yyyy-MM-ddTHH:mm" in the place's time zone: they compare as text.
        if (t < now.left(13)) {
            continue;
        }
        f.hours.push_back({ t, temps[i].toDouble(), codes[i].toInt(), i >= days.size() || days[i].toInt(1) != 0 });
    }

    const QJsonArray dates = daily.value(QLatin1String("time")).toArray();
    const QJsonArray dmax = daily.value(QLatin1String("temperature_2m_max")).toArray();
    const QJsonArray dmin = daily.value(QLatin1String("temperature_2m_min")).toArray();
    const QJsonArray dcode = daily.value(QLatin1String("weather_code")).toArray();
    const int d = std::min({ int(dates.size()), int(dmax.size()), int(dmin.size()), int(dcode.size()) });
    for (int i = 0; i < d; ++i) {
        f.days.push_back({ dates[i].toString(), dmin[i].toDouble(), dmax[i].toDouble(), dcode[i].toInt() });
    }
    f.valid = true;
    return f;
}

QVariantList parsePlaces(const QByteArray& body)
{
    QVariantList out;
    const QJsonArray results = QJsonDocument::fromJson(body).object().value(QLatin1String("results")).toArray();
    for (const QJsonValue& value : results) {
        const QJsonObject r = value.toObject();
        if (!r.contains(QLatin1String("latitude")) || !r.contains(QLatin1String("longitude"))) {
            continue;
        }
        QStringList detail;
        for (const char* key : { "admin1", "country" }) {
            const QString part = r.value(QLatin1String(key)).toString();
            if (!part.isEmpty() && !detail.contains(part)) {
                detail << part;
            }
        }
        out.append(QVariantMap { { QStringLiteral("name"), r.value(QLatin1String("name")).toString() },
            { QStringLiteral("detail"), detail.join(QStringLiteral(", ")) },
            { QStringLiteral("lat"), QString::number(r.value(QLatin1String("latitude")).toDouble(), 'f', 4) },
            { QStringLiteral("lon"), QString::number(r.value(QLatin1String("longitude")).toDouble(), 'f', 4) } });
    }
    return out;
}

} // namespace vela::weather

using namespace vela::weather;

vela::weather::IpPlace vela::weather::parseIpPlace(const QByteArray& body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    vela::weather::IpPlace place;
    if (!root.value(QLatin1String("success")).toBool(false) || !root.contains(QLatin1String("latitude"))
        || !root.contains(QLatin1String("longitude"))) {
        return place;
    }
    place.lat = root.value(QLatin1String("latitude")).toDouble();
    place.lon = root.value(QLatin1String("longitude")).toDouble();
    place.city = root.value(QLatin1String("city")).toString();
    place.ok = std::abs(place.lat) <= 90 && std::abs(place.lon) <= 180;
    return place;
}

Weather::Weather(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, &Weather::refresh);
}

void Weather::setConfig(bool enabled, const QString& city, const QString& lat, const QString& lon, const QString& unit)
{
    m_cfgEnabled = enabled;
    m_cfgCity = city;
    m_cfgLat = lat;
    m_cfgLon = lon;
    m_cfgUnit = unit;
    bool latOk = false, lonOk = false;
    lat.toDouble(&latOk);
    lon.toDouble(&lonOk);
    if (enabled && !(latOk && lonOk)) {
        if (m_located) {
            // No place chosen: the one found from the IP address.
            apply(enabled, city.isEmpty() ? m_ipCity : city, QString::number(m_ipLat, 'f', 4), QString::number(m_ipLon, 'f', 4), unit);
            return;
        }
        locate();
        if (m_located) {
            return; // answered at once (a test's fetcher): already applied
        }
    }
    apply(enabled, city, lat, lon, unit);
}

void Weather::locate()
{
    if (m_locating) {
        return;
    }
    if (!m_fetcher) {
        if (!m_manager) {
            m_manager = new QNetworkAccessManager(this);
        }
        m_fetcher = networkFetcher(m_manager);
    }
    m_locating = true;
    m_fetcher(QUrl(QStringLiteral("https://ipwho.is/?fields=success,latitude,longitude,city")), [this](const QByteArray& body) {
        m_locating = false;
        const vela::weather::IpPlace place = vela::weather::parseIpPlace(body);
        if (!place.ok) {
            return; // offline: the widget stays hidden until the next config change
        }
        m_located = true;
        m_ipLat = place.lat;
        m_ipLon = place.lon;
        m_ipCity = place.city;
        setConfig(m_cfgEnabled, m_cfgCity, m_cfgLat, m_cfgLon, m_cfgUnit);
    });
}

void Weather::apply(bool enabled, const QString& city, const QString& lat, const QString& lon, const QString& unit)
{
    bool latOk = false, lonOk = false;
    const double la = lat.toDouble(&latOk), lo = lon.toDouble(&lonOk);
    const bool placed = latOk && lonOk && std::abs(la) <= 90 && std::abs(lo) <= 180;
    const bool fahrenheit = unit == QLatin1String("f");
    const bool samePlace = placed == m_placed && la == m_lat && lo == m_lon;
    const bool wasShown = shown();
    if (enabled == m_enabled && samePlace && city == m_city && fahrenheit == m_fahrenheit) {
        return;
    }
    m_enabled = enabled;
    m_placed = placed;
    m_city = city;
    m_fahrenheit = fahrenheit;
    if (!samePlace) {
        m_lat = la;
        m_lon = lo;
        m_forecast = {};
        m_stale = false;
        ++m_generation;
    }
    if (!shown()) {
        m_timer.stop();
        ++m_generation; // a pending answer is dropped
    } else if (!wasShown || !samePlace) {
        if (!m_forecast.valid) {
            loadCache();
        }
        m_timer.start();
        refresh();
    }
    emit changed();
}

void Weather::refresh()
{
    if (!shown()) {
        return; // off: no request at all
    }
    if (!m_fetcher) {
        if (!m_manager) {
            m_manager = new QNetworkAccessManager(this);
        }
        m_fetcher = networkFetcher(m_manager);
    }
    const quint64 generation = m_generation;
    m_fetcher(forecastUrl(m_lat, m_lon), [this, generation](const QByteArray& body) {
        if (generation == m_generation) {
            fetched(body);
        }
    });
}

void Weather::fetched(const QByteArray& body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const Forecast f = parseForecast(root);
    if (!f.valid) {
        m_stale = true; // keep showing the last value, marked old
        emit changed();
        return;
    }
    m_forecast = f;
    m_stale = false;
    m_updated = QDateTime::currentDateTime();

    QString path = m_cachePath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/weather.json");
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (vela::openPrivateFile(file)) {
        file.write(QJsonDocument(QJsonObject { { QStringLiteral("lat"), m_lat }, { QStringLiteral("lon"), m_lon },
                                     { QStringLiteral("updated"), m_updated.toString(Qt::ISODate) },
                                     { QStringLiteral("data"), root } })
                       .toJson(QJsonDocument::Compact));
        file.commit();
    }
    emit changed();
}

void Weather::loadCache()
{
    QString path = m_cachePath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/weather.json");
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject cache = QJsonDocument::fromJson(file.readAll()).object();
    if (cache.value(QLatin1String("lat")).toDouble() != m_lat || cache.value(QLatin1String("lon")).toDouble() != m_lon) {
        return; // another place
    }
    const Forecast f = parseForecast(cache.value(QLatin1String("data")).toObject());
    if (f.valid) {
        m_forecast = f;
        m_updated = QDateTime::fromString(cache.value(QLatin1String("updated")).toString(), Qt::ISODate);
        // Until the first refresh succeeds the values are the cache's.
        m_stale = true;
    }
}

int Weather::display(double celsius) const
{
    return qRound(m_fahrenheit ? celsius * 9 / 5 + 32 : celsius);
}

int Weather::windSpeed() const
{
    return qRound(m_fahrenheit ? m_forecast.wind * 0.621371 : m_forecast.wind);
}

QString Weather::icon() const { return iconName(m_forecast.code, m_forecast.day); }
QString Weather::text() const { return description(m_forecast.code); }

QVariantList Weather::hourly() const
{
    QVariantList out;
    for (const Hour& h : m_forecast.hours) {
        out.append(QVariantMap { { QStringLiteral("time"), h.time.mid(11, 5) }, { QStringLiteral("temp"), display(h.temp) },
            { QStringLiteral("icon"), iconName(h.code, h.day) }, { QStringLiteral("text"), description(h.code) } });
    }
    return out;
}

QVariantList Weather::daily() const
{
    QVariantList out;
    const QDate today = QDate::currentDate();
    for (const Day& d : m_forecast.days) {
        const QDate date = QDate::fromString(d.date, Qt::ISODate);
        out.append(QVariantMap { { QStringLiteral("day"), date == today ? tr("Today") : QLocale().toString(date, QStringLiteral("ddd")) },
            { QStringLiteral("min"), display(d.min) }, { QStringLiteral("max"), display(d.max) },
            { QStringLiteral("icon"), iconName(d.code, true) }, { QStringLiteral("text"), description(d.code) } });
    }
    return out;
}

void PlaceSearch::search(const QString& query)
{
    const QString text = query.trimmed();
    ++m_generation;
    m_failed = false;
    if (text.size() < 2) {
        m_results.clear();
        m_busy = false;
        emit changed();
        return;
    }
    if (!m_fetcher) {
        if (!m_manager) {
            m_manager = new QNetworkAccessManager(this);
        }
        m_fetcher = networkFetcher(m_manager);
    }
    m_busy = true;
    emit changed();
    const quint64 generation = m_generation;
    m_fetcher(searchUrl(text, QLocale().name().section(u'_', 0, 0)), [this, generation](const QByteArray& body) {
        if (generation != m_generation) {
            return;
        }
        m_busy = false;
        m_failed = body.isEmpty();
        m_results = parsePlaces(body);
        emit changed();
    });
}
