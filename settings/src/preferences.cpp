// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences.h"
#include "appshortcuts.h"
#include "../../common/privatefile.h"

extern "C" {
#include "legacy_names.h"
}

#include "iconprovider.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QtEndian>
#include <QTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QIcon>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QLocalSocket>
#include <QMap>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <cmath>

namespace {


QSettings shellSettings()
{
    return QSettings(QStringLiteral("Vela"), QStringLiteral("vela-shell"));
}

QString compositorConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf");
}

// The directory of an installed cursor theme (one with cursors/), "" if none:
// where the compositor's libxcursor looks, first one wins.
QString cursorThemeDir(const QString& name)
{
    if (name.isEmpty() || name.contains(u'/') || name == QLatin1String("..")) {
        return {};
    }
    const QStringList roots { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/icons"),
        QDir::homePath() + QStringLiteral("/.icons"), QStringLiteral("/usr/share/icons") };
    for (const QString& root : roots) {
        if (QFileInfo(root + u'/' + name + QStringLiteral("/cursors")).isDir()) {
            return root + u'/' + name;
        }
    }
    return {};
}

// The first image of an Xcursor file nearest to `size`: "Xcur", header
// length, version, entry count, then (type, size, offset) entries; an image
// chunk has 9 32-bit fields (width, height at 16 and 20) and then ARGB pixels.
QImage xcursorImage(const QString& path, int size)
{
    QFile file(path);
    if (file.size() > 16 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.readAll();
    auto u32 = [&](qint64 at) {
        return at >= 0 && at + 4 <= data.size() ? qFromLittleEndian<quint32>(data.mid(at, 4).constData()) : 0u;
    };
    if (data.size() < 16 || !data.startsWith("Xcur")) {
        return {};
    }
    const quint32 count = u32(12);
    qint64 best = -1;
    quint32 bestSize = 0;
    for (quint32 i = 0; i < count && 16 + (i + 1) * 12 <= quint64(data.size()); ++i) {
        const qint64 entry = 16 + qint64(i) * 12;
        const quint32 entrySize = u32(entry + 4);
        if (u32(entry) == 0xfffd0002
            && (best < 0 || std::abs(int(entrySize) - size) < std::abs(int(bestSize) - size))) {
            best = u32(entry + 8);
            bestSize = entrySize;
        }
    }
    if (best < 0 || best + 36 > data.size()) {
        return {};
    }
    const quint32 width = u32(best + 16);
    const quint32 height = u32(best + 20);
    if (width == 0 || height == 0 || width > 1024 || height > 1024 || best + 36 + qint64(width) * height * 4 > data.size()) {
        return {};
    }
    QImage image(int(width), int(height), QImage::Format_ARGB32_Premultiplied);
    for (quint32 y = 0; y < height; ++y) {
        // Premultiplied ARGB in little-endian words, as QImage keeps them.
        qFromLittleEndian<quint32>(data.constData() + best + 36 + qint64(y) * width * 4, width, image.scanLine(int(y)));
    }
    return image;
}

// App ids as vela.conf keeps them: trimmed, without commas or control
// characters (they would break the list or the file), no duplicates.
QStringList cleanAppIds(const QStringList& apps)
{
    QStringList out;
    for (const QString& raw : apps) {
        const QString app = raw.trimmed();
        const bool bad = app.contains(u',')
            || std::any_of(app.cbegin(), app.cend(), [](QChar c) { return c.category() == QChar::Other_Control; });
        if (!app.isEmpty() && !bad && !out.contains(app, Qt::CaseInsensitive)) {
            out << app;
        }
    }
    return out;
}

// A question on the compositor's socket and its one-line answer (empty if
// none).
QByteArray askCompositor(const QByteArray& question)
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"));
    QLocalSocket socket;
    socket.connectToServer(runtimeDir + QStringLiteral("/vela-") + display + QStringLiteral(".sock"));
    if (!socket.waitForConnected(200)) {
        return {};
    }
    socket.write(question + '\n');
    socket.waitForBytesWritten(200);
    QByteArray reply;
    while (!reply.contains('\n') && socket.waitForReadyRead(200)) {
        reply += socket.readAll();
    }
    return reply.trimmed();
}

} // namespace

void sendToCompositor(const QByteArray& command)
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"));
    QLocalSocket socket;
    socket.connectToServer(runtimeDir + QStringLiteral("/vela-") + display + QStringLiteral(".sock"));
    if (socket.waitForConnected(200)) {
        socket.write(command + '\n');
        socket.waitForBytesWritten(200);
    }
}

Preferences::Preferences(QObject* parent)
    : QObject(parent)
{
    reload();
    // vela.conf changes outside of here too (the shell's quick settings).
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        watchConfig();
        reloadCompositor();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    watchConfig();
}

void Preferences::watchConfig()
{
    const QString path = compositorConfigPath();
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    if (info.exists() && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }
    if (!m_watcher.directories().contains(info.absolutePath())) {
        m_watcher.addPath(info.absolutePath());
    }
}

void Preferences::reload()
{
    QSettings settings = shellSettings();
    settings.sync();

    QString wallpaper = settings.value(QStringLiteral("appearance/wallpaper")).toString();
    if (wallpaper.isEmpty() || !QFileInfo::exists(wallpaper)) {
        wallpaper = vela::defaults::wallpaper();
    }
    const QStringList recent = settings.value(QStringLiteral("appearance/recentWallpapers")).toStringList();
    if (wallpaper != m_wallpaper || recent != m_recentWallpapers) {
        m_wallpaper = wallpaper;
        m_recentWallpapers = recent;
        emit wallpaperChanged();
    }

    QColor accent(settings.value(QStringLiteral("appearance/accent")).toString());
    if (!accent.isValid()) {
        accent = vela::defaults::accent();
    }
    if (accent != m_accent) {
        m_accent = accent;
        emit accentChanged();
    }

    m_taskbarAlignment = settings.value(QStringLiteral("taskbar/alignment")).toString() == QLatin1String("left")
        ? QStringLiteral("left")
        : QStringLiteral("center");
    m_taskbarPosition = settings.value(QStringLiteral("taskbar/position")).toString();
    if (m_taskbarPosition != QLatin1String("top") && m_taskbarPosition != QLatin1String("left")
        && m_taskbarPosition != QLatin1String("right")) {
        m_taskbarPosition = QStringLiteral("bottom");
    }
    m_taskbarStyle = settings.value(QStringLiteral("taskbar/style")).toString();
    if (m_taskbarStyle != QLatin1String("islands") && m_taskbarStyle != QLatin1String("dock")) {
        m_taskbarStyle = QStringLiteral("classic");
    }
    m_endTask = settings.value(QStringLiteral("taskbar/endTask"), true).toBool();
    m_taskView = settings.value(QStringLiteral("taskbar/taskView"), true).toBool();
    m_trayOverflow = settings.value(QStringLiteral("taskbar/trayOverflow"), true).toBool();
    m_taskbarAllScreens = settings.value(QStringLiteral("taskbar/allScreens"), true).toBool();
    m_taskbarAutoHide = settings.value(QStringLiteral("taskbar/autoHide"), false).toBool();
    m_taskbarCombine = settings.value(QStringLiteral("taskbar/combine")).toString() == QLatin1String("never")
        ? QStringLiteral("never")
        : QStringLiteral("always");
    const QString weather = settings.value(QStringLiteral("taskbar/weather")).toString().toLower();
    m_weather = weather == QLatin1String("on") || weather == QLatin1String("true") || weather == QLatin1String("1");
    m_weatherCity = settings.value(QStringLiteral("taskbar/weather-city")).toString();
    m_weatherUnit = settings.value(QStringLiteral("taskbar/weather-unit")).toString().toLower() == QLatin1String("f")
        ? QStringLiteral("f")
        : QStringLiteral("c");
    m_showDesktopCorner = settings.value(QStringLiteral("taskbar/showDesktopCorner"), true).toBool();
    emit taskbarChanged();
    m_doNotDisturb = settings.value(QStringLiteral("notifications/doNotDisturb"), false).toBool();
    emit doNotDisturbChanged();
    m_clipboardHistory = settings.value(QStringLiteral("clipboard/history"), false).toBool();
    m_clipboardAtPointer = settings.value(QStringLiteral("clipboard/atPointer"), true).toBool();
    m_clipboardKeep = settings.value(QStringLiteral("clipboard/keep"), false).toBool();
    m_clipboardDays = settings.value(QStringLiteral("clipboard/days"), 0).toInt();
    m_clipboardLimit = settings.value(QStringLiteral("clipboard/limit"), 25).toInt();
    emit clipboardChanged();

    // The mode: the one chosen here (vela-shell.conf); for apps, if not set
    // yet, the one GTK and the portal already know.
    QString appTheme = settings.value(QStringLiteral("appearance/appTheme")).toString();
    if (appTheme.isEmpty()) {
        QProcess gsettings;
        gsettings.start(QStringLiteral("gsettings"),
            { QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("color-scheme") });
        appTheme = gsettings.waitForFinished(1000) && gsettings.readAllStandardOutput().contains("light")
            ? QStringLiteral("light")
            : QStringLiteral("dark");
    }
    m_appTheme = appTheme == QLatin1String("light") ? QStringLiteral("light") : QStringLiteral("dark");
    m_shellTheme = settings.value(QStringLiteral("appearance/shellTheme")).toString() == QLatin1String("light")
        ? QStringLiteral("light")
        : QStringLiteral("dark");
    emit appThemeChanged();

    reloadCompositor();
}

