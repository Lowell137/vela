// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weathermodel.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

using namespace vela::weather;

namespace vela::weather {
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
} // namespace vela::weather

namespace {
// The setting values, in the order of the enums.
const QStringList kTemperature { QStringLiteral("c"), QStringLiteral("f") };
const QStringList kWind { QStringLiteral("kmh"), QStringLiteral("mph"), QStringLiteral("ms"), QStringLiteral("kn") };
const QStringList kPrecipitation { QStringLiteral("mm"), QStringLiteral("in") };
const QStringList kPressure { QStringLiteral("hpa"), QStringLiteral("inhg") };

template <typename E>
E pick(const QStringList& keys, const QString& key, E fallback)
{
    const int i = keys.indexOf(key);
    return i < 0 ? fallback : E(i);
}
} // namespace

WeatherModel::WeatherModel(const QString& placesPath, const QString& cacheDir, const QString& shellConfig, QSettings* settings,
    Fetcher fetcher, QObject* parent)
    : QObject(parent)
    , m_store(placesPath)
    , m_cacheDir(cacheDir)
    , m_settings(settings)
    , m_fetch(std::move(fetcher))
    , m_clock([] { return QDateTime::currentDateTime(); })
{
    if (m_settings) {
        m_units.temperature = pick(kTemperature, m_settings->value(QStringLiteral("units/temperature")).toString(), TemperatureUnit::Celsius);
        m_units.wind = pick(kWind, m_settings->value(QStringLiteral("units/wind")).toString(), WindUnit::KmH);
        m_units.precipitation = pick(kPrecipitation, m_settings->value(QStringLiteral("units/precipitation")).toString(), PrecipitationUnit::Mm);
        m_units.pressure = pick(kPressure, m_settings->value(QStringLiteral("units/pressure")).toString(), PressureUnit::HPa);
        m_units.hour24 = m_settings->value(QStringLiteral("units/hour24"), true).toBool();
    }

    const PlacesStore::Load loaded = m_store.load();
    m_corrupt = loaded == PlacesStore::Load::Corrupt;
    if (loaded == PlacesStore::Load::Missing && !shellConfig.isEmpty()) {
        // First run: the taskbar widget's place, when it has one.
        if (const auto place = placeFromShellConfig(shellConfig)) {
            m_store.add(*place);
            m_store.save();
        }
    }
    m_index = m_store.places().empty() ? -1 : m_store.defaultIndex();
    load();

    m_timer.setInterval(kRefreshMinutes * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &WeatherModel::refresh);
    m_timer.start();
}

Fetcher& WeatherModel::fetcher()
{
    if (!m_fetch) {
        if (!m_manager) {
            m_manager = new QNetworkAccessManager(this);
        }
        m_fetch = networkFetcher(m_manager);
    }
    return m_fetch;
}

const Place* WeatherModel::current() const
{
    return m_index >= 0 && m_index < int(m_store.places().size()) ? &m_store.places()[m_index] : nullptr;
}

// --- loading ---
void WeatherModel::load()
{
    ++m_generation;
    m_forecast = {};
    m_air = {};
    m_forecastJson = m_airJson = {};
    m_updated = {};
    m_failed = m_loading = false;
    if (const Place* p = current()) {
        if (const auto cached = readCache(cacheFile(m_cacheDir, p->lat, p->lon))) {
            const Forecast f = parseForecast(cached->forecast);
            if (f.valid) {
                m_forecast = f;
                m_forecastJson = cached->forecast;
                m_airJson = cached->air;
                m_air = parseAirQuality(cached->air);
                m_updated = cached->updated;
            }
        }
    }
}

void WeatherModel::saveCache()
{
    const Place* p = current();
    if (p && m_forecast.valid && !m_forecastJson.isEmpty() && m_updated.isValid()) {
        writeCache(cacheFile(m_cacheDir, p->lat, p->lon), { m_updated, m_forecastJson, m_airJson });
    }
}

void WeatherModel::refresh()
{
    const Place* p = current();
    if (!p) {
        return;
    }
    const quint64 generation = m_generation;
    m_loading = true;
    emit changed();
    fetcher()(forecastUrl(p->lat, p->lon), [this, generation](const QByteArray& body) { forecastArrived(generation, body); });
    // The air quality is optional: its answer never marks the page as failed.
    fetcher()(airQualityUrl(p->lat, p->lon), [this, generation](const QByteArray& body) { airArrived(generation, body); });
}

