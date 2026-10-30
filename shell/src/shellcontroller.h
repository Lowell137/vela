// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QDBusUnixFileDescriptor>
#include <QLocalServer>
#include <QLocalSocket>
#include <LayerShellQt/Window>
#include <QList>
#include <QMargins>
#include <QPointer>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QStringList>

// The point of contact between the shell and the rest of the system:
// - receives commands from the compositor on a Unix socket (such as
//   "toggle-start" when you press Super);
// - exposes small session details to QML.
class QWindow;

class ShellController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString userInitial READ userInitial CONSTANT)
    Q_PROPERTY(QString userPicture READ userPicture CONSTANT)
    // The Start menu state, shared by the menu window and the taskbar.
    Q_PROPERTY(bool startMenuOpen READ startMenuOpen WRITE setStartMenuOpen NOTIFY startMenuOpenChanged)
    // The apps pinned to Start (.desktop ids), in the chosen order.
    Q_PROPERTY(QStringList startPins READ startPins NOTIFY startPinsChanged)
    // The main output's name (panels asked for from the keyboard open there).
    Q_PROPERTY(QString primaryScreen READ primaryScreen NOTIFY primaryScreenChanged)
    // The screen is being shared or recorded (the taskbar's indicator): the
    // compositor's "capture on" and "capture off".
    Q_PROPERTY(bool screenCaptured READ screenCaptured NOTIFY screenCapturedChanged)
    // "capture on dnd": the compositor also asks for notifications to be
    // silenced while it lasts (capture-dnd=yes).
    Q_PROPERTY(bool captureSilence READ captureSilence NOTIFY screenCapturedChanged)
    // While captured: how many windows are left out of it ("capture on
    // hidden N", docs/renderer.md §15).
    Q_PROPERTY(int captureHiddenWindows READ captureHiddenWindows NOTIFY screenCapturedChanged)
    // "fullscreen on" / "fullscreen off": a visible window is full screen and
    // fullscreen-dnd=yes (the compositor only sends it then).
    Q_PROPERTY(bool fullscreenActive READ fullscreenActive NOTIFY fullscreenActiveChanged)