namespace {

// A vela.conf line with the current names (compositor/src/legacy_names.h).
QString modernLine(const QString& line)
{
    const qsizetype eq = line.indexOf(u'=');
    if (line.isEmpty() || line.startsWith(u'#') || eq < 0) {
        return line;
    }
    const QByteArray key = line.left(eq).toUtf8();
    const QByteArray value = line.mid(eq + 1).toUtf8();
    const char* newKey = vela_legacy_key(key.constData());
    const char* newValue = vela_legacy_value(newKey ? newKey : key.constData(), value.constData());
    if (!newKey && !newValue) {
        return line;
    }
    return QString::fromUtf8(newKey ? QByteArray(newKey) : key) + u'='
        + QString::fromUtf8(newValue ? QByteArray(newValue) : value);
}

} // namespace

/** Refresh compositor settings, including the Night Light backend toggle. */
void Preferences::reloadCompositor()
{
    // vela.conf
    m_compositor.clear();
    QFile file(compositorConfigPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = modernLine(in.readLine());
            const qsizetype eq = line.indexOf(u'=');
            if (!line.startsWith(u'#') && eq > 0) {
                m_compositor.append({ line.left(eq), line.mid(eq + 1) });
            }
        }
    }
    auto value = [this](const QString& key, const QString& fallback) {
        const QString v = compositorValue(key);
        return v.isNull() ? fallback : v;
    };
    auto flag = [&](const QString& key, bool fallback) {
        const QString v = value(key, fallback ? QStringLiteral("yes") : QStringLiteral("no"));
        return v == QLatin1String("yes") || v == QLatin1String("1") || v == QLatin1String("true");
    };
    m_screenOffMinutes = value(QStringLiteral("screen-off"), QStringLiteral("10")).toInt();
    const QString lock = value(QStringLiteral("lock-on-idle"), QStringLiteral("yes"));
    m_lockOnIdle = lock != QLatin1String("0") && lock != QLatin1String("no");
    emit idleChanged();
    m_language = value(QStringLiteral("language"), {});
    emit languageChanged();
    m_iconTheme = value(QStringLiteral("icon-theme"), {});
    emit iconThemeChanged();
    m_visualTheme = value(QStringLiteral("theme"), {});
    m_taskbarBackdrop = flag(QStringLiteral("taskbar-backdrop"), false);
    for (const char* surface : { "taskbar", "startmenu", "notifications" }) {
        m_surfaceStyles.insert(QLatin1String(surface), value(QLatin1String(surface) + QStringLiteral("-style"), {}));
    }
    emit visualThemeChanged();
    m_cursorTheme = value(QStringLiteral("cursor-theme"), {});
    m_cursorSize = value(QStringLiteral("cursor-size"), qEnvironmentVariable("XCURSOR_SIZE", QStringLiteral("24"))).toInt();
    emit cursorChanged();
    m_windowBlur = flag(QStringLiteral("window-blur"), false);
    m_windowBlurAll = flag(QStringLiteral("window-blur-all"), false);
    m_windowBlurInclude = cleanAppIds(value(QStringLiteral("window-blur-include"), {}).split(u','));
    m_windowBlurExclude = cleanAppIds(value(QStringLiteral("window-blur-exclude"), {}).split(u','));
    m_windowOpacity = std::clamp(value(QStringLiteral("window-opacity"), QStringLiteral("100")).toInt(), 30, 100);
    emit windowBlurChanged();
    m_windowRules.clear();
    for (const auto& [key, rule] : std::as_const(m_compositor)) {
        if (key == QLatin1String("window-rule") && !rule.trimmed().isEmpty()) {
            m_windowRules << rule;
        }
    }
    emit windowRulesChanged();

    m_layouts.clear();
    const QStringList layouts = value(QStringLiteral("keyboard-layout"), {}).split(u',', Qt::SkipEmptyParts);
    const QStringList variants = value(QStringLiteral("keyboard-variant"), {}).split(u',');
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        m_layouts.append(QVariantMap { { QStringLiteral("layout"), layouts[i].trimmed() },
            { QStringLiteral("variant"), i < variants.size() ? variants[i].trimmed() : QString() } });
    }
    m_repeatDelay = value(QStringLiteral("keyboard-repeat-delay"), QStringLiteral("400")).toInt();
    m_repeatRate = value(QStringLiteral("keyboard-repeat-rate"), QStringLiteral("30")).toInt();
    emit keyboardChanged();
    loadShortcuts();

    m_nightLight = flag(QStringLiteral("night-light"), false);
    m_nightStrength = std::clamp(value(QStringLiteral("night-light-strength"), QStringLiteral("48")).toInt(), 0, 100);
    m_nightNvidiaFix = flag(QStringLiteral("night-light-nvidia-fix"), false);
    m_nightSchedule = value(QStringLiteral("night-light-schedule"), QStringLiteral("no"));
    m_nightFrom = value(QStringLiteral("night-light-from"), QStringLiteral("21:00"));
    m_nightTo = value(QStringLiteral("night-light-to"), QStringLiteral("07:00"));
    m_tearing = flag(QStringLiteral("tearing"), true);
    m_vrr = value(QStringLiteral("variable-refresh"), QStringLiteral("games"));
    m_directScanout = value(QStringLiteral("direct-scanout"), QStringLiteral("auto"));
    m_colorFilter = flag(QStringLiteral("color-filters"), false);
    m_colorFilterKind = value(QStringLiteral("color-filter"), QStringLiteral("grayscale"));
    m_colorFilterShortcut = flag(QStringLiteral("color-filters-shortcut"), false);
    m_magnifierStep = value(QStringLiteral("magnifier-step"), QStringLiteral("100")).toInt();
    m_stickyKeys = flag(QStringLiteral("sticky-keys"), false);
    m_mouseSpeed = std::clamp(value(QStringLiteral("mouse-speed"), QStringLiteral("10")).toInt(), 1, 20);
    m_mousePrecision = flag(QStringLiteral("mouse-precision"), true);
    m_mouseLeftHanded = value(QStringLiteral("mouse-primary-button"), QStringLiteral("left")) == QLatin1String("right");
    m_wheelLines = std::clamp(value(QStringLiteral("mouse-scroll-lines"), QStringLiteral("3")).toInt(), 1, 20);
    m_middleClickAutoscroll = flag(QStringLiteral("middle-click-autoscroll"), false);
    m_autoscrollSpeed = std::clamp(value(QStringLiteral("autoscroll-speed"), QStringLiteral("5")).toInt(), 1, 10);
    m_autoscrollClick = flag(QStringLiteral("autoscroll-click"), true);
    m_autoscrollHold = flag(QStringLiteral("autoscroll-hold"), true);
    m_touchpad = flag(QStringLiteral("touchpad"), true);
    m_touchpadWithMouse = flag(QStringLiteral("touchpad-with-mouse"), true);
    m_touchpadSpeed = std::clamp(value(QStringLiteral("touchpad-speed"), QStringLiteral("10")).toInt(), 1, 20);
    m_touchpadTap = flag(QStringLiteral("touchpad-tap"), true);
    m_touchpadNatural = flag(QStringLiteral("touchpad-natural-scroll"), true);
    m_hotCorner = flag(QStringLiteral("hot-corner"), true);
    m_taskbarAllDesktops = flag(QStringLiteral("taskbar-all-desktops"), false);
    m_captureIndicator = flag(QStringLiteral("capture-indicator"), true);
    m_captureDnd = flag(QStringLiteral("capture-dnd"), true);
    m_fullscreenDnd = flag(QStringLiteral("fullscreen-dnd"), true);
    m_hotCornerGlow = flag(QStringLiteral("hot-corner-glow"), true);
    m_hotCornerAnimation = flag(QStringLiteral("hot-corner-animation"), true);
    m_rememberWindowPositions = flag(QStringLiteral("remember-window-positions"), true);
    m_snapDragLayouts = flag(QStringLiteral("snap-drag-layouts"), true);
    m_snapDragDistance = std::clamp(value(QStringLiteral("snap-drag-trigger-distance"), QStringLiteral("64")).toInt(), 4, 200);
    m_snapDragWidth = std::clamp(value(QStringLiteral("snap-drag-trigger-width"), QStringLiteral("160")).toInt(), 0, 960);
    m_snapDragScale = std::clamp(value(QStringLiteral("snap-drag-scale"), QStringLiteral("100")).toInt(), 60, 200);
    m_threeFingers = value(QStringLiteral("touchpad-three-fingers"), QStringLiteral("app"));
    m_fourFingers = value(QStringLiteral("touchpad-four-fingers"), QStringLiteral("desktop"));
    emit inputChanged();
    queryCompositor();
    emit nightLightChanged();
    emit accessibilityChanged();
}