void WeatherModel::forecastArrived(quint64 generation, const QByteArray& body)
{
    if (generation != m_generation) {
        return;
    }
    m_loading = false;
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const Forecast f = parseForecast(root);
    if (!f.valid) {
        m_failed = true; // what's shown stays, marked old
        emit changed();
        return;
    }
    m_failed = false;
    m_forecast = f;
    m_forecastJson = root;
    m_updated = m_clock();
    if (m_index >= 0 && m_store.places()[m_index].timezone.isEmpty() && !f.timezone.isEmpty()) {
        m_store.at(m_index).timezone = f.timezone;
        m_store.save();
    }
    saveCache();
    emit changed();
}

void WeatherModel::airArrived(quint64 generation, const QByteArray& body)
{
    if (generation != m_generation) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const AirQuality a = parseAirQuality(root);
    if (!a.valid) {
        return; // keeps the previous value, if any
    }
    m_air = a;
    m_airJson = root;
    saveCache();
    emit changed();
}

// --- places ---
void WeatherModel::choose(int index)
{
    if (index < 0 || index >= int(m_store.places().size()) || index == m_index) {
        return;
    }
    m_index = index;
    load();
    emit placesChanged();
    emit changed();
    refresh();
}

QVariantList WeatherModel::places() const
{
    QVariantList out;
    int i = 0;
    for (const Place& p : m_store.places()) {
        out.append(QVariantMap { { QStringLiteral("name"), p.name }, { QStringLiteral("detail"), p.detail() },
            { QStringLiteral("lat"), p.lat }, { QStringLiteral("lon"), p.lon },
            { QStringLiteral("isDefault"), i == m_store.defaultIndex() }, { QStringLiteral("current"), i == m_index } });
        ++i;
    }
    return out;
}

bool WeatherModel::addResult(int index)
{
    if (index < 0 || index >= int(m_results.size())) {
        return false;
    }
    const Place place = m_results[index];
    int at = -1;
    for (int i = 0; i < int(m_store.places().size()); ++i) {
        const Place& p = m_store.places()[i];
        if (p.name.compare(place.name, Qt::CaseInsensitive) == 0 && std::abs(p.lat - place.lat) < 0.05 && std::abs(p.lon - place.lon) < 0.05) {
            at = i;
        }
    }
    if (at < 0) {
        m_store.add(place);
        m_store.save();
        at = int(m_store.places().size()) - 1;
    }
    m_index = -1; // a changed place even when it's the same slot
    emit placesChanged();
    choose(at);
    clearSearch();
    return true;
}

void WeatherModel::remove(int index)
{
    if (index < 0 || index >= int(m_store.places().size())) {
        return;
    }
    const bool wasCurrent = index == m_index;
    m_store.remove(index);
    m_store.save();
    if (m_store.places().empty()) {
        m_index = -1;
        load();
    } else if (wasCurrent) {
        m_index = -1;
        emit placesChanged();
        choose(m_store.defaultIndex());
        return;
    } else if (index < m_index) {
        --m_index;
    }
    emit placesChanged();
    emit changed();
}

void WeatherModel::setDefault(int index)
{
    m_store.setDefault(index);
    m_store.save();
    emit placesChanged();
}

// --- search ---
void WeatherModel::search(const QString& text)
{
    const QString query = text.trimmed();
    const quint64 generation = ++m_searchGeneration;
    m_results.clear();
    m_searchFailed = false;
    m_searched = false;
    m_searching = query.size() >= 2;
    emit searchChanged();
    if (!m_searching) {
        return;
    }
    fetcher()(searchUrl(query, QLocale().name().section(QLatin1Char('_'), 0, 0)), [this, generation](const QByteArray& body) {
        if (generation != m_searchGeneration) {
            return;
        }
        m_searching = false;
        m_searched = true;
        m_searchFailed = body.isEmpty();
        m_results = parsePlaces(body);
        emit searchChanged();
    });
}

void WeatherModel::clearSearch()
{
    ++m_searchGeneration;
    m_results.clear();
    m_searching = m_searchFailed = m_searched = false;
    emit searchChanged();
}

QVariantList WeatherModel::searchResults() const
{
    QVariantList out;
    for (const Place& r : m_results) {
        bool saved = false;
        for (const Place& p : m_store.places()) {
            saved = saved || (p.name.compare(r.name, Qt::CaseInsensitive) == 0 && std::abs(p.lat - r.lat) < 0.05 && std::abs(p.lon - r.lon) < 0.05);
        }
        out.append(QVariantMap { { QStringLiteral("name"), r.name }, { QStringLiteral("detail"), r.detail() },
            { QStringLiteral("lat"), r.lat }, { QStringLiteral("lon"), r.lon }, { QStringLiteral("saved"), saved } });
    }
    return out;
}