public:
    explicit ShellController(QObject* parent = nullptr);

    // The socket path, tied to the current Wayland session.
    static QString socketPath();
    // Sends a command to an already running shell. false if there is none.
    static bool sendToRunningInstance(const QByteArray& command);
    // A command with an answer (waits as long as needed): "choose-source".
    static QByteArray askRunningInstance(const QByteArray& command);

    // Screen sharing (xdg-desktop-portal-wlr): the answer for the portal,
    // "Monitor: <output>", "Window: <identifier>", or empty if the user
    // cancels.
    Q_INVOKABLE void chooseSource(const QString& answer);

    bool listen();
    // A command to the compositor, on its socket (such as "lock").
    static bool sendToCompositor(const QByteArray& command);
    // Shift held right now (the compositor knows: the keyboard may belong to
    // another app while the taskbar is clicked).
    Q_INVOKABLE bool shiftHeld() const;
    // A shell window anchored at the bottom left (the taskbar previews):
    // `left` pixels from the output's left edge.
    Q_INVOKABLE void placeAtLeft(QWindow* window, int left);
    // Slides the taskbar out of its edge by `margin` px (auto-hide).
    Q_INVOKABLE void setBottomMargin(QWindow* window, int margin);
    enum class Position { Bottom, Top, Left, Right };
    // How a window follows the taskbar's edge (taskbar/position). Panel: the
    // bottom corner it has (bottom, or the side of the taskbar). Start: at the
    // top, next to a vertical taskbar. Beside: tooltips and previews, placed
    // along a vertical taskbar with placeAtTop. Taskbar: the taskbar itself.
    enum class FollowKind { Panel, Start, Beside, Taskbar };
    struct Follower {
        QPointer<QWindow> window;
        FollowKind kind;
        QFlags<LayerShellQt::Window::Anchor> anchors; // at the bottom taskbar
        QMargins margins;
    };
    void setTaskbarPosition(const QString& position);
    // The dock style: the panels (quick settings...) open from the top panel.
    void setPanelsAtTop(bool top);
    // Call it with the window's anchors and margins as they are at the bottom
    // taskbar, and again after changing them by hand.
    void followTaskbar(QWindow* window, FollowKind kind = FollowKind::Panel);
    // A window along a vertical taskbar: `top` px from the top of the output.
    Q_INVOKABLE void placeAtTop(QWindow* window, int top);
    // A (hidden) shell panel on output `name`: it appears there the next time
    // it opens.
    Q_INVOKABLE void placeOnScreen(QWindow* window, const QString& name);
    // Puts `window` (hidden) with its corner next to a point of the desktop
    // (layout coordinates), on that point's output and inside it; with a
    // negative `x`, back at the bottom right of its output.
    // Where the corner went, on the output; (-1, -1) at the bottom right.
    Q_INVOKABLE QPoint placeAtPoint(QWindow* window, int x, int y);
    // Alt+Tab: a click on preview `index` (the compositor switches).
    Q_INVOKABLE void pickSwitcher(int index) { sendToCompositor("switcher-pick " + QByteArray::number(index)); }
    Q_INVOKABLE void closeSwitcherWindow(int index) { sendToCompositor("switcher-close " + QByteArray::number(index)); }

    QString userName() const;
    QString userPicture() const;
    QString primaryScreen() const;
    void setPreferredPrimaryOutput(const QString& name);
    QString userInitial() const;

    bool screenCaptured() const { return m_screenCaptured; }
    bool fullscreenActive() const { return m_fullscreenActive; }
    bool captureSilence() const { return m_screenCaptured && m_captureDnd; }
    int captureHiddenWindows() const { return m_screenCaptured ? m_captureHidden : 0; }
    bool startMenuOpen() const { return m_startMenuOpen; }
    void setStartMenuOpen(bool open)
    {
        if (open != m_startMenuOpen) {
            m_startMenuOpen = open;
            emit startMenuOpenChanged();
        }
    }

    // A Vela update is waiting (found by the last check, not installed yet):
    // the power menu offers Update and shut down / restart.
    Q_INVOKABLE bool updatePending() const;
    // Installs it, then shuts down or restarts (vela-settings --install-update).
    Q_INVOKABLE void updateThen(bool reboot);
    Q_INVOKABLE void toggleStartMenu() { emit toggleStartRequested(); }

    QStringList startPins() const { return m_startPins; }
    Q_INVOKABLE void pinToStart(const QString& id);
    Q_INVOKABLE void unpinFromStart(const QString& id);
    Q_INVOKABLE void moveStartPinToFront(const QString& id);

    // The window menu: an action (restore, move, resize, minimize, maximize,
    // close) on the window with that identifier, or on "active".
    Q_INVOKABLE void windowAction(const QString& window, const QString& action);
    // Signs out: asks the compositor to close (apps close with it). Doesn't
    // shut down or restart the computer.
    Q_INVOKABLE void logout();
    // Relaunches only vela-shell (the compositor and apps stay).
    Q_INVOKABLE void restartShell();
    Q_INVOKABLE void lock(); // Win+L from the shell
    // The wallpaper's average color (the Mica tint, docs/renderer.md §9.4),
    // also left in the cache for Vela's apps; invalid if it can't be read.
    void sendWallpaperTint(const QString& wallpaper) const;
    // Locks the screen before the computer suspends.
    void watchSleep();
    // The computer, through systemd-logind (polkit asks for the password if
    // needed).
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE bool canSuspend() const;