QString Preferences::compositorValue(const QString& key) const
{
    for (const auto& [k, v] : std::as_const(m_compositor)) {
        if (k == key) {
            return v;
        }
    }
    return {};
}

void Preferences::queryCompositor()
{
    // What isn't in the file: the magnifier (it opens and closes) and sun
    // times.
    const QJsonObject state = QJsonDocument::fromJson(askCompositor("accessibility")).object();
    if (state.isEmpty()) {
        return;
    }
    m_magnifier = state[QStringLiteral("magnifier")].toBool();
    m_hasTouchpad = state[QStringLiteral("touchpad")].toBool();
    m_sunset = state[QStringLiteral("sunset")].toString();
    m_sunrise = state[QStringLiteral("sunrise")].toString();
}

void Preferences::setShell(const QString& key, const QVariant& value)
{
    QSettings settings = shellSettings();
    settings.setValue(key, value);
    settings.sync();
}

// ---------------------------------------------------------- wallpaper --

QStringList Preferences::systemWallpapers() const
{
    // Installed wallpapers: KDE packages (a folder with
    // contents/images/<resolution>.<ext>, of which the largest is taken) and
    // loose images from GNOME and the like.
    QStringList out;
    const QStringList roots = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("wallpapers"),
        QStandardPaths::LocateDirectory)
        + QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("backgrounds"),
            QStandardPaths::LocateDirectory);
    const QStringList images { QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"),
        QStringLiteral("*.webp"), QStringLiteral("*.svg") };
    for (const QString& root : roots) {
        const QDir dir(root);
        for (const QFileInfo& entry : dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (entry.isDir()) {
                QDir contents(entry.filePath() + QStringLiteral("/contents/images"));
                QFileInfoList candidates = contents.entryInfoList(images, QDir::Files);
                if (candidates.isEmpty()) {
                    continue;
                }
                // The largest by area (names are "3840x2160.png").
                auto area = [](const QFileInfo& f) {
                    const QStringList wh = f.completeBaseName().split(u'x');
                    return wh.size() == 2 ? wh[0].toLongLong() * wh[1].toLongLong() : 0LL;
                };
                std::sort(candidates.begin(), candidates.end(),
                    [&](const QFileInfo& a, const QFileInfo& b) { return area(a) > area(b); });
                out.append(candidates.first().filePath());
            } else if (QDir::match(images, entry.fileName())) {
                out.append(entry.filePath());
            }
        }
    }
    return out;
}

void Preferences::setWallpaper(const QString& pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    if (path.isEmpty()) {
        return;
    }
    // Recent images, like Windows: the last five, without Vela's.
    QStringList recent = m_recentWallpapers;
    recent.removeAll(path);
    if (!path.startsWith(u':')) {
        recent.prepend(path);
    }
    while (recent.size() > 5) {
        recent.removeLast();
    }
    QSettings settings = shellSettings();
    settings.setValue(QStringLiteral("appearance/wallpaper"), path.startsWith(u':') ? QString() : path);
    settings.setValue(QStringLiteral("appearance/recentWallpapers"), recent);
    settings.sync();
    m_wallpaper = path;
    m_recentWallpapers = recent;
    emit wallpaperChanged();
}

// --------------------------------------------------- colors and theme --

void Preferences::setAccent(const QColor& color)
{
    if (!color.isValid() || color == m_accent) {
        return;
    }
    m_accent = color;
    setShell(QStringLiteral("appearance/accent"), color.name());
    emit accentChanged();
}

void Preferences::setAppTheme(const QString& theme)
{
    if (theme == m_appTheme || (theme != QLatin1String("light") && theme != QLatin1String("dark"))) {
        return;
    }
    m_appTheme = theme;
    const bool light = theme == QLatin1String("light");
    // The shell and the compositor (title bars), Explorer.
    setShell(QStringLiteral("appearance/appTheme"), theme);
    // GTK, the portal and whoever follows org.freedesktop.appearance.
    QProcess::startDetached(QStringLiteral("gsettings"),
        { QStringLiteral("set"), QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("color-scheme"),
            light ? QStringLiteral("prefer-light") : QStringLiteral("prefer-dark") });
    // KDE and Qt apps: the light or dark Breeze color scheme, and matching
    // icons.
    if (!QStandardPaths::findExecutable(QStringLiteral("plasma-apply-colorscheme")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("plasma-apply-colorscheme"),
            { light ? QStringLiteral("BreezeLight") : QStringLiteral("BreezeDark") });
    }
    const QString icons = QIcon::themeName();
    applyIconTheme(light); // ours too
    if (QIcon::themeName() != icons) {
        const QString changer = QStringLiteral("/usr/lib/plasma-changeicons");
        if (QFileInfo(changer).isExecutable()) {
            QProcess::startDetached(changer, { QIcon::themeName() });
        } else if (!QStandardPaths::findExecutable(QStringLiteral("kwriteconfig6")).isEmpty()) {
            QProcess::startDetached(QStringLiteral("kwriteconfig6"),
                { QStringLiteral("--file"), QStringLiteral("kdeglobals"), QStringLiteral("--group"), QStringLiteral("Icons"),
                    QStringLiteral("--key"), QStringLiteral("Theme"), QIcon::themeName() });
        }
    }
    emit appThemeChanged();
}

void Preferences::setIconTheme(const QString& theme)
{
    if (theme == m_iconTheme || (!theme.isEmpty() && !iconThemeExists(theme))) {
        return;
    }
    m_iconTheme = theme;
    // Shell, Settings (Appearance watches vela.conf) and compositor.
    saveCompositor({ { QStringLiteral("icon-theme"), theme.isEmpty() ? QString() : theme } });
    emit iconThemeChanged();
}

void Preferences::setVisualTheme(const QString& theme)
{
    if (theme == m_visualTheme) {
        return;
    }
    m_visualTheme = theme;
    // Shell and apps (Appearance watches vela.conf).
    saveCompositor({ { QStringLiteral("theme"), theme.isEmpty() ? QString() : theme } });
    emit visualThemeChanged();
}

void Preferences::setTaskbarBackdrop(bool on)
{
    if (on == m_taskbarBackdrop) {
        return;
    }
    m_taskbarBackdrop = on;
    saveCompositor({ { QStringLiteral("taskbar-backdrop"), on ? QStringLiteral("yes") : QStringLiteral("no") } });
    emit visualThemeChanged();
}

void Preferences::setSurfaceStyle(const QString& surface, const QString& style)
{
    if (!m_surfaceStyles.contains(surface) || m_surfaceStyles.value(surface).toString() == style) {
        return;
    }
    m_surfaceStyles.insert(surface, style);
    saveCompositor({ { surface + QStringLiteral("-style"), style.isEmpty() ? QString() : style } });
    emit visualThemeChanged();
}

QVariantList Preferences::visualThemes(const QString& surface) const
{
    // The user's themes win over the installed ones of the same name.
    QMap<QString, QString> files;
    const QString dir = surface.isEmpty() ? QStringLiteral("vela/themes") : QStringLiteral("vela/themes/") + surface;
    QStringList roots = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, dir, QStandardPaths::LocateDirectory);
    roots.prepend(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' + dir);
    for (const QString& root : roots) {
        for (const QString& file : QDir(root).entryList({ QStringLiteral("*.conf") }, QDir::Files)) {
            files.insert(file.chopped(5), root + u'/' + file);
        }
    }
    QVariantList list;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        // "# Window Glass: ..." names it; else the file name.
        QString text = it.key();
        QFile file(it.value());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString first = QString::fromUtf8(file.readLine()).trimmed();
            const qsizetype colon = first.indexOf(u':');
            if (first.startsWith(u'#') && colon > 1) {
                text = first.mid(1, colon - 1).trimmed();
            }
        }
        list.append(QVariantMap { { QStringLiteral("name"), it.key() }, { QStringLiteral("text"), text } });
    }
    return list;
}

void Preferences::setCursorTheme(const QString& theme)
{
    if (theme == m_cursorTheme || (!theme.isEmpty() && cursorThemeDir(theme).isEmpty())) {
        return;
    }
    m_cursorTheme = theme;
    saveCompositor({ { QStringLiteral("cursor-theme"), theme.isEmpty() ? QString() : theme } });
    emit cursorChanged();
}

void Preferences::setCursorSize(int size)
{
    if (size == m_cursorSize || size < 8 || size > 256) {
        return;
    }
    m_cursorSize = size;
    saveCompositor({ { QStringLiteral("cursor-size"), QString::number(size) } });
    emit cursorChanged();
}

void Preferences::setWindowBlur(bool on)
{
    if (on == m_windowBlur) {
        return;
    }
    m_windowBlur = on;
    saveCompositor({ { QStringLiteral("window-blur"), on ? QStringLiteral("yes") : QStringLiteral("no") } });
    emit windowBlurChanged();
}