// --- what's shown ---
bool WeatherModel::stale() const { return m_forecast.valid && (m_failed || isStale(m_updated, m_clock())); }

QString WeatherModel::updatedText() const
{
    if (!m_updated.isValid()) {
        return {};
    }
    const QDateTime t = m_updated.toLocalTime();
    const QString clock = m_units.hour24 ? t.time().toString(QStringLiteral("HH:mm")) : QLocale().toString(t.time(), QStringLiteral("h:mm AP"));
    if (t.date() == m_clock().date()) {
        return tr("Updated %1").arg(clock);
    }
    return tr("Updated %1, %2").arg(QLocale().toString(t.date(), QLocale::ShortFormat), clock);
}

QVariantMap WeatherModel::now() const
{
    const Place* p = current();
    if (!p || !m_forecast.valid) {
        return { { QStringLiteral("place"), p ? p->name : QString() } };
    }
    const Current& c = m_forecast.current;
    const Day& today = m_forecast.days.front();
    return { { QStringLiteral("place"), p->name }, { QStringLiteral("detail"), p->detail() },
        { QStringLiteral("temp"), formatTemperature(c.temp, m_units) }, { QStringLiteral("text"), description(c.code) },
        { QStringLiteral("icon"), iconName(c.code, c.day) },
        { QStringLiteral("feels"), tr("Feels like %1").arg(formatTemperature(c.feels, m_units)) },
        { QStringLiteral("high"), formatTemperature(today.max, m_units) }, { QStringLiteral("low"), formatTemperature(today.min, m_units) } };
}

QVariantList WeatherModel::hourly() const
{
    QVariantList out;
    bool first = true;
    for (const Hour& h : m_forecast.hours) {
        out.append(QVariantMap { { QStringLiteral("time"), first ? tr("Now") : formatClock(h.time, m_units.hour24) },
            { QStringLiteral("icon"), iconName(h.code, h.day) }, { QStringLiteral("text"), description(h.code) },
            { QStringLiteral("temp"), formatTemperature(h.temp, m_units) },
            { QStringLiteral("value"), qRound(convertTemperature(h.temp, m_units.temperature)) },
            { QStringLiteral("chance"), h.precipChance } });
        first = false;
    }
    return out;
}

QVariantList WeatherModel::daily() const
{
    QVariantList out;
    bool first = true;
    for (const Day& d : m_forecast.days) {
        const QDate date = QDate::fromString(d.date, Qt::ISODate);
        out.append(QVariantMap { { QStringLiteral("day"), first ? tr("Today") : QLocale().toString(date, QStringLiteral("ddd")) },
            { QStringLiteral("icon"), iconName(d.code, true) }, { QStringLiteral("text"), description(d.code) },
            { QStringLiteral("min"), formatTemperature(d.min, m_units) }, { QStringLiteral("max"), formatTemperature(d.max, m_units) },
            { QStringLiteral("minValue"), convertTemperature(d.min, m_units.temperature) },
            { QStringLiteral("maxValue"), convertTemperature(d.max, m_units.temperature) }, { QStringLiteral("chance"), d.precipChance } });
        first = false;
    }
    return out;
}

QVariantList WeatherModel::details() const
{
    QVariantList out;
    if (!m_forecast.valid) {
        return out;
    }
    const Current& c = m_forecast.current;
    const QLocale l;
    static const char* const uvText[] = { QT_TR_NOOP("Low"), QT_TR_NOOP("Moderate"), QT_TR_NOOP("High"), QT_TR_NOOP("Very high"),
        QT_TR_NOOP("Extreme") };
    auto add = [&out](const QString& label, const QString& value) {
        out.append(QVariantMap { { QStringLiteral("label"), label }, { QStringLiteral("value"), value } });
    };
    const QString none = QStringLiteral("—");
    add(tr("Wind"), formatWind(c.wind, m_units, l) + QLatin1Char(' ') + compass(c.windDir));
    add(tr("Humidity"), QStringLiteral("%1%").arg(c.humidity));
    add(tr("Pressure"), c.pressure > 0 ? formatPressure(c.pressure, m_units, l) : none);
    add(tr("Visibility"), c.visibility >= 0 ? l.toString(c.visibility / 1000, 'f', 1) + QStringLiteral(" km") : none);
    add(tr("UV index"), l.toString(c.uv, 'f', 0) + QStringLiteral(" · ") + tr(uvText[uvClass(c.uv)]));
    add(tr("Dew point"), formatTemperature(c.dew, m_units, l));
    add(tr("Cloud cover"), QStringLiteral("%1%").arg(c.cloud));
    add(tr("Precipitation"), formatPrecipitation(c.precipitation, m_units, l));
    return out;
}

