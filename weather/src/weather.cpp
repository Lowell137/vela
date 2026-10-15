// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weather.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QUrlQuery>

#include <cmath>

namespace vela::weather {

namespace {

constexpr int kHours = 24;

struct Wmo {
    int code;
    const char* day; // icon names
    const char* night;
    const char* text;
    Sky sky;
};

// Open-Meteo documents exactly these codes. The icons are the ones the
// freedesktop theme has for sure; the text is translated in i18n/.
const Wmo kWmo[] = {
    { 0, "weather-clear", "weather-clear-night", QT_TRANSLATE_NOOP("Weather", "Clear sky"), Sky::Clear },
    { 1, "weather-few-clouds", "weather-few-clouds-night", QT_TRANSLATE_NOOP("Weather", "Mostly clear"), Sky::Clear },
    { 2, "weather-few-clouds", "weather-few-clouds-night", QT_TRANSLATE_NOOP("Weather", "Partly cloudy"), Sky::Cloudy },
    { 3, "weather-overcast", "weather-overcast", QT_TRANSLATE_NOOP("Weather", "Overcast"), Sky::Cloudy },
    { 45, "weather-fog", "weather-fog", QT_TRANSLATE_NOOP("Weather", "Fog"), Sky::Fog },
    { 48, "weather-fog", "weather-fog", QT_TRANSLATE_NOOP("Weather", "Rime fog"), Sky::Fog },
    { 51, "weather-showers-scattered", "weather-showers-scattered", QT_TRANSLATE_NOOP("Weather", "Light drizzle"), Sky::Rain },
    { 53, "weather-showers-scattered", "weather-showers-scattered", QT_TRANSLATE_NOOP("Weather", "Drizzle"), Sky::Rain },
    { 55, "weather-showers-scattered", "weather-showers-scattered", QT_TRANSLATE_NOOP("Weather", "Heavy drizzle"), Sky::Rain },
    { 56, "weather-freezing-rain", "weather-freezing-rain", QT_TRANSLATE_NOOP("Weather", "Light freezing drizzle"), Sky::Rain },
    { 57, "weather-freezing-rain", "weather-freezing-rain", QT_TRANSLATE_NOOP("Weather", "Freezing drizzle"), Sky::Rain },
    { 61, "weather-showers-scattered", "weather-showers-scattered", QT_TRANSLATE_NOOP("Weather", "Light rain"), Sky::Rain },
    { 63, "weather-showers", "weather-showers", QT_TRANSLATE_NOOP("Weather", "Rain"), Sky::Rain },
    { 65, "weather-showers", "weather-showers", QT_TRANSLATE_NOOP("Weather", "Heavy rain"), Sky::Rain },
    { 66, "weather-freezing-rain", "weather-freezing-rain", QT_TRANSLATE_NOOP("Weather", "Light freezing rain"), Sky::Rain },
    { 67, "weather-freezing-rain", "weather-freezing-rain", QT_TRANSLATE_NOOP("Weather", "Freezing rain"), Sky::Rain },
    { 71, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Light snow"), Sky::Snow },
    { 73, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Snow"), Sky::Snow },
    { 75, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Heavy snow"), Sky::Snow },
    { 77, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Snow grains"), Sky::Snow },
    { 80, "weather-showers-scattered", "weather-showers-scattered", QT_TRANSLATE_NOOP("Weather", "Light rain showers"), Sky::Rain },
    { 81, "weather-showers", "weather-showers", QT_TRANSLATE_NOOP("Weather", "Rain showers"), Sky::Rain },
    { 82, "weather-showers", "weather-showers", QT_TRANSLATE_NOOP("Weather", "Violent rain showers"), Sky::Rain },
    { 85, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Light snow showers"), Sky::Snow },
    { 86, "weather-snow", "weather-snow", QT_TRANSLATE_NOOP("Weather", "Heavy snow showers"), Sky::Snow },
    { 95, "weather-storm", "weather-storm", QT_TRANSLATE_NOOP("Weather", "Thunderstorm"), Sky::Storm },
    { 96, "weather-storm", "weather-storm", QT_TRANSLATE_NOOP("Weather", "Thunderstorm with light hail"), Sky::Storm },
    { 99, "weather-storm", "weather-storm", QT_TRANSLATE_NOOP("Weather", "Thunderstorm with heavy hail"), Sky::Storm },
};

const Wmo* find(int code)
{
    for (const Wmo& w : kWmo) {
        if (w.code == code) {
            return &w;
        }
    }
    return nullptr;
}

QString number(double value, int decimals, QLocale locale)
{
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    // "-0" is never shown.
    const double scale = std::pow(10.0, decimals);
    double rounded = std::round(value * scale) / scale;
    if (rounded == 0) {
        rounded = 0;
    }
    return locale.toString(rounded, 'f', decimals);
}

QJsonArray array(const QJsonObject& o, const char* key) { return o.value(QLatin1String(key)).toArray(); }

// "2026-10-15T07:15" as minutes-capable time, zone-free (no DST gaps).
QDateTime localTime(const QString& iso) { return QDateTime::fromString(iso, QStringLiteral("yyyy-MM-ddTHH:mm")); }

} // namespace

// --- tables ---
bool knownCode(int code) { return find(code) != nullptr; }

QString iconName(int code, bool day)
{
    const Wmo* w = find(code);
    if (!w) {
        return QStringLiteral("weather-none-available");
    }
    return QString::fromLatin1(day ? w->day : w->night);
}

QString description(int code)
{
    const Wmo* w = find(code);
    return QCoreApplication::translate("Weather", w ? w->text : QT_TRANSLATE_NOOP("Weather", "Unknown"));
}

Sky skyOf(int code)
{
    const Wmo* w = find(code);
    return w ? w->sky : Sky::Cloudy;
}

// --- units ---
double convertTemperature(double c, TemperatureUnit u) { return u == TemperatureUnit::Fahrenheit ? c * 9 / 5 + 32 : c; }

double convertWind(double kmh, WindUnit u)
{
    switch (u) {
    case WindUnit::Mph: return kmh * 0.621371;
    case WindUnit::Ms: return kmh / 3.6;
    case WindUnit::Knots: return kmh * 0.539957;
    case WindUnit::KmH: break;
    }
    return kmh;
}

double convertPrecipitation(double mm, PrecipitationUnit u) { return u == PrecipitationUnit::Inch ? mm / 25.4 : mm; }
double convertPressure(double hpa, PressureUnit u) { return u == PressureUnit::InHg ? hpa * 0.0295300 : hpa; }

QString formatTemperature(double c, const Units& u, const QLocale& l)
{
    return number(convertTemperature(c, u.temperature), 0, l) + QChar(0x00B0);
}

QString formatWind(double kmh, const Units& u, const QLocale& l)
{
    const double v = convertWind(kmh, u.wind);
    switch (u.wind) {
    case WindUnit::Mph: return number(v, 0, l) + QStringLiteral(" mph");
    case WindUnit::Ms: return number(v, 1, l) + QStringLiteral(" m/s");
    case WindUnit::Knots: return number(v, 0, l) + QStringLiteral(" kn");
    case WindUnit::KmH: break;
    }
    return number(v, 0, l) + QStringLiteral(" km/h");
}

QString formatPrecipitation(double mm, const Units& u, const QLocale& l)
{
    if (u.precipitation == PrecipitationUnit::Inch) {
        return number(convertPrecipitation(mm, u.precipitation), 2, l) + QStringLiteral(" in");
    }
    return number(mm, 1, l) + QStringLiteral(" mm");
}

QString formatPressure(double hpa, const Units& u, const QLocale& l)
{
    if (u.pressure == PressureUnit::InHg) {
        return number(convertPressure(hpa, u.pressure), 2, l) + QStringLiteral(" inHg");
    }
    return number(hpa, 0, l) + QStringLiteral(" hPa");
}

QString formatClock(const QString& iso, bool hour24, const QLocale& locale)
{
    const QTime t = QTime::fromString(iso.mid(11, 5), QStringLiteral("HH:mm"));
    if (!t.isValid()) {
        return {};
    }
    return hour24 ? t.toString(QStringLiteral("HH:mm")) : locale.toString(t, QStringLiteral("h:mm AP"));
}

QString compass(double degrees)
{
    static const char* const points[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    const int i = int(std::floor((std::fmod(std::fmod(degrees, 360) + 360, 360) + 22.5) / 45)) % 8;
    return QString::fromLatin1(points[i]);
}

int aqiBand(double aqi)
{
    // European Environment Agency bands.
    static const double limits[] = { 20, 40, 60, 80, 100 };
    int band = 0;
    while (band < 5 && aqi > limits[band]) {
        ++band;
    }
    return band;
}

int uvClass(double uv) { return uv < 3 ? 0 : uv < 6 ? 1 : uv < 8 ? 2 : uv < 11 ? 3 : 4; }

// --- requests and parsers ---
QUrl forecastUrl(double lat, double lon)
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(lat, 'f', 4));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(lon, 'f', 4));
    q.addQueryItem(QStringLiteral("current"),
        QStringLiteral("temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,"
                       "wind_direction_10m,surface_pressure,cloud_cover,precipitation,is_day,uv_index,dew_point_2m,visibility"));
    q.addQueryItem(QStringLiteral("hourly"), QStringLiteral("temperature_2m,weather_code,is_day,precipitation_probability"));
    q.addQueryItem(QStringLiteral("daily"),
        QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max"));
    q.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(q);
    return url;
}

QUrl airQualityUrl(double lat, double lon)
{
    QUrl url(QStringLiteral("https://air-quality-api.open-meteo.com/v1/air-quality"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(lat, 'f', 4));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(lon, 'f', 4));
    q.addQueryItem(QStringLiteral("current"), QStringLiteral("european_aqi,pm10,pm2_5"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(q);
    return url;
}

QUrl searchUrl(const QString& text, const QString& language)
{
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("name"), text);
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("language"), language);
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(q);
    return url;
}

Forecast parseForecast(const QJsonObject& root)
{
    Forecast f;
    const QJsonObject cur = root.value(QLatin1String("current")).toObject();
    if (!cur.contains(QLatin1String("temperature_2m")) || !cur.contains(QLatin1String("weather_code"))
        || !cur.contains(QLatin1String("time"))) {
        return f;
    }
    auto num = [&cur](const char* key, double fallback = 0) { return cur.value(QLatin1String(key)).toDouble(fallback); };
    Current& c = f.current;
    c.time = cur.value(QLatin1String("time")).toString();
    c.temp = num("temperature_2m");
    c.feels = num("apparent_temperature", c.temp);
    c.humidity = qRound(num("relative_humidity_2m"));
    c.code = cur.value(QLatin1String("weather_code")).toInt();
    c.wind = num("wind_speed_10m");
    c.windDir = num("wind_direction_10m");
    c.pressure = num("surface_pressure", -1);
    c.cloud = qRound(num("cloud_cover"));
    c.precipitation = num("precipitation");
    c.uv = num("uv_index");
    c.dew = num("dew_point_2m");
    c.visibility = num("visibility", -1);
    c.day = cur.value(QLatin1String("is_day")).toInt(1) != 0;
    f.timezone = root.value(QLatin1String("timezone")).toString();

    const QJsonObject hourly = root.value(QLatin1String("hourly")).toObject();
    const QJsonArray times = array(hourly, "time"), temps = array(hourly, "temperature_2m"), codes = array(hourly, "weather_code"),
                     days = array(hourly, "is_day"), rain = array(hourly, "precipitation_probability");
    const int n = std::min({ int(times.size()), int(temps.size()), int(codes.size()) });
    for (int i = 0; i < n && int(f.hours.size()) < kHours; ++i) {
        const QString t = times[i].toString();
        // Both are "yyyy-MM-ddTHH:mm" of the place: they compare as text.
        if (t.left(13) < c.time.left(13)) {
            continue;
        }
        f.hours.push_back({ t, temps[i].toDouble(), codes[i].toInt(), i < rain.size() && !rain[i].isNull() ? rain[i].toInt() : -1,
            i >= days.size() || days[i].toInt(1) != 0 });
    }

    const QJsonObject daily = root.value(QLatin1String("daily")).toObject();
    const QJsonArray dates = array(daily, "time"), dmax = array(daily, "temperature_2m_max"), dmin = array(daily, "temperature_2m_min"),
                     dcode = array(daily, "weather_code"), rise = array(daily, "sunrise"), set = array(daily, "sunset"),
                     drain = array(daily, "precipitation_probability_max");
    const int d = std::min({ int(dates.size()), int(dmax.size()), int(dmin.size()), int(dcode.size()) });
    for (int i = 0; i < d; ++i) {
        f.days.push_back({ dates[i].toString(), dmin[i].toDouble(), dmax[i].toDouble(), dcode[i].toInt(),
            i < drain.size() && !drain[i].isNull() ? drain[i].toInt() : -1, i < rise.size() ? rise[i].toString() : QString(),
            i < set.size() ? set[i].toString() : QString() });
    }
    f.valid = !f.days.empty();
    return f;
}

AirQuality parseAirQuality(const QJsonObject& root)
{
    AirQuality a;
    const QJsonObject cur = root.value(QLatin1String("current")).toObject();
    // A null AQI (no station data) is "not available", not a zero.
    if (cur.value(QLatin1String("european_aqi")).isNull() || !cur.contains(QLatin1String("european_aqi"))) {
        return a;
    }
    a.aqi = qRound(cur.value(QLatin1String("european_aqi")).toDouble());
    a.pm25 = cur.value(QLatin1String("pm2_5")).toDouble();
    a.pm10 = cur.value(QLatin1String("pm10")).toDouble();
    a.valid = true;
    return a;
}

double sunProgress(const QString& now, const QString& sunrise, const QString& sunset)
{
    const QDateTime n = localTime(now), r = localTime(sunrise), s = localTime(sunset);
    if (!n.isValid() || !r.isValid() || !s.isValid() || s <= r) {
        return -1;
    }
    return double(r.secsTo(n)) / double(r.secsTo(s));
}

// --- places ---
QString Place::detail() const
{
    QStringList parts;
    for (const QString& part : { region, country }) {
        if (!part.isEmpty() && !parts.contains(part)) {
            parts << part;
        }
    }
    return parts.join(QStringLiteral(", "));
}

namespace {
bool sameSpot(const Place& a, const Place& b)
{
    return a.name.compare(b.name, Qt::CaseInsensitive) == 0 && std::abs(a.lat - b.lat) < 0.05 && std::abs(a.lon - b.lon) < 0.05;
}
bool validSpot(double lat, double lon) { return std::abs(lat) <= 90 && std::abs(lon) <= 180; }

QJsonObject toJson(const Place& p)
{
    return { { QStringLiteral("name"), p.name }, { QStringLiteral("region"), p.region }, { QStringLiteral("country"), p.country },
        { QStringLiteral("lat"), p.lat }, { QStringLiteral("lon"), p.lon }, { QStringLiteral("timezone"), p.timezone } };
}
} // namespace

std::vector<Place> parsePlaces(const QByteArray& body)
{
    std::vector<Place> out;
    const QJsonArray results = QJsonDocument::fromJson(body).object().value(QLatin1String("results")).toArray();
    for (const QJsonValue& value : results) {
        const QJsonObject r = value.toObject();
        Place p;
        p.name = r.value(QLatin1String("name")).toString();
        p.region = r.value(QLatin1String("admin1")).toString();
        p.country = r.value(QLatin1String("country")).toString();
        p.timezone = r.value(QLatin1String("timezone")).toString();
        if (p.name.isEmpty() || !r.value(QLatin1String("latitude")).isDouble() || !r.value(QLatin1String("longitude")).isDouble()) {
            continue;
        }
        p.lat = r.value(QLatin1String("latitude")).toDouble();
        p.lon = r.value(QLatin1String("longitude")).toDouble();
        if (!validSpot(p.lat, p.lon)) {
            continue;
        }
        bool dup = false;
        for (const Place& q : out) {
            dup = dup || (sameSpot(p, q) && p.region == q.region);
        }
        if (!dup) {
            out.push_back(p);
        }
    }
    return out;
}

std::optional<Place> placeFromShellConfig(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return std::nullopt;
    }
    const QSettings s(path, QSettings::IniFormat);
    bool latOk = false, lonOk = false;
    Place p;
    p.lat = s.value(QStringLiteral("taskbar/weather-lat")).toString().toDouble(&latOk);
    p.lon = s.value(QStringLiteral("taskbar/weather-lon")).toString().toDouble(&lonOk);
    p.name = s.value(QStringLiteral("taskbar/weather-city")).toString();
    if (!latOk || !lonOk || !validSpot(p.lat, p.lon) || p.name.isEmpty()) {
        return std::nullopt;
    }
    return p;
}

PlacesStore::Load PlacesStore::load()
{
    m_places.clear();
    m_default = 0;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Load::Missing;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError || !doc.isObject() || !doc.object().value(QLatin1String("places")).isArray()) {
        QFile::remove(m_path + QStringLiteral(".corrupt"));
        QFile::rename(m_path, m_path + QStringLiteral(".corrupt"));
        return Load::Corrupt;
    }
    for (const QJsonValue& v : doc.object().value(QLatin1String("places")).toArray()) {
        const QJsonObject o = v.toObject();
        Place p;
        p.name = o.value(QLatin1String("name")).toString();
        p.region = o.value(QLatin1String("region")).toString();
        p.country = o.value(QLatin1String("country")).toString();
        p.timezone = o.value(QLatin1String("timezone")).toString();
        p.lat = o.value(QLatin1String("lat")).toDouble(1000);
        p.lon = o.value(QLatin1String("lon")).toDouble(1000);
        if (!p.name.isEmpty() && validSpot(p.lat, p.lon)) {
            m_places.push_back(p);
        }
    }
    const int d = doc.object().value(QLatin1String("default")).toInt();
    m_default = d >= 0 && d < int(m_places.size()) ? d : 0;
    return Load::Ok;
}

bool PlacesStore::save() const
{
    QJsonArray places;
    for (const Place& p : m_places) {
        places.append(toJson(p));
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject { { QStringLiteral("default"), m_default }, { QStringLiteral("places"), places } }).toJson());
    return file.commit();
}

bool PlacesStore::add(const Place& place)
{
    for (const Place& p : m_places) {
        if (sameSpot(p, place)) {
            return false;
        }
    }
    m_places.push_back(place);
    return true;
}

void PlacesStore::remove(int index)
{
    if (index < 0 || index >= int(m_places.size())) {
        return;
    }
    m_places.erase(m_places.begin() + index);
    if (m_default > index || m_default >= int(m_places.size())) {
        m_default = std::max(0, m_default - 1);
    }
}

void PlacesStore::setDefault(int index)
{
    if (index >= 0 && index < int(m_places.size())) {
        m_default = index;
    }
}

// --- cache ---
QString cacheFile(const QString& dir, double lat, double lon)
{
    return dir + QStringLiteral("/%1_%2.json").arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3);
}

bool writeCache(const QString& path, const CacheEntry& e)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject { { QStringLiteral("updated"), e.updated.toUTC().toString(Qt::ISODate) },
                                 { QStringLiteral("forecast"), e.forecast }, { QStringLiteral("air"), e.air } })
                   .toJson(QJsonDocument::Compact));
    return file.commit();
}

std::optional<CacheEntry> readCache(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    CacheEntry e;
    e.updated = QDateTime::fromString(o.value(QLatin1String("updated")).toString(), Qt::ISODate);
    e.forecast = o.value(QLatin1String("forecast")).toObject();
    e.air = o.value(QLatin1String("air")).toObject();
    if (!e.updated.isValid() || e.forecast.isEmpty()) {
        return std::nullopt;
    }
    return e;
}

bool isStale(const QDateTime& updated, const QDateTime& now)
{
    // A clock that went back counts as stale too.
    return !updated.isValid() || updated > now || updated.secsTo(now) >= kRefreshMinutes * 60;
}

} // namespace vela::weather
