// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-shell: Vela's taskbar and Start menu.
//
// Every shell piece is a "layer-shell" window: the compositor knows it's part
// of the desktop (not an app), keeps it above or below the windows and
// reserves space for it on the output.

#include "../../common/portaldialogs.h"
#include "accessibility.h"
#include "appmodel.h"
#include "audio.h"
#include "audioapps.h"
#include "bluetooth/bluetooth.h"
#include "bluetoothprompts.h"
#include "updatenotices.h"
#include "updates/updates.h"
#include "brightness.h"
#include "clipboard.h"
#include "emoji.h"
#include "config.h"
#include "systemstats.h"
#include "weather.h"
#include "controls.h"
#include "desktopmodel.h"
#include "fileactions.h"
#include "filesearch.h"
#include "fileproperties.h"
#include "filethumbnails.h"
#include "foreigntoplevels.h"
#include "iconprovider.h"
#include "jumplists.h"
#include "language.h"
#include "levelmeter.h"
#include "launcherbadges.h"
#include "mediakeys.h"
#include "mica.h"
#include "network.h"
#include "filechooser.h"
#include "globalshortcuts.h"
#include "notifications.h"
#include "servicemenus.h"
#include "shellcontroller.h"
#include "snip.h"
#include "soundkeys.h"
#include "systemactions.h"
#include "systemstatus.h"
#include "taskbarmodel.h"
#include "transfermonitor.h"
#include "tray.h"
#include "wallpaperprovider.h"
#include "screenwindows.h"
#include "../../common/primaryoutput.h"
#include "workspaces.h"
#include "windowcapture.h"

#include <LayerShellQt/Window>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QtDebug>
#include <QtQml/QQmlExtensionPlugin>

#include <algorithm>
#include <cstdio>
#include <memory>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)
Q_IMPORT_QML_PLUGIN(Vela_Bluetooth_UiPlugin)

namespace {

using LayerWindow = LayerShellQt::Window;

// Opt-in diagnostics for the real functional test: observe the event *before*
// Qt Quick maps it into MouseArea coordinates. Never adjust or consume input.
class SelectionInputTrace : public QObject {
public:
    explicit SelectionInputTrace(QQuickWindow* window) : QObject(window), m_window(window) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_window && (event->type() == QEvent::MouseButtonPress
                || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseMove)) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton || (mouse->buttons() & Qt::LeftButton)) {
                const QPointF local = mouse->position();
                const QPointF global = mouse->globalPosition();
                const QString screenName = m_window->screen() ? m_window->screen()->name() : QStringLiteral("none");
                qInfo("VELA_SELECTION qt event=%d screen=%s local=%.8f,%.8f global=%.8f,%.8f "
                      "window=%dx%d effectiveDpr=%.8f",
                      int(event->type()), qPrintable(screenName), local.x(), local.y(), global.x(), global.y(),
                      m_window->width(), m_window->height(), m_window->effectiveDevicePixelRatio());
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QQuickWindow* m_window;
};

void releaseHiddenPanel(QQuickWindow* window)
{
    // Qt can retain the wl_surface and its EGL buffers after hide(). A new
    // layer role then shares that native surface with the previous rendering
    // cycle. Use a fresh surface on reopen, including its configure handshake.
    // Keep the QML window, models and LayerShellQt settings intact.
    QObject::connect(window, &QWindow::visibleChanged, window, [window](bool visible) {
        if (!visible) {
            QTimer::singleShot(0, window, [window] {
                if (!window->isVisible()) {
                    window->destroy();
                }
            });
        }
    });
}

// The taskbar's edge (taskbar/position) and the controller that moves what
// follows it, for the taskbars ScreenWindows creates as outputs appear.
ShellController* taskbarShell = nullptr;

void setupTaskbar(QQuickWindow* window, QScreen* screen)
{
    LayerWindow* layer = LayerWindow::get(window);
    layer->setScreen(screen); // one per output (ScreenWindows)
    layer->setScope(QStringLiteral("vela-taskbar"));
    layer->setLayer(LayerWindow::LayerTop);
    layer->setAnchors(LayerWindow::Anchors(LayerWindow::AnchorBottom) | LayerWindow::AnchorLeft
        | LayerWindow::AnchorRight);
    layer->setExclusiveZone(std::min(window->width(), window->height())); // its thickness; maximized windows don't cover it
    layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
    taskbarShell->followTaskbar(window, ShellController::FollowKind::Taskbar);
    // The dock style is taller than the classic bar: the reserved space follows.
    QObject::connect(window, &QWindow::heightChanged, window, [window, layer] {
        layer->setExclusiveZone(std::min(window->width(), window->height()));
    });
}

