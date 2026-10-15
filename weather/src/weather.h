// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What vela-weather knows about Open-Meteo's answers, without any network or
// UI: the WMO tables, the JSON parsers, unit conversions, the saved places and
// the cache of the last good forecast. Stored values are always metric
// (°C, km/h, mm, hPa); they're converted when they are shown.
//
// shortcut: the shell's taskbar widget (shell/src/weather.cpp, topic/weather)
// has its own copy of the WMO table and parser; unify the two once both
// branches are merged.

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QLocale>
#include <QString>
#include <QUrl>

#include <optional>
#include <vector>

namespace vela::weather {

// --- WMO weather codes ---
enum class Sky { Clear, Cloudy, Rain, Snow, Storm, Fog };

// Every code Open-Meteo documents; anything else gets "unknown".
bool knownCode(int code);
QString iconName(int code, bool day); // freedesktop weather-* name
QString description(int code); // English source, translated at runtime
Sky skyOf(int code);

// --- units ---
enum class TemperatureUnit { Celsius, Fahrenheit };
enum class WindUnit { KmH, Mph, Ms, Knots };
enum class PrecipitationUnit { Mm, Inch };
enum class PressureUnit { HPa, InHg };

struct Units {
    TemperatureUnit temperature = TemperatureUnit::Celsius;
    WindUnit wind = WindUnit::KmH;
    PrecipitationUnit precipitation = PrecipitationUnit::Mm;
    PressureUnit pressure = PressureUnit::HPa;
    bool hour24 = true;
};

double convertTemperature(double celsius, TemperatureUnit unit);
double convertWind(double kmh, WindUnit unit);
double convertPrecipitation(double mm, PrecipitationUnit unit);
double convertPressure(double hpa, PressureUnit unit);

// Number and unit, ready to show: "16°", "34 km/h", "0.1 mm", "1013 hPa".
QString formatTemperature(double celsius, const Units& units, const QLocale& locale = QLocale());
QString formatWind(double kmh, const Units& units, const QLocale& locale = QLocale());
QString formatPrecipitation(double mm, const Units& units, const QLocale& locale = QLocale());
QString formatPressure(double hpa, const Units& units, const QLocale& locale = QLocale());
// "2026-10-15T07:15" (place-local, no zone) as "07:15" or "7:15 AM".
QString formatClock(const QString& isoLocal, bool hour24, const QLocale& locale = QLocale());
QString compass(double degrees); // "N", "NE", ...

// Index (0-5) of the European AQI band, and the UV index class (0-4).
int aqiBand(double aqi);
int uvClass(double uv);

// --- forecast ---
struct Current {
    QString time; // "2026-10-15T05:45", local to the place
    double temp = 0, feels = 0, wind = 0, windDir = 0, pressure = 0, visibility = 0; // visibility in m
    double uv = 0, dew = 0, precipitation = 0;
    int humidity = 0, cloud = 0, code = 0;
    bool day = true;
};
struct Hour {
    QString time;
    double temp = 0;
    int code = 0;
    int precipChance = -1; // percent; -1 when the answer has none
    bool day = true;
};
struct Day {
    QString date;
    double min = 0, max = 0;
    int code = 0;
    int precipChance = -1;
    QString sunrise, sunset;
};
struct Forecast {
    bool valid = false;
    QString timezone; // IANA, as the API resolved it
    Current current;
    std::vector<Hour> hours; // the next 24, from the current hour
    std::vector<Day> days; // today and the following days
};
struct AirQuality {
    bool valid = false;
    int aqi = 0; // European AQI
    double pm25 = 0, pm10 = 0;
};

QUrl forecastUrl(double lat, double lon);
QUrl airQualityUrl(double lat, double lon);
QUrl searchUrl(const QString& query, const QString& language);
Forecast parseForecast(const QJsonObject& root);
AirQuality parseAirQuality(const QJsonObject& root);

// Fraction of the daylight that has passed at `now`: below 0 before the
// sunrise, above 1 after the sunset. All three are place-local ISO times.
double sunProgress(const QString& now, const QString& sunrise, const QString& sunset);

// --- places ---
struct Place {
    QString name, region, country, timezone;
    double lat = 0, lon = 0;
    QString detail() const; // "Region, Country" without repeats
};

// The geocoding answer: no duplicates, no entries without coordinates.
std::vector<Place> parsePlaces(const QByteArray& body);
// The taskbar widget's saved place (taskbar/weather-*), if any.
std::optional<Place> placeFromShellConfig(const QString& path);

// ~/.local/share/Vela/weather/places.json: {"default": 0, "places": [...]}.
class PlacesStore {
public:
    enum class Load { Ok, Missing, Corrupt };

    explicit PlacesStore(QString path)
        : m_path(std::move(path))
    {
    }
    // A corrupt file is renamed to <path>.corrupt and the store starts empty.
    Load load();
    bool save() const;
    // False for a place already there (same name, within 0.05 degrees).
    bool add(const Place& place);
    void remove(int index);
    void setDefault(int index);

    const std::vector<Place>& places() const { return m_places; }
    int defaultIndex() const { return m_default; }
    Place& at(int index) { return m_places[index]; }

private:
    QString m_path;
    std::vector<Place> m_places;
    int m_default = 0;
};

// --- cache of the last good answer, one file per place ---
struct CacheEntry {
    QDateTime updated;
    QJsonObject forecast, air;
};
QString cacheFile(const QString& dir, double lat, double lon);
bool writeCache(const QString& path, const CacheEntry& entry);
std::optional<CacheEntry> readCache(const QString& path);
// Older than the refresh interval: shown with the stale marker.
constexpr int kRefreshMinutes = 15;
bool isStale(const QDateTime& updated, const QDateTime& now);

} // namespace vela::weather