void Preferences::setWindowBlurAll(bool on)
{
    if (on == m_windowBlurAll) {
        return;
    }
    m_windowBlurAll = on;
    saveCompositor({ { QStringLiteral("window-blur-all"), on ? QStringLiteral("yes") : QStringLiteral("no") } });
    emit windowBlurChanged();
}

void Preferences::setWindowBlurInclude(const QStringList& apps)
{
    const QStringList clean = cleanAppIds(apps);
    if (clean == m_windowBlurInclude) {
        return;
    }
    m_windowBlurInclude = clean;
    saveCompositor({ { QStringLiteral("window-blur-include"), clean.isEmpty() ? QString() : clean.join(u',') } });
    emit windowBlurChanged();
}

void Preferences::setWindowRules(const QStringList& rules)
{
    if (rules == m_windowRules) {
        return;
    }
    m_windowRules = rules;
    // One window-rule line per rule: saveCompositor replaces all of them.
    saveCompositor({ { QStringLiteral("window-rule"), rules.isEmpty() ? QString() : rules.join(u'\n') } });
    emit windowRulesChanged();
}

void Preferences::setWindowBlurExclude(const QStringList& apps)
{
    const QStringList clean = cleanAppIds(apps);
    if (clean == m_windowBlurExclude) {
        return;
    }
    m_windowBlurExclude = clean;
    saveCompositor({ { QStringLiteral("window-blur-exclude"), clean.isEmpty() ? QString() : clean.join(u',') } });
    emit windowBlurChanged();
}

void Preferences::setWindowOpacity(int percent)
{
    percent = std::clamp(percent, 30, 100);
    if (percent == m_windowOpacity) {
        return;
    }
    m_windowOpacity = percent;
    saveCompositor({ { QStringLiteral("window-opacity"), QString::number(percent) } });
    emit windowBlurChanged();
}

QVariantList Preferences::openWindows() const
{
    const QJsonArray windows = QJsonDocument::fromJson(askCompositor("state")).object()[QStringLiteral("windows")].toArray();
    QVariantList result;
    QStringList seen;
    for (const QJsonValue& window : windows) {
        const QString app = window[QStringLiteral("app")].toString();
        if (app.isEmpty() || seen.contains(app)) {
            continue;
        }
        seen << app;
        result.append(QVariantMap { { QStringLiteral("appId"), app },
            { QStringLiteral("title"), window[QStringLiteral("title")].toString() } });
    }
    return result;
}

void Preferences::pickWindow()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"));
    auto* socket = new QLocalSocket(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [this, socket, finished](const QString& appId, const QString& title) {
        if (*finished) {
            return;
        }
        *finished = true;
        socket->deleteLater();
        emit windowPicked(appId, title);
    };
    connect(socket, &QLocalSocket::connected, socket, [socket] { socket->write("pick-window\n"); });
    connect(socket, &QLocalSocket::readyRead, socket, [socket, finish] {
        // One line: {"appId":...,"title":...,"output":...} or "cancel".
        if (!socket->canReadLine()) {
            return;
        }
        const QJsonObject window = QJsonDocument::fromJson(socket->readLine()).object();
        finish(window[QStringLiteral("appId")].toString(), window[QStringLiteral("title")].toString());
    });
    connect(socket, &QLocalSocket::disconnected, socket, [finish] { finish({}, {}); });
    connect(socket, &QLocalSocket::errorOccurred, socket, [finish] { finish({}, {}); });
    socket->connectToServer(runtimeDir + QStringLiteral("/vela-") + display + QStringLiteral(".sock"));
}

QVariantList Preferences::cursorThemes() const
{
    QMap<QString, QString> themes; // name -> Name= of index.theme
    const QStringList roots { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/icons"),
        QDir::homePath() + QStringLiteral("/.icons"), QStringLiteral("/usr/share/icons") };
    for (const QString& root : roots) {
        const QStringList names = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& name : names) {
            if (themes.contains(name) || cursorThemeDir(name) != root + u'/' + name) {
                continue;
            }
            QString text = name;
            QFile file(root + u'/' + name + QStringLiteral("/index.theme"));
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!file.atEnd()) {
                    const QString line = QString::fromUtf8(file.readLine()).trimmed();
                    if (line.startsWith(QLatin1String("Name="))) {
                        text = line.mid(5).trimmed();
                        break;
                    }
                }
            }
            themes.insert(name, text);
        }
    }
    QVariantList result;
    for (auto it = themes.cbegin(); it != themes.cend(); ++it) {
        result.append(QVariantMap { { QStringLiteral("name"), it.key() }, { QStringLiteral("text"), it.value() } });
    }
    std::sort(result.begin(), result.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()[QStringLiteral("text")].toString().compare(b.toMap()[QStringLiteral("text")].toString(), Qt::CaseInsensitive) < 0;
    });
    return result;
}

QString Preferences::cursorPreview(const QString& theme, int size) const
{
    const QString dir = cursorThemeDir(theme.isEmpty() ? qEnvironmentVariable("XCURSOR_THEME") : theme);
    if (dir.isEmpty()) {
        return {};
    }
    for (const char* name : { "default", "left_ptr" }) {
        const QImage image = xcursorImage(dir + QStringLiteral("/cursors/") + QLatin1String(name), size);
        if (!image.isNull()) {
            QByteArray png;
            QBuffer buffer(&png);
            image.save(&buffer, "PNG");
            return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
        }
    }
    return {};
}

QVariantList Preferences::iconThemes() const
{
    // name -> Name=; the first directory wins, like Qt's lookup. Only themes
    // with icon directories: not the cursor-only ones, not hicolor (Hidden).
    QMap<QString, QString> themes;
    const QStringList roots = QIcon::themeSearchPaths();
    for (const QString& root : roots) {
        const QStringList names = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& name : names) {
            if (themes.contains(name)) {
                continue;
            }
            QFile file(root + u'/' + name + QStringLiteral("/index.theme"));
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            QString text = name;
            QStringList directories;
            bool hidden = false;
            bool inside = false;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith(u'[')) {
                    if (inside) {
                        break;
                    }
                    inside = line == QLatin1String("[Icon Theme]");
                } else if (inside && line.startsWith(QLatin1String("Name="))) {
                    text = line.mid(5).trimmed();
                } else if (inside && line.startsWith(QLatin1String("Directories="))) {
                    directories = line.mid(12).split(u',', Qt::SkipEmptyParts);
                } else if (inside && line.startsWith(QLatin1String("Hidden="))) {
                    hidden = line.mid(7).trimmed() == QLatin1String("true");
                }
            }
            directories.removeIf([](const QString& d) { return d.trimmed().startsWith(QLatin1String("cursors")); });
            if (!hidden && !directories.isEmpty()) {
                themes.insert(name, text);
            }
        }
    }
    QVariantList result;
    for (auto it = themes.cbegin(); it != themes.cend(); ++it) {
        const QString& name = it.key();
        if (name.endsWith(QLatin1String("-dark"), Qt::CaseInsensitive) && themes.contains(name.chopped(5))) {
            continue; // the dark variant of a listed theme
        }
        result.append(QVariantMap { { QStringLiteral("name"), name }, { QStringLiteral("text"), it.value() } });
    }
    std::sort(result.begin(), result.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()[QStringLiteral("text")].toString().compare(b.toMap()[QStringLiteral("text")].toString(), Qt::CaseInsensitive) < 0;
    });
    return result;
}

void Preferences::setShellTheme(const QString& theme)
{
    if (theme == m_shellTheme || (theme != QLatin1String("light") && theme != QLatin1String("dark"))) {
        return;
    }
    m_shellTheme = theme;
    setShell(QStringLiteral("appearance/shellTheme"), theme);
    emit appThemeChanged();
}

// ------------------------------------------------------------ taskbar --

void Preferences::setTaskbarAlignment(const QString& alignment)
{
    if (alignment != m_taskbarAlignment) {
        m_taskbarAlignment = alignment;
        setShell(QStringLiteral("taskbar/alignment"), alignment);
        emit taskbarChanged();
    }
}

void Preferences::setTaskbarStyle(const QString& style)
{
    const QString value = style == QLatin1String("islands") || style == QLatin1String("dock") ? style : QStringLiteral("classic");
    if (value != m_taskbarStyle) {
        m_taskbarStyle = value;
        setShell(QStringLiteral("taskbar/style"), value);
        emit taskbarChanged();
    }
}

void Preferences::setTaskbarPosition(const QString& position)
{
    if (position != m_taskbarPosition) {
        m_taskbarPosition = position;
        setShell(QStringLiteral("taskbar/position"), position);
        emit taskbarChanged();
    }
}

void Preferences::setEndTask(bool on)
{
    if (on != m_endTask) {
        m_endTask = on;
        setShell(QStringLiteral("taskbar/endTask"), on);
        emit taskbarChanged();
    }
}

void Preferences::setTaskbarAllScreens(bool on)
{
    if (on != m_taskbarAllScreens) {
        m_taskbarAllScreens = on;
        setShell(QStringLiteral("taskbar/allScreens"), on);
        emit taskbarChanged();
    }
}