// The dock style's top panel: it reserves its own strip at the top.
void setupTopBar(QQuickWindow* window, QScreen* screen)
{
    LayerWindow* layer = LayerWindow::get(window);
    layer->setScreen(screen);
    layer->setScope(QStringLiteral("vela-topbar"));
    layer->setLayer(LayerWindow::LayerTop);
    layer->setAnchors(LayerWindow::Anchors(LayerWindow::AnchorTop) | LayerWindow::AnchorLeft | LayerWindow::AnchorRight);
    layer->setExclusiveZone(window->height());
    layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
}

void setupWallpaper(QQuickWindow* window, QScreen* screen)
{
    LayerWindow* layer = LayerWindow::get(window);
    layer->setScreen(screen); // this very output, not the one the compositor would choose
    layer->setScope(QStringLiteral("vela-wallpaper"));
    layer->setLayer(LayerWindow::LayerBackground);
    layer->setAnchors(LayerWindow::Anchors(LayerWindow::AnchorTop) | LayerWindow::AnchorBottom
        | LayerWindow::AnchorLeft | LayerWindow::AnchorRight);
    layer->setExclusiveZone(-1); // the whole output, also below the taskbar
    // Desktop icons take the keyboard when clicked (F2, Del, Ctrl+C...).
    layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityOnDemand);
    if (qEnvironmentVariableIsSet("VELA_DEBUG_SELECTION")) {
        window->setProperty("selectionTraceEnabled", true);
        window->installEventFilter(new SelectionInputTrace(window));
    }
}

// The shell's other windows: one each, made layer-shell surfaces by
// loadPanel. Anchored at no edge, the compositor centers a window; an
// exclusive zone of -1 covers the whole output, also below the taskbar, and 0
// keeps out of the space the taskbar reserved.
struct Panel {
    const char* qml; // the component in Vela.Shell
    const char* scope;
    LayerWindow::Layer layer;
    LayerWindow::Anchors anchors;
    int exclusiveZone;
    LayerWindow::KeyboardInteractivity keyboard;
    QMargins margins = {};
    bool clickThrough = false; // neither clicks nor keyboard
};

constexpr LayerWindow::Anchors centered;
constexpr LayerWindow::Anchors bottom(LayerWindow::AnchorBottom);
constexpr LayerWindow::Anchors bottomLeft = bottom | LayerWindow::AnchorLeft;
constexpr LayerWindow::Anchors bottomRight = bottom | LayerWindow::AnchorRight;
constexpr LayerWindow::Anchors topLeft = LayerWindow::Anchors(LayerWindow::AnchorTop) | LayerWindow::AnchorLeft;
constexpr LayerWindow::Anchors fill = bottom | LayerWindow::AnchorTop | LayerWindow::AnchorLeft | LayerWindow::AnchorRight;
constexpr auto layerTop = LayerWindow::LayerTop;
constexpr auto layerOverlay = LayerWindow::LayerOverlay;
constexpr auto keysNone = LayerWindow::KeyboardInteractivityNone;
constexpr auto keysOnDemand = LayerWindow::KeyboardInteractivityOnDemand;
constexpr auto keysExclusive = LayerWindow::KeyboardInteractivityExclusive;

