// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gamestats.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class AppModel;
class NotificationServer;

// The Game Bar's live side (Win+G, GameBar.qml, GameOverlay.qml): the
// compositor's frame statistics ("perf", every 2 s while frames are drawn),
// the focused app ("perfapp"), and while someone looks (`active`) CPU, GPU
// and memory once a second. GameMode for the focused game and instant replay
// (gpu-screen-recorder) when they're installed. Numbers not known are -1.
class GameBar : public QObject {
    Q_OBJECT
    // The panel or the overlay is on screen: only then the 1 s sampling and
    // nvidia-smi run.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(double fps READ fps NOTIFY perfChanged)
    Q_PROPERTY(double frameMs READ frameMs NOTIFY perfChanged)
    Q_PROPERTY(int missed READ missed NOTIFY perfChanged)
    // No "perf" line for 5 s: nothing is being drawn (or game-stats=no).
    Q_PROPERTY(bool perfStale READ perfStale NOTIFY perfChanged)
    // The last minute, oldest first (one value per "perf" line).
    Q_PROPERTY(QVariantList fpsHistory READ fpsHistory NOTIFY perfChanged)
    Q_PROPERTY(QVariantList frameHistory READ frameHistory NOTIFY perfChanged)
    Q_PROPERTY(QString appId READ appId NOTIFY appChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY appChanged)
    Q_PROPERTY(QString appIcon READ appIcon NOTIFY appChanged)
    Q_PROPERTY(int pid READ pid NOTIFY appChanged)
    Q_PROPERTY(double cpu READ cpu NOTIFY statsChanged)
    Q_PROPERTY(double cpuTemp READ cpuTemp NOTIFY statsChanged)
    Q_PROPERTY(double gpu READ gpu NOTIFY statsChanged)
    Q_PROPERTY(double gpuTemp READ gpuTemp NOTIFY statsChanged)
    Q_PROPERTY(double gpuPower READ gpuPower NOTIFY statsChanged)
    Q_PROPERTY(double vramUsed READ vramUsed NOTIFY statsChanged) // MB
    Q_PROPERTY(double vramTotal READ vramTotal NOTIFY statsChanged)
    Q_PROPERTY(double ramUsed READ ramUsed NOTIFY statsChanged) // MB
    Q_PROPERTY(double ramTotal READ ramTotal NOTIFY statsChanged)
    // GameMode (gamemoded on the session bus): the row is hidden without it.
    Q_PROPERTY(bool gameModeAvailable READ gameModeAvailable NOTIFY gameModeChanged)
    Q_PROPERTY(bool gameMode READ gameMode NOTIFY gameModeChanged)
    // Instant replay: gpu-screen-recorder keeps the last 30 s in memory.
    Q_PROPERTY(bool replayAvailable READ replayAvailable CONSTANT)
    Q_PROPERTY(bool replay READ replay NOTIFY replayChanged)
    Q_PROPERTY(bool mangoHudInstalled READ mangoHudInstalled CONSTANT)

public:
    GameBar(AppModel* apps, NotificationServer* notifications, QObject* parent = nullptr);
    ~GameBar() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    double fps() const { return m_fps; }
    double frameMs() const { return m_frameMs; }
    int missed() const { return m_missed; }
    bool perfStale() const { return m_perfStale; }
    QVariantList fpsHistory() const { return m_fpsHistory.values(); }
    QVariantList frameHistory() const { return m_frameHistory.values(); }
    QString appId() const { return m_appId; }
    QString appName() const;
    QString appIcon() const;
    int pid() const { return m_pid; }
    double cpu() const { return m_cpu; }
    double cpuTemp() const { return m_cpuTemp; }
    double gpu() const { return m_gpu.busy; }
    double gpuTemp() const { return m_gpu.temp; }
    double gpuPower() const { return m_gpu.power; }
    double vramUsed() const { return m_gpu.memUsedMb; }
    double vramTotal() const { return m_gpu.memTotalMb; }
    double ramUsed() const { return m_ramUsed; }
    double ramTotal() const { return m_ramTotal; }
    bool gameModeAvailable() const { return m_gameModeAvailable; }
    bool gameMode() const { return m_pid > 0 && m_gameModePid == m_pid; }
    Q_INVOKABLE void setGameMode(bool on);
    bool replayAvailable() const { return !m_replayProgram.isEmpty(); }
    bool replay() const { return m_replay != nullptr; }
    Q_INVOKABLE void setReplay(bool on);
    Q_INVOKABLE void saveReplay();
    bool mangoHudInstalled() const { return m_mangoHud; }

    // The compositor's lines (ShellController).
    void perf(double fps, double frameMs, int missed);
    void focusedApp(const QString& appId, int pid);

signals:
    void activeChanged();
    void perfChanged();
    void appChanged();
    void statsChanged();
    void gameModeChanged();
    void replayChanged();

private:
    void sample();
    void sampleGpuSysfs();
    void startNvidia();
    void stopNvidia();
    void findSensors();

    AppModel* m_apps;
    NotificationServer* m_notifications;
    bool m_active = false;
    double m_fps = -1;
    double m_frameMs = -1;
    int m_missed = 0;
    bool m_perfStale = true;
    gamestats::RingBuffer m_fpsHistory { 30 }; // 60 s of 2 s lines
    gamestats::RingBuffer m_frameHistory { 30 };
    QTimer m_staleTimer;
    QString m_appId;
    int m_pid = 0;

    QTimer m_sampler;
    gamestats::CpuTimes m_cpuTimes;
    double m_cpu = -1;
    double m_cpuTemp = -1;
    double m_ramUsed = -1;
    double m_ramTotal = -1;
    gamestats::GpuSample m_gpu;
    QString m_cpuTempFile; // hwmon temp1_input of k10temp, coretemp, zenpower
    QString m_gpuDevice; // /sys/class/drm/cardN/device with gpu_busy_percent
    QString m_gpuHwmon; // its hwmon directory
    bool m_nvidia = false;
    QPointer<QProcess> m_nvidiaSmi;

    bool m_gameModeAvailable = false;
    int m_gameModePid = 0;

    QString m_replayProgram;
    QProcess* m_replay = nullptr;
    QHash<uint, QString> m_saved; // notification -> saved replay
    bool m_mangoHud = false;
};