signals:
    void toggleStartRequested();
    // Alt+Tab, driven by the compositor: windows (ext-foreign-toplevel
    // identifiers) in order of recent use and the selected one.
    void switcherShown(int selected, const QStringList& windows);
    void switcherSelected(int selected);
    void switcherHidden();
    void startMenuOpenChanged();
    void startPinsChanged();
    void primaryScreenChanged();
    void screenCapturedChanged();
    void fullscreenActiveChanged();
    void chooseSourceRequested();
    void chooseSourceCancelled();
    // Windows shortcuts arriving from the compositor.
    void winXRequested(); // Win+X
    void runRequested(); // Win+R
    void appShortcutEvent(int number, bool pressed); // an app's global shortcut (globalshortcuts.h)
    void spotlightRequested(); // Win+Space
    void showDesktopRequested(); // Win+D
    void quickSettingsRequested(); // Win+A
    void soundOutputRequested(); // Win+Ctrl+V: quick settings' sound page
    void notificationCenterRequested(); // Win+N
    void settingsRequested(); // Win+I
    void filesRequested(); // Win+E
    void taskViewRequested(); // Win+Tab
    // The top-left corner (interact.c): "near", "away" or "fire", on which output,
    // and how close to it while near (0..1).
    void hotCornerChanged(const QString& state, const QString& output, double progress);
    void workspacesReceived(const QByteArray& json); // the state of the virtual desktops
    // Snap layouts below the Maximize button (Win+Z): x, y on the output.
    void snapLayoutsRequested(const QString& window, const QString& output, int x, int y, bool keyboard);
    // Snap layouts while a window is dragged to the top (snap.c): "show" (name:
    // output, json: where, how large, the layouts), "hover" (a: layout, b:
    // zone, -1: none), "hide", "drop" (name: window, a: layout).
    void snapDragChanged(const QString& state, const QString& name, int a, int b, const QString& json);
    // After a snap: the windows to offer for the free spaces (JSON).
    void snapAssistRequested(const QString& json);
    void propertiesRequested(const QStringList& paths); // from Explorer
    void shareRequested(const QStringList& paths); // "Share", from Explorer
    void openWithRequested(const QString& path); // "Choose another app", from Explorer
    void newShortcutRequested(const QString& folder); // "New > Shortcut", from Explorer
    // File Explorer passes its originating QScreen with requests for shell
    // layer-shell dialogs; older clients may still send only file paths.
    void propertiesOnScreenRequested(const QStringList& paths, const QString& screen);
    void shareOnScreenRequested(const QStringList& paths, const QString& screen);
    void openWithOnScreenRequested(const QString& path, const QString& screen);
    void newShortcutOnScreenRequested(const QString& folder, const QString& screen);
    void desktopTransferUndoReceived(const QByteArray& json);
    void desktopTransferReadyReceived(qint64 pid);
    void accessibilityReceived(const QByteArray& json); // from the compositor
    // Win+V ("clipboard") and Win+. ("emoji"); the pointer, in layout
    // coordinates, or -1 when it isn't known.
    void clipboardRequested(const QString& tab, int x, int y);
    void snipRequested(); // Win+Shift+S, Print
    void snipTextRequested(); // Win+Shift+T
    void snipLensRequested(); // Win+Shift+G
    void clipboardClearRequested(); // from Settings
    // The keyboard's volume keys: "up", "down", "mute", "mic-mute".
    void volumeKeyPressed(const QString& key);
    // The keyboard's media keys: "play-pause", "pause", "stop", "next",
    // "previous".
    void mediaKeyPressed(const QString& key);
    // The window menu (right click on the title bar, Alt+Space): on which
    // output and where, in output coordinates.
    void windowScreenshotRequested(const QString& window);
    void windowMenuRequested(const QString& window, const QString& output, int x, int y, bool maximized,
        bool resizable, bool keyboard, bool onTop, bool captureHidden);

private Q_SLOTS:
    void onPrepareForSleep(bool starting);

private:
    QString m_preferredPrimaryOutput;
    void handleCommand(const QByteArray& command);
    void takeSleepDelay();
    void startChooser(QLocalSocket* socket);
    void refreshScreenCastPortal();
    QPointer<QLocalSocket> m_chooser; // the portal waiting for the choice
    void saveStartPins();
    QDBusUnixFileDescriptor m_sleepDelay; // while it's open, logind waits before suspending

    QLocalServer m_server;
    bool m_startMenuOpen = false;
    Position m_position = Position::Bottom;
    bool m_panelsTop = false;
    QList<Follower> m_followers;
    bool m_screenCaptured = false;
    bool m_captureDnd = false;
    int m_captureHidden = 0;
    bool m_fullscreenActive = false;
    QStringList m_startPins;
    QTimer m_updateCheck; // vela-settings --check-updates now and then
};