void Preferences::setTaskbarAutoHide(bool on)
{
    if (on != m_taskbarAutoHide) {
        m_taskbarAutoHide = on;
        setShell(QStringLiteral("taskbar/autoHide"), on);
        emit taskbarChanged();
    }
}

void Preferences::setTaskbarCombine(const QString& mode)
{
    const QString value = mode == QLatin1String("never") ? QStringLiteral("never") : QStringLiteral("always");
    if (value != m_taskbarCombine) {
        m_taskbarCombine = value;
        setShell(QStringLiteral("taskbar/combine"), value);
        emit taskbarChanged();
    }
}

void Preferences::setShowDesktopCorner(bool on)
{
    if (on != m_showDesktopCorner) {
        m_showDesktopCorner = on;
        setShell(QStringLiteral("taskbar/showDesktopCorner"), on);
        emit taskbarChanged();
    }
}

void Preferences::setTaskView(bool on)
{
    if (on != m_taskView) {
        m_taskView = on;
        setShell(QStringLiteral("taskbar/taskView"), on);
        emit taskbarChanged();
    }
}

void Preferences::setTrayOverflow(bool on)
{
    if (on != m_trayOverflow) {
        m_trayOverflow = on;
        setShell(QStringLiteral("taskbar/trayOverflow"), on);
        emit taskbarChanged();
    }
}

void Preferences::setWeather(bool on)
{
    if (on != m_weather) {
        m_weather = on;
        setShell(QStringLiteral("taskbar/weather"), on ? QStringLiteral("on") : QStringLiteral("off"));
        emit taskbarChanged();
    }
}

void Preferences::setWeatherUnit(const QString& unit)
{
    const QString value = unit == QLatin1String("f") ? QStringLiteral("f") : QStringLiteral("c");
    if (value != m_weatherUnit) {
        m_weatherUnit = value;
        setShell(QStringLiteral("taskbar/weather-unit"), value);
        emit taskbarChanged();
    }
}

void Preferences::setWeatherPlace(const QString& city, const QString& lat, const QString& lon)
{
    m_weatherCity = city;
    QSettings settings = shellSettings();
    settings.setValue(QStringLiteral("taskbar/weather-city"), city);
    settings.setValue(QStringLiteral("taskbar/weather-lat"), lat);
    settings.setValue(QStringLiteral("taskbar/weather-lon"), lon);
    settings.sync();
    emit taskbarChanged();
}

void Preferences::setDoNotDisturb(bool on)
{
    if (on != m_doNotDisturb) {
        m_doNotDisturb = on;
        setShell(QStringLiteral("notifications/doNotDisturb"), on);
        emit doNotDisturbChanged();
    }
}

// ------------------------------------------------------- compositor --

void Preferences::saveCompositorKeys(const QStringList& keys)
{
    QList<QPair<QString, QString>> changes;
    for (const QString& key : keys) {
        if (key == QLatin1String("screen-off")) {
            changes.append({ key, QString::number(m_screenOffMinutes) });
        } else if (key == QLatin1String("lock-on-idle")) {
            changes.append({ key, m_lockOnIdle ? QStringLiteral("yes") : QStringLiteral("no") });
        } else if (key == QLatin1String("language")) {
            changes.append({ key, m_language });
        } else if (key == QLatin1String("keyboard-layout") || key == QLatin1String("keyboard-variant")) {
            QStringList values;
            for (const QVariant& value : std::as_const(m_layouts)) {
                values << value.toMap()[key == QLatin1String("keyboard-layout") ? QStringLiteral("layout") : QStringLiteral("variant")].toString();
            }
            changes.append({ key, values.join(u',') });
        } else if (key == QLatin1String("keyboard-repeat-delay")) {
            changes.append({ key, QString::number(m_repeatDelay) });
        } else if (key == QLatin1String("keyboard-repeat-rate")) {
            changes.append({ key, QString::number(m_repeatRate) });
        }
    }
    saveCompositor(changes);
}