const Panel panels[] = {
    // Above the taskbar, which reserved its space: the window touches it, so
    // the rising panel is cut there and seems to come out from behind it,
    // like on Windows 11. Centered or at the left: placeStartMenu.
    { "StartMenu", "vela-start-menu", layerTop, bottom, 0, keysOnDemand },
    // Fullscreen and transparent, the panel in the center. The keyboard stays
    // with the compositor, which handles Alt+Tab.
    { "Switcher", "vela-switcher", layerOverlay, fill, -1, keysNone },
    { "NotificationPopups", "vela-notifications", layerOverlay, bottomRight, 0, keysNone },
    // Right-click menus: the whole output, transparent, above everything; a
    // click outside closes them. Arrows, Enter and Esc.
    { "ContextMenu", "vela-context-menu", layerOverlay, fill, -1, keysOnDemand },
    // "Run", at the bottom left above the taskbar, like in Windows; the
    // window touches both, the dialog rests 12 units from them.
    { "RunDialog", "vela-run", layerTop, bottomLeft, 0, keysOnDemand },
    // Questions such as "Empty Recycle Bin", and what to share on screen.
    { "ConfirmDialog", "vela-confirm", layerOverlay, centered, 0, keysOnDemand },
    { "SourceChooser", "vela-share", layerOverlay, centered, 0, keysOnDemand },
    // The Bluetooth agent's questions (docs/bluetooth.md §9.3).
    { "BluetoothDialog", "vela-bluetooth-question", layerOverlay, centered, 0, keysOnDemand },
    { "PropertiesDialog", "vela-properties", layerTop, centered, 0, keysOnDemand },
    { "QuickSettings", "vela-quick-settings", layerTop, bottomRight, 0, keysOnDemand },
    { "NotificationCenter", "vela-notification-center", layerTop, bottomRight, 0, keysOnDemand },
    // All the space above the taskbar, which stays visible and clickable,
    // like in Windows.
    { "TaskView", "vela-task-view", layerTop, fill, 0, keysExclusive },
    // The desktop's name in the center of the output.
    { "DesktopOsd", "vela-desktop-osd", layerOverlay, centered, -1, keysNone, {}, true },
    // The hot corner's glow and pulse, in the top-left corner; the mouse goes
    // through.
    { "HotCorner", "vela-hot-corner", layerOverlay, topLeft, -1, keysNone, {}, true },
    // The volume: above fullscreen windows too, like Windows 11. It takes the
    // mouse (drag, mute, wheel). The window touches the taskbar, the pill
    // rests 12 units above it.
    { "VolumeOsd", "vela-volume-osd", layerOverlay, bottom, 0, keysNone },
    // The whole output, transparent: a click outside the panel closes it.
    // The keyboard for Win+Z.
    { "SnapLayouts", "vela-snap-layouts", layerOverlay, fill, -1, keysOnDemand },
    // The same while dragging a window to the top: the compositor has the
    // pointer, the mouse and keyboard go through.
    { "SnapDragLayouts", "vela-snap-drag", layerOverlay, fill, -1, keysNone, {}, true },
    // The output's usable area, so the spaces in twelfths match the windows'.
    { "SnapAssist", "vela-snap-assist", layerTop, fill, 0, keysExclusive },
    // The distance from the left edge is decided by the button
    // (ShellController::placeAtLeft).
    // Spotlight (Win+Space): the centered search, near the top.
    { "Spotlight", "vela-spotlight", layerOverlay, LayerWindow::Anchors(LayerWindow::AnchorTop), -1, keysOnDemand, QMargins(0, 140, 0, 0) },
    { "TaskbarPreview", "vela-taskbar-preview", layerTop, bottomLeft, 0, keysNone },
    // Above the taskbar too, placed the same way; the mouse goes through.
    { "TaskbarTooltip", "vela-taskbar-tooltip", layerTop, bottomLeft, 0, keysNone, {}, true },
    // The hidden tray icons: the whole output, transparent, so a click
    // outside the flyout (the "^" button too) closes it. Esc.
    { "TrayOverflow", "vela-tray-overflow", layerOverlay, fill, -1, keysOnDemand },
    // The weather flyout: same idea as the hidden icons one.
    { "WeatherFlyout", "vela-weather", layerOverlay, fill, -1, keysOnDemand },
    { "FileDialogs", "vela-file-dialogs", layerTop, centered, 0, keysOnDemand },
    { "ClipboardPanel", "vela-clipboard", layerTop, bottomRight, 0, keysOnDemand },
};

// QML windows start invisible: they become layer-shell surfaces BEFORE
// they're shown.
QQuickWindow* loadPanel(QQmlApplicationEngine& engine, const Panel& panel)
{
    const qsizetype loaded = engine.rootObjects().size();
    engine.loadFromModule("Vela.Shell", panel.qml);
    if (engine.rootObjects().size() == loaded) {
        return nullptr;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    if (!window) {
        return nullptr;
    }
    LayerWindow* layer = LayerWindow::get(window);
    layer->setScope(QString::fromLatin1(panel.scope));
    layer->setLayer(panel.layer);
    layer->setAnchors(panel.anchors);
    layer->setExclusiveZone(panel.exclusiveZone);
    layer->setMargins(panel.margins);
    layer->setKeyboardInteractivity(panel.keyboard);
    if (panel.clickThrough) {
        window->setFlag(Qt::WindowTransparentForInput);
    }
    // Panels come and go: a hidden one gives its surface back. The taskbar
    // and the wallpapers (ScreenWindows) stay mapped all session.
    releaseHiddenPanel(window);
    return window;
}

// With the taskbar aligned left, Start opens at the bottom left.
void placeStartMenu(QQuickWindow* window, const QString& alignment, bool vertical, ShellController& shell, bool themeLeft)
{
    LayerWindow* layer = LayerWindow::get(window);
    // A theme with its Start button at the left (Windows 7's) opens it flush at the corner.
    const bool left = (alignment == QLatin1String("left") || themeLeft) && !vertical;
    layer->setAnchors(left ? bottomLeft : bottom);
    layer->setMargins(QMargins(left ? (themeLeft ? 0 : 12) : 0, 0, 0, 0));
    shell.followTaskbar(window, ShellController::FollowKind::Start); // the edge it was just put back from
}

} // namespace