QVariantMap WeatherModel::sun() const
{
    if (!m_forecast.valid || m_forecast.days.empty()) {
        return { { QStringLiteral("valid"), false } };
    }
    const Day& d = m_forecast.days.front();
    const double progress = sunProgress(m_forecast.current.time, d.sunrise, d.sunset);
    QString daylight;
    const QDateTime r = QDateTime::fromString(d.sunrise, QStringLiteral("yyyy-MM-ddTHH:mm")),
                    s = QDateTime::fromString(d.sunset, QStringLiteral("yyyy-MM-ddTHH:mm"));
    if (r.isValid() && s.isValid() && s > r) {
        const int minutes = int(r.secsTo(s) / 60);
        daylight = tr("%1 h %2 min of daylight").arg(minutes / 60).arg(minutes % 60);
    }
    return { { QStringLiteral("valid"), progress > -1 || !d.sunrise.isEmpty() }, { QStringLiteral("rise"), formatClock(d.sunrise, m_units.hour24) },
        { QStringLiteral("set"), formatClock(d.sunset, m_units.hour24) }, { QStringLiteral("progress"), progress },
        { QStringLiteral("daylight"), daylight } };
}

QVariantMap WeatherModel::air() const
{
    if (!m_air.valid) {
        return { { QStringLiteral("available"), false } };
    }
    static const char* const text[] = { QT_TR_NOOP("Good"), QT_TR_NOOP("Fair"), QT_TR_NOOP("Moderate"), QT_TR_NOOP("Poor"),
        QT_TR_NOOP("Very poor"), QT_TR_NOOP("Extremely poor") };
    const int band = aqiBand(m_air.aqi);
    const QLocale l;
    return { { QStringLiteral("available"), true }, { QStringLiteral("value"), m_air.aqi }, { QStringLiteral("band"), band },
        { QStringLiteral("text"), tr(text[band]) }, { QStringLiteral("pm25"), l.toString(m_air.pm25, 'f', 1) + QStringLiteral(" µg/m³") },
        { QStringLiteral("pm10"), l.toString(m_air.pm10, 'f', 1) + QStringLiteral(" µg/m³") } };
}

QString WeatherModel::sky() const
{
    if (!m_forecast.valid) {
        return QStringLiteral("cloudy");
    }
    switch (skyOf(m_forecast.current.code)) {
    case Sky::Clear: return QStringLiteral("clear");
    case Sky::Cloudy: return QStringLiteral("cloudy");
    case Sky::Rain: return QStringLiteral("rain");
    case Sky::Snow: return QStringLiteral("snow");
    case Sky::Storm: return QStringLiteral("storm");
    case Sky::Fog: return QStringLiteral("fog");
    }
    return QStringLiteral("cloudy");
}

// --- units ---
QString WeatherModel::temperatureUnit() const { return kTemperature.value(int(m_units.temperature)); }
QString WeatherModel::windUnit() const { return kWind.value(int(m_units.wind)); }
QString WeatherModel::precipitationUnit() const { return kPrecipitation.value(int(m_units.precipitation)); }
QString WeatherModel::pressureUnit() const { return kPressure.value(int(m_units.pressure)); }

void WeatherModel::saveUnits()
{
    if (m_settings) {
        m_settings->setValue(QStringLiteral("units/temperature"), temperatureUnit());
        m_settings->setValue(QStringLiteral("units/wind"), windUnit());
        m_settings->setValue(QStringLiteral("units/precipitation"), precipitationUnit());
        m_settings->setValue(QStringLiteral("units/pressure"), pressureUnit());
        m_settings->setValue(QStringLiteral("units/hour24"), m_units.hour24);
    }
    emit unitsChanged();
    emit changed();
}

void WeatherModel::setTemperatureUnit(const QString& k) { m_units.temperature = pick(kTemperature, k, m_units.temperature); saveUnits(); }
void WeatherModel::setWindUnit(const QString& k) { m_units.wind = pick(kWind, k, m_units.wind); saveUnits(); }
void WeatherModel::setPrecipitationUnit(const QString& k) { m_units.precipitation = pick(kPrecipitation, k, m_units.precipitation); saveUnits(); }
void WeatherModel::setPressureUnit(const QString& k) { m_units.pressure = pick(kPressure, k, m_units.pressure); saveUnits(); }
void WeatherModel::setHour24(bool on) { m_units.hour24 = on; saveUnits(); }
