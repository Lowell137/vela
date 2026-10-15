// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the QML shows (`Weather`): the saved places, the forecast of the
// chosen one with the cache under it, the city search and the units. The
// network is a Fetcher, replaced in tests; QML only sees ready-to-show text.

#include "weather.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QNetworkAccessManager;
class QSettings;

namespace vela::weather {

// Gets a URL; the callback receives the body, or an empty array on failure.
using Callback = std::function<void(const QByteArray&)>;
using Fetcher = std::function<void(const QUrl&, const Callback&)>;
Fetcher networkFetcher(QNetworkAccessManager* manager);

} // namespace vela::weather

class WeatherModel : public QObject {
    Q_OBJECT
    // [{name, detail, lat, lon, isDefault, current}]
    Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged)
    Q_PROPERTY(int placeIndex READ placeIndex NOTIFY changed)
    Q_PROPERTY(bool hasPlace READ hasPlace NOTIFY placesChanged)
    // [{name, detail, lat, lon, saved}]
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(bool searchFailed READ searchFailed NOTIFY searchChanged)
    Q_PROPERTY(bool searchEmpty READ searchEmpty NOTIFY searchChanged) // asked, nothing found

    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    Q_PROPERTY(bool offline READ offline NOTIFY changed) // the last refresh failed
    Q_PROPERTY(bool stale READ stale NOTIFY changed) // older than 15 minutes, or offline
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY changed)
    Q_PROPERTY(bool corruptPlaces READ corruptPlaces CONSTANT)

    // {place, temp, text, icon, feels, high, low}
    Q_PROPERTY(QVariantMap now READ now NOTIFY changed)
    // [{time, icon, text, temp, value, chance}]
    Q_PROPERTY(QVariantList hourly READ hourly NOTIFY changed)
    // [{day, icon, text, min, max, minValue, maxValue, chance}]
    Q_PROPERTY(QVariantList daily READ daily NOTIFY changed)
    // [{label, value}]
    Q_PROPERTY(QVariantList details READ details NOTIFY changed)
    // {rise, set, progress, daylight, valid}
    Q_PROPERTY(QVariantMap sun READ sun NOTIFY changed)
    // {available, value, band, text, pm25, pm10}
    Q_PROPERTY(QVariantMap air READ air NOTIFY changed)
    Q_PROPERTY(QString sky READ sky NOTIFY changed) // clear, cloudy, rain, snow, storm, fog
    Q_PROPERTY(bool night READ night NOTIFY changed)

    Q_PROPERTY(QString temperatureUnit READ temperatureUnit WRITE setTemperatureUnit NOTIFY unitsChanged) // c, f
    Q_PROPERTY(QString windUnit READ windUnit WRITE setWindUnit NOTIFY unitsChanged) // kmh, mph, ms, kn
    Q_PROPERTY(QString precipitationUnit READ precipitationUnit WRITE setPrecipitationUnit NOTIFY unitsChanged) // mm, in
    Q_PROPERTY(QString pressureUnit READ pressureUnit WRITE setPressureUnit NOTIFY unitsChanged) // hpa, inhg
    Q_PROPERTY(bool hour24 READ hour24 WRITE setHour24 NOTIFY unitsChanged)

public:
    // placesPath/cacheDir: where the data lives. shellConfig: vela-shell.conf,
    // read once (never written) to offer the taskbar widget's place when there
    // is no places file yet. settings: where the units are kept (may be null).
    WeatherModel(const QString& placesPath, const QString& cacheDir, const QString& shellConfig, QSettings* settings,
        vela::weather::Fetcher fetcher = {}, QObject* parent = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void choose(int index);
    Q_INVOKABLE void search(const QString& text);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE bool addResult(int index); // saves it and makes it the chosen place
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void setDefault(int index);

    QVariantList places() const;
    int placeIndex() const { return m_index; }
    bool hasPlace() const { return !m_store.places().empty(); }
    QVariantList searchResults() const;
    bool searching() const { return m_searching; }
    bool searchFailed() const { return m_searchFailed; }
    bool searchEmpty() const { return m_searched && !m_searching && !m_searchFailed && m_results.empty(); }
    bool loading() const { return m_loading; }
    bool hasData() const { return m_forecast.valid; }
    bool offline() const { return m_failed; }
    bool stale() const;
    QString updatedText() const;
    bool corruptPlaces() const { return m_corrupt; }

    QVariantMap now() const;
    QVariantList hourly() const;
    QVariantList daily() const;
    QVariantList details() const;
    QVariantMap sun() const;
    QVariantMap air() const;
    QString sky() const;
    bool night() const { return m_forecast.valid && !m_forecast.current.day; }

    QString temperatureUnit() const;
    QString windUnit() const;
    QString precipitationUnit() const;
    QString pressureUnit() const;
    bool hour24() const { return m_units.hour24; }
    void setTemperatureUnit(const QString& key);
    void setWindUnit(const QString& key);
    void setPrecipitationUnit(const QString& key);
    void setPressureUnit(const QString& key);
    void setHour24(bool on);

    // Test hooks: the clock.
    void setClock(std::function<QDateTime()> clock) { m_clock = std::move(clock); }

signals:
    void changed();
    void placesChanged();
    void searchChanged();
    void unitsChanged();

private:
    const vela::weather::Place* current() const;
    void load(); // the cache of the chosen place, if any
    void saveCache();
    void forecastArrived(quint64 generation, const QByteArray& body);
    void airArrived(quint64 generation, const QByteArray& body);
    void saveUnits();
    vela::weather::Fetcher& fetcher();

    vela::weather::PlacesStore m_store;
    QString m_cacheDir;
    QSettings* m_settings;
    vela::weather::Fetcher m_fetch;
    QNetworkAccessManager* m_manager = nullptr;
    std::function<QDateTime()> m_clock;
    vela::weather::Units m_units;
    bool m_corrupt = false;

    int m_index = -1;
    vela::weather::Forecast m_forecast;
    vela::weather::AirQuality m_air;
    QJsonObject m_forecastJson, m_airJson;
    QDateTime m_updated;
    bool m_failed = false, m_loading = false;
    quint64 m_generation = 0; // answers for another place are dropped
    QTimer m_timer; // every 15 minutes while the window is open

    std::vector<vela::weather::Place> m_results;
    bool m_searching = false, m_searchFailed = false, m_searched = false;
    quint64 m_searchGeneration = 0;
};