void Preferences::saveCompositor(const QList<QPair<QString, QString>>& changes)
{
    // The file as it is now (the compositor may have changed it), with these
    // new keys.
    const QString path = compositorConfigPath();
    QStringList lines;
    {
        QFile in(path);
        if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
            lines = QString::fromUtf8(in.readAll()).split(u'\n');
            for (QString& line : lines) {
                line = modernLine(line); // the old Italian names, if the compositor hasn't changed them already
            }
            while (!lines.isEmpty() && lines.last().isEmpty()) {
                lines.removeLast();
            }
        }
    }
    if (lines.isEmpty()) {
        lines << QStringLiteral("# Vela settings (written by the Settings app)");
    }
    for (const auto& [key, value] : changes) {
        if (value.isNull()) {
            // A null value removes the key.
            lines.erase(std::remove_if(lines.begin(), lines.end(),
                            [&](const QString& line) { return line.startsWith(key + u'='); }),
                lines.end());
            continue;
        }
        if (key == QLatin1String("window-rule")) {
            // Repeatable: all the old lines out, one new line per rule.
            lines.erase(std::remove_if(lines.begin(), lines.end(),
                            [&](const QString& line) { return line.startsWith(key + u'='); }),
                lines.end());
            for (const QString& rule : value.split(u'\n', Qt::SkipEmptyParts)) {
                lines << key + u'=' + rule;
            }
            continue;
        }
        bool found = false;
        for (QString& line : lines) {
            if (line.startsWith(key + u'=')) {
                line = key + u'=' + value;
                found = true;
                break;
            }
        }
        if (!found) {
            lines << key + u'=' + value;
        }
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    // Existing settings may have been created with mode 0644.
    if (!vela::openPrivateFile(file, QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    const QByteArray data = (lines.join(u'\n') + u'\n').toUtf8();
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return;
    }
    if (file.commit()) {
        // The compositor rereads (and our copy of the file updates from the
        // watcher).
        sendToCompositor("reload-config");
    }
}

void Preferences::setScreenOffMinutes(int minutes)
{
    if (minutes != m_screenOffMinutes) {
        m_screenOffMinutes = std::max(0, minutes);
        saveCompositorKeys(QStringList { QStringLiteral("screen-off") });
        emit idleChanged();
    }
}

void Preferences::setLockOnIdle(bool on)
{
    if (on != m_lockOnIdle) {
        m_lockOnIdle = on;
        saveCompositorKeys(QStringList { QStringLiteral("lock-on-idle") });
        emit idleChanged();
    }
}

void Preferences::setCompositorValue(const QString& key, const QString& program)
{
    // System tools (Default apps): the chosen program, empty for automatic
    // (the key goes away).
    saveCompositor({ { key, program.isEmpty() ? QString() : program } });
}

void Preferences::setLanguage(const QString& language)
{
    if (language != m_language) {
        m_language = language;
        saveCompositorKeys(QStringList { QStringLiteral("language") });
        emit languageChanged();
    }
}

void Preferences::saveLayouts()
{
    saveCompositorKeys(QStringList { QStringLiteral("keyboard-layout"), QStringLiteral("keyboard-variant") });
    emit keyboardChanged();
}

void Preferences::addKeyboardLayout(const QString& layout, const QString& variant)
{
    for (const QVariant& value : std::as_const(m_layouts)) {
        const QVariantMap entry = value.toMap();
        if (entry[QStringLiteral("layout")] == layout && entry[QStringLiteral("variant")] == variant) {
            return;
        }
    }
    m_layouts.append(QVariantMap { { QStringLiteral("layout"), layout }, { QStringLiteral("variant"), variant } });
    saveLayouts();
}

void Preferences::removeKeyboardLayout(int index)
{
    if (index >= 0 && index < m_layouts.size()) {
        m_layouts.removeAt(index);
        saveLayouts();
    }
}

void Preferences::moveKeyboardLayoutUp(int index)
{
    if (index > 0 && index < m_layouts.size()) {
        m_layouts.swapItemsAt(index, index - 1);
        saveLayouts();
    }
}

void Preferences::setRepeatDelay(int ms)
{
    if (ms != m_repeatDelay) {
        m_repeatDelay = std::clamp(ms, 100, 2000);
        saveCompositorKeys(QStringList { QStringLiteral("keyboard-repeat-delay") });
        emit keyboardChanged();
    }
}

void Preferences::setRepeatRate(int perSecond)
{
    if (perSecond != m_repeatRate) {
        m_repeatRate = std::clamp(perSecond, 1, 100);
        saveCompositorKeys(QStringList { QStringLiteral("keyboard-repeat-rate") });
        emit keyboardChanged();
    }
}

// ------------------------------------------- night light and accessibility --

namespace {

QString yesNo(bool on)
{
    return on ? QStringLiteral("yes") : QStringLiteral("no");
}

} // namespace

void Preferences::setNightLight(bool on)
{
    if (on != m_nightLight) {
        m_nightLight = on;
        saveCompositor({ { QStringLiteral("night-light"), yesNo(on) } });
        emit nightLightChanged();
    }
}

void Preferences::setNightStrength(int strength)
{
    strength = std::clamp(strength, 0, 100);
    if (strength != m_nightStrength) {
        m_nightStrength = strength;
        saveCompositor({ { QStringLiteral("night-light-strength"), QString::number(strength) } });
        emit nightLightChanged();
    }
}

/** Persist the NVIDIA workaround setting and notify bindings in the UI. */
void Preferences::setNightNvidiaFix(bool on)
{
    if (on != m_nightNvidiaFix) {
        m_nightNvidiaFix = on;
        saveCompositor({ { QStringLiteral("night-light-nvidia-fix"), yesNo(on) } });
        emit nightLightChanged();
    }
}

void Preferences::setNightSchedule(const QString& schedule)
{
    if (schedule != m_nightSchedule) {
        m_nightSchedule = schedule;
        saveCompositor({ { QStringLiteral("night-light-schedule"), schedule } });
        emit nightLightChanged();
    }
}

void Preferences::setNightFrom(const QString& time)
{
    if (time != m_nightFrom && QTime::fromString(time, QStringLiteral("HH:mm")).isValid()) {
        m_nightFrom = time;
        saveCompositor({ { QStringLiteral("night-light-from"), time } });
        emit nightLightChanged();
    }
}

void Preferences::setNightTo(const QString& time)
{
    if (time != m_nightTo && QTime::fromString(time, QStringLiteral("HH:mm")).isValid()) {
        m_nightTo = time;
        saveCompositor({ { QStringLiteral("night-light-to"), time } });
        emit nightLightChanged();
    }
}

void Preferences::setTearing(bool on)
{
    if (on != m_tearing) {
        m_tearing = on;
        saveCompositor({ { QStringLiteral("tearing"), yesNo(on) } });
        emit accessibilityChanged();
    }
}

void Preferences::setVrr(const QString& mode)
{
    if (mode != m_vrr) {
        m_vrr = mode;
        saveCompositor({ { QStringLiteral("variable-refresh"), mode } });
        emit accessibilityChanged();
    }
}

void Preferences::setDirectScanout(const QString& mode)
{
    if (mode != m_directScanout) {
        m_directScanout = mode;
        saveCompositor({ { QStringLiteral("direct-scanout"), mode } });
        emit accessibilityChanged();
    }
}

void Preferences::setMagnifier(bool on)
{
    if (on != m_magnifier) {
        m_magnifier = on;
        sendToCompositor(on ? "magnifier on" : "magnifier off");
        emit accessibilityChanged();
    }
}

void Preferences::setMagnifierStep(int percent)
{
    if (percent != m_magnifierStep) {
        m_magnifierStep = percent;
        saveCompositor({ { QStringLiteral("magnifier-step"), QString::number(percent) } });
        emit accessibilityChanged();
    }
}

void Preferences::setColorFilter(bool on)
{
    if (on != m_colorFilter) {
        m_colorFilter = on;
        saveCompositor({ { QStringLiteral("color-filters"), yesNo(on) } });
        emit accessibilityChanged();
    }
}

void Preferences::setColorFilterKind(const QString& kind)
{
    if (kind != m_colorFilterKind) {
        m_colorFilterKind = kind;
        saveCompositor({ { QStringLiteral("color-filter"), kind } });
        emit accessibilityChanged();
    }
}

void Preferences::setColorFilterShortcut(bool on)
{
    if (on != m_colorFilterShortcut) {
        m_colorFilterShortcut = on;
        saveCompositor({ { QStringLiteral("color-filters-shortcut"), yesNo(on) } });
        emit accessibilityChanged();
    }
}

void Preferences::setStickyKeys(bool on)
{
    if (on != m_stickyKeys) {
        m_stickyKeys = on;
        saveCompositor({ { QStringLiteral("sticky-keys"), yesNo(on) } });
        emit accessibilityChanged();
    }
}

// ---------------------------------------------------- mouse and touchpad --

void Preferences::setMouseSpeed(int value)
{
    value = std::clamp(value, 1, 20);
    if (value != m_mouseSpeed) {
        m_mouseSpeed = value;
        saveCompositor({ { QStringLiteral("mouse-speed"), QString::number(value) } });
        emit inputChanged();
    }
}

void Preferences::setMousePrecision(bool on)
{
    if (on != m_mousePrecision) {
        m_mousePrecision = on;
        saveCompositor({ { QStringLiteral("mouse-precision"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setMouseLeftHanded(bool on)
{
    if (on != m_mouseLeftHanded) {
        m_mouseLeftHanded = on;
        saveCompositor({ { QStringLiteral("mouse-primary-button"), on ? QStringLiteral("right") : QStringLiteral("left") } });
        emit inputChanged();
    }
}

void Preferences::setWheelLines(int lines)
{
    lines = std::clamp(lines, 1, 20);
    if (lines != m_wheelLines) {
        m_wheelLines = lines;
        saveCompositor({ { QStringLiteral("mouse-scroll-lines"), QString::number(lines) } });
        emit inputChanged();
    }
}

void Preferences::setMiddleClickAutoscroll(bool on)
{
    if (on != m_middleClickAutoscroll) {
        m_middleClickAutoscroll = on;
        saveCompositor({ { QStringLiteral("middle-click-autoscroll"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setAutoscrollSpeed(int value)
{
    value = std::clamp(value, 1, 10);
    if (value != m_autoscrollSpeed) {
        m_autoscrollSpeed = value;
        saveCompositor({ { QStringLiteral("autoscroll-speed"), QString::number(value) } });
        emit inputChanged();
    }
}

void Preferences::setAutoscrollClick(bool on)
{
    if (on != m_autoscrollClick) {
        m_autoscrollClick = on;
        saveCompositor({ { QStringLiteral("autoscroll-click"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setAutoscrollHold(bool on)
{
    if (on != m_autoscrollHold) {
        m_autoscrollHold = on;
        saveCompositor({ { QStringLiteral("autoscroll-hold"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setTouchpad(bool on)
{
    if (on != m_touchpad) {
        m_touchpad = on;
        saveCompositor({ { QStringLiteral("touchpad"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setTouchpadWithMouse(bool on)
{
    if (on != m_touchpadWithMouse) {
        m_touchpadWithMouse = on;
        saveCompositor({ { QStringLiteral("touchpad-with-mouse"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setTouchpadSpeed(int value)
{
    value = std::clamp(value, 1, 20);
    if (value != m_touchpadSpeed) {
        m_touchpadSpeed = value;
        saveCompositor({ { QStringLiteral("touchpad-speed"), QString::number(value) } });
        emit inputChanged();
    }
}

void Preferences::setTouchpadTap(bool on)
{
    if (on != m_touchpadTap) {
        m_touchpadTap = on;
        saveCompositor({ { QStringLiteral("touchpad-tap"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setTaskbarAllDesktops(bool on)
{
    if (on != m_taskbarAllDesktops) {
        m_taskbarAllDesktops = on;
        saveCompositor({ { QStringLiteral("taskbar-all-desktops"), yesNo(on) } });
        emit taskbarChanged();
    }
}

void Preferences::setHotCorner(bool on)
{
    if (on != m_hotCorner) {
        m_hotCorner = on;
        saveCompositor({ { QStringLiteral("hot-corner"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setCaptureDnd(bool on)
{
    if (on != m_captureDnd) {
        m_captureDnd = on;
        saveCompositor({ { QStringLiteral("capture-dnd"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setFullscreenDnd(bool on)
{
    if (on != m_fullscreenDnd) {
        m_fullscreenDnd = on;
        saveCompositor({ { QStringLiteral("fullscreen-dnd"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setCaptureIndicator(bool on)
{
    if (on != m_captureIndicator) {
        m_captureIndicator = on;
        saveCompositor({ { QStringLiteral("capture-indicator"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setHotCornerGlow(bool on)
{
    if (on != m_hotCornerGlow) {
        m_hotCornerGlow = on;
        saveCompositor({ { QStringLiteral("hot-corner-glow"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setHotCornerAnimation(bool on)
{
    if (on != m_hotCornerAnimation) {
        m_hotCornerAnimation = on;
        saveCompositor({ { QStringLiteral("hot-corner-animation"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setRememberWindowPositions(bool on)
{
    if (on != m_rememberWindowPositions) {
        m_rememberWindowPositions = on;
        saveCompositor({ { QStringLiteral("remember-window-positions"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setSnapDragLayouts(bool on)
{
    if (on != m_snapDragLayouts) {
        m_snapDragLayouts = on;
        saveCompositor({ { QStringLiteral("snap-drag-layouts"), yesNo(on) } });
        emit shortcutsChanged();
    }
}

void Preferences::setSnapDragDistance(int value)
{
    value = std::clamp(value, 4, 200);
    if (value != m_snapDragDistance) {
        m_snapDragDistance = value;
        saveCompositor({ { QStringLiteral("snap-drag-trigger-distance"), QString::number(value) } });
        emit shortcutsChanged();
    }
}

void Preferences::setSnapDragWidth(int value)
{
    value = std::clamp(value, 0, 960);
    if (value != m_snapDragWidth) {
        m_snapDragWidth = value;
        saveCompositor({ { QStringLiteral("snap-drag-trigger-width"), QString::number(value) } });
        emit shortcutsChanged();
    }
}

void Preferences::setSnapDragScale(int value)
{
    value = std::clamp(value, 60, 200);
    if (value != m_snapDragScale) {
        m_snapDragScale = value;
        saveCompositor({ { QStringLiteral("snap-drag-scale"), QString::number(value) } });
        emit shortcutsChanged();
    }
}

void Preferences::setTouchpadNatural(bool on)
{
    if (on != m_touchpadNatural) {
        m_touchpadNatural = on;
        saveCompositor({ { QStringLiteral("touchpad-natural-scroll"), yesNo(on) } });
        emit inputChanged();
    }
}

void Preferences::setThreeFingers(const QString& action)
{
    if (action != m_threeFingers) {
        m_threeFingers = action;
        saveCompositor({ { QStringLiteral("touchpad-three-fingers"), action } });
        emit inputChanged();
    }
}

void Preferences::setFourFingers(const QString& action)
{
    if (action != m_fourFingers) {
        m_fourFingers = action;
        saveCompositor({ { QStringLiteral("touchpad-four-fingers"), action } });
        emit inputChanged();
    }
}

// --------------------------------------------------------------- clipboard --

void Preferences::setClipboardHistory(bool on)
{
    if (on != m_clipboardHistory) {
        m_clipboardHistory = on;
        setShell(QStringLiteral("clipboard/history"), on);
        emit clipboardChanged();
    }
}

void Preferences::setClipboardAtPointer(bool on)
{
    if (on != m_clipboardAtPointer) {
        m_clipboardAtPointer = on;
        setShell(QStringLiteral("clipboard/atPointer"), on);
        emit clipboardChanged();
    }
}

void Preferences::setClipboardKeep(bool on)
{
    if (on != m_clipboardKeep) {
        m_clipboardKeep = on;
        setShell(QStringLiteral("clipboard/keep"), on);
        emit clipboardChanged();
    }
}

void Preferences::setClipboardDays(int days)
{
    if (days != m_clipboardDays) {
        m_clipboardDays = days;
        setShell(QStringLiteral("clipboard/days"), days);
        emit clipboardChanged();
    }
}

void Preferences::setClipboardLimit(int limit)
{
    if (limit != m_clipboardLimit) {
        m_clipboardLimit = limit;
        setShell(QStringLiteral("clipboard/limit"), limit);
        emit clipboardChanged();
    }
}

void Preferences::clearClipboard()
{
    // To the shell, on its socket.
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"));
    QLocalSocket socket;
    socket.connectToServer(runtimeDir + QStringLiteral("/vela-shell-") + display + QStringLiteral(".sock"));
    if (socket.waitForConnected(200)) {
        socket.write("clipboard-clear\n");
        socket.waitForBytesWritten(200);
    }
}

// -------------------------------------------------------- shortcuts --

namespace {

// The actions the compositor lets the user change (compositor/src/shortcuts.c)
// with their built-in keys (docs/shortcuts.md).
struct ShortcutAction {
    const char* id;
    const char* title;
    const char* combo;
};

const ShortcutAction kShortcutActions[] = {
    { "close", QT_TRANSLATE_NOOP("Preferences", "Close window"), "Alt+F4" },
    { "terminal", QT_TRANSLATE_NOOP("Preferences", "Terminal"), "Super+Return" },
    { "lock", QT_TRANSLATE_NOOP("Preferences", "Lock"), "Super+L" },
    { "task-view", QT_TRANSLATE_NOOP("Preferences", "Task View"), "Super+Tab" },
    { "spotlight", QT_TRANSLATE_NOOP("Preferences", "Spotlight search"), "Super+Space" },
    { "show-desktop", QT_TRANSLATE_NOOP("Preferences", "Show desktop"), "Super+D" },
    { "files", QT_TRANSLATE_NOOP("Preferences", "File Explorer"), "Super+E" },
    { "settings", QT_TRANSLATE_NOOP("Preferences", "Settings"), "Super+I" },
    { "run", QT_TRANSLATE_NOOP("Preferences", "Run"), "Super+R" },
    { "spotlight", QT_TRANSLATE_NOOP("Preferences", "Spotlight"), "Super+Space" },
    { "winx", QT_TRANSLATE_NOOP("Preferences", "Power-user menu"), "Super+X" },
    { "quick-settings", QT_TRANSLATE_NOOP("Preferences", "Quick settings"), "Super+A" },
    { "notification-center", QT_TRANSLATE_NOOP("Preferences", "Notification center"), "Super+N" },
    { "clipboard", QT_TRANSLATE_NOOP("Preferences", "Clipboard history"), "Super+V" },
    { "emoji", QT_TRANSLATE_NOOP("Preferences", "Emoji and more"), "Super+." },
    { "sound-output", QT_TRANSLATE_NOOP("Preferences", "Sound output and volume mixer"), "Super+Ctrl+V" },
    { "snip", QT_TRANSLATE_NOOP("Preferences", "Snipping tool"), "Super+Shift+S" },
    { "snip-text", QT_TRANSLATE_NOOP("Preferences", "Copy text from the screen"), "Super+Shift+T" },
};

struct FixedShortcut {
    const char* title;
    const char* combo;
};

const FixedShortcut kFixedShortcuts[] = {
    { QT_TRANSLATE_NOOP("Preferences", "Switch windows"), "Alt+Tab" },
    { QT_TRANSLATE_NOOP("Preferences", "Snap left / right"), "Super+Left / Super+Right" },
    { QT_TRANSLATE_NOOP("Preferences", "Maximize / restore, then minimize"), "Super+Up / Super+Down" },
    { QT_TRANSLATE_NOOP("Preferences", "Snap layouts"), "Super+Z" },
    { QT_TRANSLATE_NOOP("Preferences", "Previous / next desktop"), "Super+Ctrl+Left / Super+Ctrl+Right" },
    { QT_TRANSLATE_NOOP("Preferences", "Move the window to the previous / next desktop"), "Super+Ctrl+Shift+Left / Super+Ctrl+Shift+Right" },
    { QT_TRANSLATE_NOOP("Preferences", "Go to desktop 1 to 10"), "Super+1 ... Super+9, Super+0" },
    { QT_TRANSLATE_NOOP("Preferences", "Move the window to desktop 1 to 10"), "Super+Shift+1 ... Super+Shift+9, Super+Shift+0" },
    { QT_TRANSLATE_NOOP("Preferences", "New desktop"), "Super+Ctrl+D" },
    { QT_TRANSLATE_NOOP("Preferences", "Close desktop"), "Super+Ctrl+F4" },
    { QT_TRANSLATE_NOOP("Preferences", "Window menu"), "Alt+Space" },
    { QT_TRANSLATE_NOOP("Preferences", "Next keyboard layout"), "Super+Space" },
    { QT_TRANSLATE_NOOP("Preferences", "Magnifier in / out / off"), "Super+Plus / Super+Minus / Super+Esc" },
    { QT_TRANSLATE_NOOP("Preferences", "Quit Vela"), "Alt+Shift+Esc" },
};

QString canonicalKey(const QString& raw)
{
    const QString key = raw.trimmed();
    const QString lower = key.toLower();
    static const QHash<QString, QString> named {
        { QStringLiteral("enter"), QStringLiteral("Return") }, { QStringLiteral("return"), QStringLiteral("Return") },
        { QStringLiteral("esc"), QStringLiteral("Escape") }, { QStringLiteral("escape"), QStringLiteral("Escape") },
        { QStringLiteral("space"), QStringLiteral("space") }, { QStringLiteral("tab"), QStringLiteral("Tab") },
        { QStringLiteral("left"), QStringLiteral("Left") }, { QStringLiteral("right"), QStringLiteral("Right") },
        { QStringLiteral("up"), QStringLiteral("Up") }, { QStringLiteral("down"), QStringLiteral("Down") },
        { QStringLiteral("print"), QStringLiteral("Print") }, { QStringLiteral("home"), QStringLiteral("Home") },
        { QStringLiteral("end"), QStringLiteral("End") }, { QStringLiteral("delete"), QStringLiteral("Delete") },
        { QStringLiteral("backspace"), QStringLiteral("BackSpace") }, { QStringLiteral("insert"), QStringLiteral("Insert") },
        { QStringLiteral("pageup"), QStringLiteral("Prior") }, { QStringLiteral("pagedown"), QStringLiteral("Next") },
        { QStringLiteral("plus"), QStringLiteral("plus") }, { QStringLiteral("minus"), QStringLiteral("minus") },
    };
    if (named.contains(lower)) {
        return named.value(lower);
    }
    static const QRegularExpression fkey(QStringLiteral("^f([1-9]|1[0-9]|2[0-4])$"));
    if (fkey.match(lower).hasMatch()) {
        return QStringLiteral("F") + lower.mid(1);
    }
    if (key.size() == 1 && key.at(0).isLetterOrNumber()) {
        return key.toUpper();
    }
    return {};
}

} // namespace

QString Preferences::normalizeShortcut(const QString& text) const
{
    bool super = false, ctrl = false, alt = false, shift = false;
    QString key;
    const QStringList parts = text.split(u'+');
    for (const QString& raw : parts) {
        const QString part = raw.trimmed();
        const QString lower = part.toLower();
        if (part.isEmpty()) {
            return {};
        }
        if (lower == QLatin1String("super") || lower == QLatin1String("win") || lower == QLatin1String("meta")
            || lower == QLatin1String("logo")) {
            super = true;
        } else if (lower == QLatin1String("ctrl") || lower == QLatin1String("control")) {
            ctrl = true;
        } else if (lower == QLatin1String("alt")) {
            alt = true;
        } else if (lower == QLatin1String("shift")) {
            shift = true;
        } else if (key.isEmpty()) {
            key = canonicalKey(part);
            if (key.isEmpty()) {
                return {};
            }
        } else {
            return {}; // two keys
        }
    }
    if (key.isEmpty()) {
        return {};
    }
    const bool functionKey = key.startsWith(u'F') && key.size() > 1 && key.at(1).isDigit();
    if (!(super || ctrl || alt) && !functionKey && key != QLatin1String("Print")) {
        return {}; // would take a plain key away from the apps
    }
    QStringList out;
    if (super) {
        out << QStringLiteral("Super");
    }
    if (ctrl) {
        out << QStringLiteral("Ctrl");
    }
    if (alt) {
        out << QStringLiteral("Alt");
    }
    if (shift) {
        out << QStringLiteral("Shift");
    }
    out << key;
    return out.join(u'+');
}

QVariantList Preferences::fixedShortcuts() const
{
    QVariantList list;
    for (const FixedShortcut& fixed : kFixedShortcuts) {
        list.append(QVariantMap { { QStringLiteral("title"), tr(fixed.title) },
            { QStringLiteral("combo"), QString::fromLatin1(fixed.combo) } });
    }
    return list;
}

void Preferences::loadShortcuts()
{
    m_shortcuts.clear();
    for (const ShortcutAction& action : kShortcutActions) {
        const QString key = QStringLiteral("shortcut-") + QLatin1String(action.id);
        const QString defaultCombo = QString::fromLatin1(action.combo);
        QString combo = defaultCombo;
        bool changed = false;
        const QString saved = compositorValue(key);
        if (!saved.isNull()) {
            changed = true;
            const QString normal = normalizeShortcut(saved);
            combo = normal; // "" when "none" or not valid: the built-in key is off
        }
        m_shortcuts.append(QVariantMap { { QStringLiteral("id"), QString::fromLatin1(action.id) },
            { QStringLiteral("title"), tr(action.title) }, { QStringLiteral("combo"), combo },
            { QStringLiteral("defaultCombo"), defaultCombo }, { QStringLiteral("changed"), changed } });
    }
    m_customShortcuts.clear();
    QList<int> numbers;
    for (const auto& [key, value] : std::as_const(m_compositor)) {
        if (key.startsWith(QLatin1String("shortcut-custom-"))) {
            bool ok = false;
            const int number = key.mid(16).toInt(&ok);
            if (ok && value.contains(u'|')) {
                numbers.append(number);
            }
        }
    }
    std::sort(numbers.begin(), numbers.end());
    for (int number : std::as_const(numbers)) {
        const QString value = compositorValue(QStringLiteral("shortcut-custom-") + QString::number(number));
        const qsizetype bar = value.indexOf(u'|');
        m_customShortcuts.append(QVariantMap { { QStringLiteral("number"), number },
            { QStringLiteral("combo"), normalizeShortcut(value.left(bar)) }, { QStringLiteral("command"), value.mid(bar + 1) } });
    }
    // The keys apps asked for through the portal (appshortcuts.h).
    m_appShortcuts.clear();
    for (const vela::AppShortcut& s : vela::loadAppShortcuts()) {
        m_appShortcuts.append(QVariantMap { { QStringLiteral("number"), s.number },
            { QStringLiteral("combo"), normalizeShortcut(s.combo) }, { QStringLiteral("app"), s.appId },
            { QStringLiteral("description"), s.description.isEmpty() ? s.id : s.description } });
    }
    emit shortcutsChanged();
}

QString Preferences::shortcutConflict(const QString& combo, const QString& exceptKey) const
{
    const QString wanted = combo.toLower();
    for (const QVariant& entry : m_shortcuts) {
        const QVariantMap map = entry.toMap();
        const QString key = QStringLiteral("shortcut-") + map.value(QStringLiteral("id")).toString();
        if (key != exceptKey && map.value(QStringLiteral("combo")).toString().toLower() == wanted) {
            return tr("%1 already uses %2").arg(map.value(QStringLiteral("title")).toString(), combo);
        }
    }
    for (const QVariant& entry : m_customShortcuts) {
        const QVariantMap map = entry.toMap();
        const QString key = QStringLiteral("shortcut-custom-") + map.value(QStringLiteral("number")).toString();
        if (key != exceptKey && map.value(QStringLiteral("combo")).toString().toLower() == wanted) {
            return tr("Another shortcut already uses %1").arg(combo);
        }
    }
    for (const QVariant& entry : m_appShortcuts) {
        const QVariantMap map = entry.toMap();
        const QString key = QStringLiteral("shortcut-app-") + map.value(QStringLiteral("number")).toString();
        if (key != exceptKey && map.value(QStringLiteral("combo")).toString().toLower() == wanted) {
            return tr("%1 already uses %2").arg(map.value(QStringLiteral("app")).toString(), combo);
        }
    }
    for (const FixedShortcut& fixed : kFixedShortcuts) {
        const QStringList alternatives = QString::fromLatin1(fixed.combo).split(QStringLiteral(" / "));
        for (const QString& alternative : alternatives) {
            if (alternative.toLower() == wanted) {
                return tr("%1 already uses %2").arg(tr(fixed.title), combo);
            }
        }
    }
    return {};
}

QString Preferences::setShortcut(const QString& id, const QString& combo)
{
    bool known = false;
    for (const ShortcutAction& action : kShortcutActions) {
        known = known || id == QLatin1String(action.id);
    }
    if (!known) {
        return tr("Unknown shortcut");
    }
    const QString key = QStringLiteral("shortcut-") + id;
    if (combo.trimmed().isEmpty()) {
        saveCompositor({ { key, QStringLiteral("none") } }); // the built-in key off, nothing in its place
        return {};
    }
    const QString normal = normalizeShortcut(combo);
    if (normal.isEmpty()) {
        return tr("Use modifiers and a key, for example Super+Q");
    }
    const QString conflict = shortcutConflict(normal, key);
    if (!conflict.isEmpty()) {
        return conflict;
    }
    saveCompositor({ { key, normal } });
    return {};
}

void Preferences::resetShortcut(const QString& id)
{
    saveCompositor({ { QStringLiteral("shortcut-") + id, QString() } });
}

QString Preferences::addCustomShortcut(const QString& combo, const QString& command)
{
    int number = 1;
    for (const QVariant& entry : m_customShortcuts) {
        number = std::max(number, entry.toMap().value(QStringLiteral("number")).toInt() + 1);
    }
    return updateCustomShortcut(number, combo, command);
}

QString Preferences::updateCustomShortcut(int number, const QString& combo, const QString& command)
{
    const QString normal = normalizeShortcut(combo);
    if (normal.isEmpty()) {
        return tr("Use modifiers and a key, for example Super+T");
    }
    const QString program = command.trimmed();
    if (program.isEmpty() || program.contains(u'\n')) {
        return tr("Type the command to run");
    }
    const QString key = QStringLiteral("shortcut-custom-") + QString::number(number);
    const QString conflict = shortcutConflict(normal, key);
    if (!conflict.isEmpty()) {
        return conflict;
    }
    saveCompositor({ { key, normal + u'|' + program } });
    return {};
}

void Preferences::removeCustomShortcut(int number)
{
    saveCompositor({ { QStringLiteral("shortcut-custom-") + QString::number(number), QString() } });
}

QString Preferences::setAppShortcut(int number, const QString& combo)
{
    QList<vela::AppShortcut> all = vela::loadAppShortcuts();
    auto it = std::find_if(all.begin(), all.end(), [&](const vela::AppShortcut& s) { return s.number == number; });
    if (it == all.end()) {
        return tr("Unknown shortcut");
    }
    QString normal;
    if (!combo.trimmed().isEmpty()) {
        normal = normalizeShortcut(combo);
        if (normal.isEmpty()) {
            return tr("Use modifiers and a key, for example Super+Q");
        }
        const QString conflict = shortcutConflict(normal, QStringLiteral("shortcut-app-") + QString::number(number));
        if (!conflict.isEmpty()) {
            return conflict;
        }
    }
    it->combo = normal;
    if (!vela::saveAppShortcuts(all)) {
        return tr("Couldn't save the shortcut");
    }
    sendToCompositor("reload-config");
    return {};
}

void Preferences::removeAppShortcut(int number)
{
    QList<vela::AppShortcut> all = vela::loadAppShortcuts();
    all.removeIf([&](const vela::AppShortcut& s) { return s.number == number; });
    if (vela::saveAppShortcuts(all)) {
        sendToCompositor("reload-config");
    }
}