int main(int argc, char* argv[])
{
    // "vela-shell --choose-source": the chooser xdg-desktop-portal-wlr
    // launches when an app wants to share the screen. It asks the already open
    // shell, which shows the choice, and prints the answer for the portal.
    if (argc > 1 && QByteArray(argv[1]) == "--choose-source") {
        QCoreApplication app(argc, argv);
        const QByteArray answer = ShellController::askRunningInstance("choose-source");
        if (!answer.isEmpty()) {
            std::printf("%s\n", answer.constData());
        }
        return 0;
    }

    // Text with FreeType and font hinting, like Plasma and Windows: crisper
    // than Qt Quick's default text (distance fields, no hinting), on the
    // output's real pixels even at fractional scales.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    vela::PortalFileDialogs portalDialogs;
    QGuiApplication app(argc, argv);
    portalDialogs.restore();
    QGuiApplication::setApplicationName(QStringLiteral("vela-shell"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    // Before anything else: the C++ models are born already translated too.
    QQmlApplicationEngine* qmlEngine = nullptr;
    AppModel* appModel = nullptr;
    DesktopModel* desktopModel = nullptr;
    vela::language::install(QStringLiteral("vela-shell"), [&qmlEngine, &appModel, &desktopModel] {
        if (appModel) {
            appModel->reload(); // app names in the new language (Name[en], Name[it])
        }
        if (desktopModel) {
            desktopModel->refresh(); // the Recycle Bin
        }
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });

    // "vela-shell --toggle-start": handy for shortcuts and scripts.
    const QStringList args = QGuiApplication::arguments();
    if (args.contains(QStringLiteral("--toggle-start"))) {
        return ShellController::sendToRunningInstance("toggle-start") ? 0 : 1;
    }
    if (ShellController::sendToRunningInstance("ping")) {
        qWarning("vela-shell: already running in this session");
        return 0;
    }

    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        qWarning("vela-shell: needs a Wayland session (current platform: %s)",
            qPrintable(QGuiApplication::platformName()));
        return 1;
    }

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    Config config;

    AppModel apps;
    appModel = &apps;
    apps.reload();

    ShellController shell;
    shell.setPreferredPrimaryOutput(config.primaryOutput());
    QObject::connect(&config, &Config::primaryOutputChanged, &shell,
        [&shell, &config] { shell.setPreferredPrimaryOutput(config.primaryOutput()); });
    Workspaces workspaces;
    QObject::connect(&shell, &ShellController::workspacesReceived, &workspaces, &Workspaces::update);
    workspaces.query();
    Accessibility accessibility;
    QObject::connect(&shell, &ShellController::accessibilityReceived, &accessibility, &Accessibility::update);
    accessibility.query();
    Network network;
    shell.listen();
    shell.sendWallpaperTint(config.wallpaper());
    QObject::connect(&config, &Config::wallpaperChanged, &shell, [&] { shell.sendWallpaperTint(config.wallpaper()); });
    // The mode to the compositor (acrylic tint, title bars) and, once
    // Vela.Controls is installed below, the accent (the snap preview).
    Appearance* appearance = nullptr;
    auto sendTheme = [&config, &appearance] {
        QByteArray line = "theme " + config.shellTheme().toLatin1() + ' ' + config.appTheme().toLatin1();
        if (appearance) {
            line += ' ' + appearance->accent().name(QColor::HexRgb).toLatin1();
        }
        ShellController::sendToCompositor(line);
    };
    sendTheme();
    QObject::connect(&config, &Config::themeChanged, &shell, sendTheme);
    // Auto-hide: windows may use the space the taskbar reserves.
    auto sendAutoHide = [&config] {
        ShellController::sendToCompositor(config.taskbarAutoHide() ? "taskbar-autohide yes" : "taskbar-autohide no");
    };
    sendAutoHide();
    QObject::connect(&config, &Config::taskbarChanged, &shell, sendAutoHide);
    // The edge: the panels it opens follow it (ShellController::followTaskbar).
    taskbarShell = &shell;
    shell.setTaskbarPosition(config.taskbarPosition());
    auto sendPosition = [&config, &shell] {
        shell.setTaskbarPosition(config.taskbarPosition());
        ShellController::sendToCompositor("taskbar-position " + config.taskbarPosition().toLatin1());
    };
    sendPosition();
    QObject::connect(&config, &Config::taskbarChanged, &shell, sendPosition);
    shell.setPanelsAtTop(config.dock());
    QObject::connect(&config, &Config::taskbarChanged, &shell, [&config, &shell] { shell.setPanelsAtTop(config.dock()); });
    shell.watchSleep();

    ForeignToplevelManager windows;
    TaskbarModel tasks(&apps, &windows);
    tasks.setCombine(config.taskbarCombine() != QLatin1String("never"));
    QObject::connect(&config, &Config::taskbarChanged, &tasks,
        [&config, &tasks] { tasks.setCombine(config.taskbarCombine() != QLatin1String("never")); });
    // Off by default: nothing is requested until the widget is switched on.
    Weather weather;
    auto applyWeather = [&config, &weather] {
        weather.setConfig(config.weather(), config.weatherCity(), config.weatherLat(), config.weatherLon(), config.weatherUnit());
    };
    QObject::connect(&config, &Config::taskbarChanged, &weather, applyWeather);
    applyWeather();
    WindowCapture capture;
    NotificationServer notifications;
    notifications.registerService();
    TransferMonitor transfers;
    QObject::connect(&transfers, &TransferMonitor::completed, &notifications,
        [&notifications](const QString& title, const QString& error) {
            const QString summary = error.isEmpty()
                ? QCoreApplication::translate("Shell", "File transfer complete")
                : QCoreApplication::translate("Shell", "File transfer interrupted");
            notifications.Notify(QStringLiteral("Vela Files"), 0,
                QStringLiteral("system-file-manager"), summary,
                error.isEmpty() ? title : error, {}, {}, 6000);
        });
    notifications.setDoNotDisturb(config.doNotDisturb());
    QObject::connect(&config, &Config::doNotDisturbChanged, &notifications, &NotificationServer::setDoNotDisturb);
    // Sharing the screen with capture-dnd=yes: popups held back (the center keeps them).
    auto silenceForSharing = [&] { notifications.setCaptureSilenced(shell.captureSilence()); };
    QObject::connect(&shell, &ShellController::screenCapturedChanged, &notifications, silenceForSharing);
    silenceForSharing();
    QObject::connect(&shell, &ShellController::fullscreenActiveChanged, &notifications,
        [&] { notifications.setFullscreenSilenced(shell.fullscreenActive()); });
    // Global shortcuts for apps (xdg-desktop-portal's GlobalShortcuts).
    GlobalShortcuts globalShortcuts;
    // The same portal's file chooser: Vela's Explorer.
    (new FileChooser(&globalShortcuts))->setProgram(velaFilesExecutable());
    globalShortcuts.registerService();
    QObject::connect(&globalShortcuts, &GlobalShortcuts::saved, &app, [] { ShellController::sendToCompositor("reload-config"); });
    QObject::connect(&shell, &ShellController::appShortcutEvent, &globalShortcuts, &GlobalShortcuts::keyEvent);
    TrayModel tray;
    tray.start();
    SystemActions system;
    system.setAppModel(&apps);
    // Use the QML caller's output; keyboard focus can change when a menu closes.
    QObject::connect(&system, &SystemActions::chooseAppRequested, &shell, &ShellController::openWithRequested);
    QObject::connect(&system, &SystemActions::chooseAppOnScreenRequested, &shell, &ShellController::openWithOnScreenRequested);
    QObject::connect(&shell, &ShellController::settingsRequested, &system, [&system] { system.trigger(QStringLiteral("settings")); });
    QObject::connect(&shell, &ShellController::filesRequested, &system, [&system] { system.trigger(QStringLiteral("files")); });
    JumpLists jumps(&apps);
    DesktopModel desktop(&apps);
    QObject::connect(&shell, &ShellController::desktopTransferUndoReceived,
        &desktop, &DesktopModel::recordExternalTransfer);
    QObject::connect(&shell, &ShellController::desktopTransferReadyReceived,
        &desktop, &DesktopModel::acknowledgeTransferStart);
    QObject::connect(&desktop, &DesktopModel::chooseAppRequested, &shell, &ShellController::openWithRequested);
    QObject::connect(&desktop, &DesktopModel::chooseAppOnScreenRequested, &shell, &ShellController::openWithOnScreenRequested);
    desktopModel = &desktop;
    ServiceMenus serviceMenus;
    FileProperties properties;
    FileActions fileActions;
    FileSearch fileSearch;
    Clipboard clipboard;
    Emoji emoji;
    QObject::connect(&shell, &ShellController::clipboardClearRequested, &clipboard, &Clipboard::clear);
    // History turned on or off from Settings (vela-shell.conf).
    QObject::connect(&config, &Config::clipboardChanged, &clipboard,
        [&config, &clipboard] { clipboard.setEnabled(config.clipboardHistory()); });
    // Bluetooth (docs/bluetooth.md §9.2, §9.3): the shell is Vela's UI, and
    // draws the questions Settings doesn't.
    vela::bluetooth::Bluetooth bluetooth(true);
    // Settings says itself that the PC is still visible, while it is open.
    BluetoothPrompts bluetoothPrompts(&bluetooth, &notifications, [&windows] {
        for (const ForeignToplevel* window : windows.windows()) {
            if (window->appId == QLatin1String("vela-settings")) {
                return true;
            }
        }
        return false;
    });
    QObject::connect(&windows, &ForeignToplevelManager::windowsChanged,
        &bluetoothPrompts, &BluetoothPrompts::settingsWindowsChanged);
    SystemStatus status(&bluetooth);
    Brightness brightness; // every output's, the compositor's (docs/brightness.md §9)
    // The sound (docs/audio.md): apps named after their .desktop file.
    vela::audio::Audio audio;
    audio.setAppResolver(audioAppResolver(apps));
    audio.setBluetooth(&bluetooth); // headsets' batteries
    // The mix follows what plays, unless someone chose otherwise (§8.1).
    audio.applyDefaultSampleRate();
    SoundKeys sound(&audio);
    QObject::connect(&shell, &ShellController::volumeKeyPressed, &sound, &SoundKeys::press);
    MediaKeys media;
    LauncherBadges badges;
    QObject::connect(&shell, &ShellController::mediaKeyPressed, &media, &MediaKeys::press);

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    // Theme, Style (the shell's mode, accent, icons) and Effects (the blur):
    // Vela.Controls, like the apps.
    appearance = vela::controls::install(engine, Appearance::Mode::Shell);
    appearance->watch();
    // A theme's blur-saturation= / blur-strength= / blur-luminosity= / blur-tint=
    // (Window Glass's vivid
    // acrylic) to the compositor, which draws the blur.
    auto sendBlurLook = [appearance] {
        const QVariantMap theme = appearance->theme();
        ShellController::sendToCompositor("blur-look " + QByteArray::number(theme.value(QStringLiteral("blur-saturation"), 1.0).toDouble())
            + ' ' + QByteArray::number(theme.value(QStringLiteral("blur-strength"), 1.0).toDouble())
            + ' ' + QByteArray::number(theme.value(QStringLiteral("blur-luminosity"), -1.0).toDouble())
            + ' ' + QByteArray::number(theme.value(QStringLiteral("blur-tint"), -1.0).toDouble()));
    };
    // Each panel's own blur look, from its own style: choosing another taskbar
    // style must not change the Start menu's or quick settings'.
    auto sendPanelLooks = [appearance] {
        static const struct {
            const char* dir;
            const char* scopes[2];
        } panels[] = {
            { "taskbar/", { "vela-taskbar", nullptr } },
            { "startmenu/", { "vela-start-menu", nullptr } },
            { "notifications/", { "vela-quick-settings", "vela-notification-center" } },
        };
        for (const auto& panel : panels) {
            const QVariantMap own = appearance->themeFor(QString::fromLatin1(panel.dir));
            const auto number = [&own](const char* key, double fallback) { return own.value(QString::fromLatin1(key), fallback).toDouble(); };
            for (const char* scope : panel.scopes) {
                if (scope) {
                    ShellController::sendToCompositor(QByteArray("blur-look-for ") + scope + ' '
                        + QByteArray::number(number("blur-saturation", 1.0)) + ' ' + QByteArray::number(number("blur-strength", 1.0)) + ' '
                        + QByteArray::number(number("blur-luminosity", -1.0)) + ' ' + QByteArray::number(number("blur-tint", -1.0)));
                }
            }
        }
    };
    sendBlurLook();
    sendPanelLooks();
    QObject::connect(appearance, &Appearance::changed, &shell, sendBlurLook);
    QObject::connect(appearance, &Appearance::changed, &shell, sendPanelLooks);
    QObject::connect(appearance, &Appearance::changed, &shell, sendTheme);
    sendTheme();
    engine.addImageProvider(QStringLiteral("fileicon"), new IconProvider(32));
    engine.addImageProvider(QStringLiteral("wallpaper"), new WallpaperProvider);
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider(&capture));
    engine.addImageProvider(QStringLiteral("filethumb"), new FileThumbnailProvider);
    engine.addImageProvider(QStringLiteral("notification"), new NotificationImageProvider(&notifications));
    engine.addImageProvider(QStringLiteral("tray"), new TrayImageProvider(&tray));
    engine.addImageProvider(QStringLiteral("clipboard"), new ClipboardImageProvider(&clipboard));
    Snip snip(&engine, &capture, &clipboard, &notifications);
    QObject::connect(&shell, &ShellController::windowScreenshotRequested, &snip, &Snip::screenshotWindow);
    engine.addImageProvider(QStringLiteral("snip"), new SnipImageProvider(&snip));
    QObject::connect(&shell, &ShellController::snipRequested, &snip, &Snip::start);
    QObject::connect(&shell, &ShellController::snipTextRequested, &snip, &Snip::startText);
    QObject::connect(&shell, &ShellController::snipLensRequested, &snip, &Snip::startLens);
    engine.rootContext()->setContextProperty(QStringLiteral("Apps"), &apps);
    engine.rootContext()->setContextProperty(QStringLiteral("Shell"), &shell);
    engine.rootContext()->setContextProperty(QStringLiteral("Config"), &config);
    engine.rootContext()->setContextProperty(QStringLiteral("Desktops"), &workspaces);
    engine.rootContext()->setContextProperty(QStringLiteral("Tasks"), &tasks);
    engine.rootContext()->setContextProperty(QStringLiteral("Transfers"), &transfers);
    engine.rootContext()->setContextProperty(QStringLiteral("Capture"), &capture);
    engine.rootContext()->setContextProperty(QStringLiteral("Notifications"), &notifications);
    engine.rootContext()->setContextProperty(QStringLiteral("GlobalShortcuts"), &globalShortcuts);
    engine.rootContext()->setContextProperty(QStringLiteral("Tray"), &tray);
    engine.rootContext()->setContextProperty(QStringLiteral("System"), &system);
    engine.rootContext()->setContextProperty(QStringLiteral("Jumps"), &jumps);
    engine.rootContext()->setContextProperty(QStringLiteral("Desktop"), &desktop);
    engine.rootContext()->setContextProperty(QStringLiteral("ServiceMenus"), &serviceMenus);
    engine.rootContext()->setContextProperty(QStringLiteral("Properties"), &properties);
    engine.rootContext()->setContextProperty(QStringLiteral("FileActions"), &fileActions);
    engine.rootContext()->setContextProperty(QStringLiteral("FileSearch"), &fileSearch);
    engine.rootContext()->setContextProperty(QStringLiteral("Clip"), &clipboard);
    engine.rootContext()->setContextProperty(QStringLiteral("Emoji"), &emoji);
    engine.rootContext()->setContextProperty(QStringLiteral("Snip"), &snip);
    engine.rootContext()->setContextProperty(QStringLiteral("Status"), &status);
    qmlRegisterSingletonInstance("Vela.Bluetooth", 1, 0, "Bluetooth", &bluetooth);
    engine.rootContext()->setContextProperty(QStringLiteral("BluetoothPrompts"), &bluetoothPrompts);
    engine.rootContext()->setContextProperty(QStringLiteral("Brightness"), &brightness);
    engine.rootContext()->setContextProperty(QStringLiteral("Audio"), &audio);
    engine.rootContext()->setContextProperty(QStringLiteral("Sound"), &sound);
    engine.rootContext()->setContextProperty(QStringLiteral("Media"), &media);
    engine.rootContext()->setContextProperty(QStringLiteral("Badges"), &badges);
    // The apps' level bars in the quick settings mixer (docs/audio.md §7.1).
    qmlRegisterType<vela::audio::LevelMeter>("Vela.Audio", 1, 0, "LevelMeter");
    engine.rootContext()->setContextProperty(QStringLiteral("Access"), &accessibility);
    engine.rootContext()->setContextProperty(QStringLiteral("Network"), &network);
    engine.rootContext()->setContextProperty(QStringLiteral("Weather"), &weather);
    SystemStats systemStats;
    engine.rootContext()->setContextProperty(QStringLiteral("SysStats"), &systemStats);

    QQuickWindow* startMenu = nullptr;
    QQuickWindow* notificationPopups = nullptr;
    for (const Panel& panel : panels) {
        QQuickWindow* window = loadPanel(engine, panel);
        if (!window) {
            qCritical("vela-shell: can't load the QML interface (%s)", panel.qml);
            return 1;
        }
        if (qstrcmp(panel.qml, "StartMenu") == 0) {
            startMenu = window;
        } else if (qstrcmp(panel.qml, "NotificationPopups") == 0) {
            notificationPopups = window;
        }
        // The ones at the bottom edge follow the taskbar's; the hot corner
        // stays at the top left, whichever edge the taskbar is on.
        if (panel.anchors.testFlag(LayerWindow::AnchorBottom) && !panel.anchors.testFlag(LayerWindow::AnchorTop)) {
            const bool beside = qstrcmp(panel.qml, "TaskbarPreview") == 0 || qstrcmp(panel.qml, "TaskbarTooltip") == 0;
            shell.followTaskbar(window, beside ? ShellController::FollowKind::Beside : ShellController::FollowKind::Panel);
        }
    }
    const auto themeLeft = [appearance] { return appearance->theme().value(QStringLiteral("taskbar-align-left")).toDouble() != 0; };
    placeStartMenu(startMenu, config.taskbarAlignment(), config.taskbarVertical(), shell, themeLeft());
    QObject::connect(&config, &Config::taskbarChanged, startMenu, [&config, startMenu, &shell, themeLeft] {
        placeStartMenu(startMenu, config.taskbarAlignment(), config.taskbarVertical(), shell, themeLeft());
    });
    QObject::connect(appearance, &Appearance::changed, startMenu, [&config, startMenu, &shell, themeLeft] {
        placeStartMenu(startMenu, config.taskbarAlignment(), config.taskbarVertical(), shell, themeLeft());
    });
    // Wallpapers on every output; the taskbar too, like in Windows (or only on
    // the main one, if the user wants that).
    ScreenWindows wallpapers(&engine, "Wallpaper", setupWallpaper);
    ScreenWindows taskbars(&engine, "Taskbar", setupTaskbar);
    wallpapers.setPreferredPrimary(config.primaryOutput());
    taskbars.setPreferredPrimary(config.primaryOutput());
    QObject::connect(&config, &Config::primaryOutputChanged, &wallpapers,
        [&] { wallpapers.setPreferredPrimary(config.primaryOutput()); });
    QObject::connect(&config, &Config::primaryOutputChanged, &taskbars,
        [&] { taskbars.setPreferredPrimary(config.primaryOutput()); });
    taskbars.setPrimaryOnly(!config.taskbarAllScreens());
    QObject::connect(&config, &Config::taskbarChanged, &taskbars,
        [&config, &taskbars] { taskbars.setPrimaryOnly(!config.taskbarAllScreens()); });
    if (!wallpapers.start() || !taskbars.start()) {
        return 1;
    }
    // The top panel exists only with the dock style.
    std::unique_ptr<ScreenWindows> topbars;
    auto syncTopBar = [&] {
        if (config.dock() && !topbars) {
            topbars = std::make_unique<ScreenWindows>(&engine, "TopBar", setupTopBar);
            topbars->setPrimaryOnly(!config.taskbarAllScreens());
            topbars->start();
        } else if (!config.dock() && topbars) {
            topbars.reset();
        }
        if (topbars) {
            topbars->setPrimaryOnly(!config.taskbarAllScreens());
        }
    };
    QObject::connect(&config, &Config::taskbarChanged, &app, syncTopBar);
    syncTopBar();

    // An existing layer-shell surface cannot change wl_output while mapped.
    // Recreate only the surface; keep notification models and timers intact.
    QPointer<QScreen> notificationScreen;
    const auto refreshNotifications = [&config, &notifications, notificationPopups, &notificationScreen] {
        QScreen* target = vela::primaryOutputScreen(config.primaryOutput());
        if (!target) {
            notificationPopups->hide();
            return;
        }
        if (notificationScreen != target) {
            notificationPopups->hide();
            notificationPopups->destroy();
            notificationPopups->setScreen(target);
            LayerWindow::get(notificationPopups)->setScreen(target);
            notificationScreen = target;
        }
        if (notifications.count() > 0) notificationPopups->show();
        else notificationPopups->hide();
    };
    QObject::connect(&notifications, &NotificationServer::countChanged, notificationPopups, refreshNotifications);
    QObject::connect(&config, &Config::primaryOutputChanged, notificationPopups, refreshNotifications);
    const auto resyncNotifications = [notificationPopups, refreshNotifications] {
        QTimer::singleShot(0, notificationPopups, refreshNotifications);
    };
    QObject::connect(&app, &QGuiApplication::screenAdded, notificationPopups, resyncNotifications);
    QObject::connect(&app, &QGuiApplication::screenRemoved, notificationPopups, resyncNotifications);
    QObject::connect(&app, &QGuiApplication::primaryScreenChanged, notificationPopups, resyncNotifications);
    refreshNotifications();

    return app.exec();
}
