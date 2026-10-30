// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "defaults.h"

#include <QColor>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QObject>
#include <QStringList>
#include <QVariantList>

// The user's choices, in the two files holding them:
// ~/.config/Vela/vela-shell.conf for the shell (wallpaper, accent, mode,
// taskbar, notifications), which the shell rereads as soon as it changes;
// ~/.config/vela/vela.conf for the compositor (inactivity, keyboard, night
// light, accessibility, tearing), which rereads it on the "reload-config"
// command. The compositor writes vela.conf too (quick settings turn on night
// light, filters and sticky keys): only the changed keys are rewritten, on the
// file just reread, and it's watched. Also, the apps' light or dark mode goes
// to KDE (colors, icons) and GTK (gsettings).
class Preferences : public QObject {
    Q_OBJECT
    // --- Personalization ---
    Q_PROPERTY(QString wallpaper READ wallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(QStringList recentWallpapers READ recentWallpapers NOTIFY wallpaperChanged)
    Q_PROPERTY(QStringList systemWallpapers READ systemWallpapers CONSTANT)
    Q_PROPERTY(QString defaultWallpaper READ defaultWallpaper CONSTANT) // Vela's
    // A folder of the user's own pictures, shown under the recent ones.
    Q_PROPERTY(QString wallpaperFolder READ wallpaperFolder NOTIFY wallpaperFolderChanged)
    Q_PROPERTY(QStringList folderWallpapers READ folderWallpapers NOTIFY wallpaperFolderChanged)
    // "name", "size" (largest first), "newest" or "oldest".
    Q_PROPERTY(QString wallpaperFolderSort READ wallpaperFolderSort WRITE setWallpaperFolderSort
            NOTIFY wallpaperFolderChanged)
    // The accent follows the wallpaper's colors (set again on each wallpaper).
    Q_PROPERTY(bool accentFromWallpaper READ accentFromWallpaper WRITE setAccentFromWallpaper NOTIFY accentChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(QString appTheme READ appTheme WRITE setAppTheme NOTIFY appThemeChanged)
    // Vela's mode (taskbar, menus, panels): "dark" or "light".
    Q_PROPERTY(QString shellTheme READ shellTheme WRITE setShellTheme NOTIFY appThemeChanged)
    // Settings itself is light (it follows the apps' mode).
    Q_PROPERTY(bool light READ light NOTIFY appThemeChanged)
    Q_PROPERTY(QString taskbarAlignment READ taskbarAlignment WRITE setTaskbarAlignment NOTIFY taskbarChanged)
    Q_PROPERTY(QString taskbarStyle READ taskbarStyle WRITE setTaskbarStyle NOTIFY taskbarChanged)
    Q_PROPERTY(QString taskbarPosition READ taskbarPosition WRITE setTaskbarPosition NOTIFY taskbarChanged)
    Q_PROPERTY(bool endTask READ endTask WRITE setEndTask NOTIFY taskbarChanged)
    Q_PROPERTY(bool portalAutofix READ portalAutofix WRITE setPortalAutofix NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskView READ taskView WRITE setTaskView NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarWindowBounce READ taskbarWindowBounce WRITE setTaskbarWindowBounce NOTIFY taskbarChanged)
    // The apps' tray icons behind the "^" button (taskbar/trayOverflow).
    Q_PROPERTY(bool trayOverflow READ trayOverflow WRITE setTrayOverflow NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarAllDesktops READ taskbarAllDesktops WRITE setTaskbarAllDesktops NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarAllScreens READ taskbarAllScreens WRITE setTaskbarAllScreens NOTIFY taskbarChanged)
    // taskbar/autoHide, and taskbar/combine: "always" or "never".
    Q_PROPERTY(bool taskbarAutoHide READ taskbarAutoHide WRITE setTaskbarAutoHide NOTIFY taskbarChanged)
    Q_PROPERTY(QString taskbarCombine READ taskbarCombine WRITE setTaskbarCombine NOTIFY taskbarChanged)
    // The taskbar's weather (taskbar/weather, weather-city, weather-lat,
    // weather-lon, weather-unit "c"|"f"). Off by default; the place is chosen
    // with setWeatherPlace from a city search, never guessed.
    Q_PROPERTY(bool weather READ weather WRITE setWeather NOTIFY taskbarChanged)
    Q_PROPERTY(QString weatherCity READ weatherCity NOTIFY taskbarChanged)
    Q_PROPERTY(QString weatherUnit READ weatherUnit WRITE setWeatherUnit NOTIFY taskbarChanged)
    // taskbar/weather-system: the weather flyout's system cards (graphics card, processor, memory, disk).
    Q_PROPERTY(bool weatherSystemInfo READ weatherSystemInfo WRITE setWeatherSystemInfo NOTIFY taskbarChanged)
    // icon-theme= in vela.conf: an icon theme's directory name, "" for the
    // automatic one (Breeze). The apps apply it themselves (applyIconTheme).
    Q_PROPERTY(QString iconTheme READ iconTheme WRITE setIconTheme NOTIFY iconThemeChanged)
    // theme= in vela.conf: a theme's file name without .conf (themes/), "" for Vela's own.
    Q_PROPERTY(QString visualTheme READ visualTheme WRITE setVisualTheme NOTIFY visualThemeChanged)
    // taskbar-style=, startmenu-style=, notifications-style= in vela.conf: a
    // surface's own style (themes/<surface>/), by surface; "" for the theme's.
    Q_PROPERTY(QVariantMap surfaceStyles READ surfaceStyles NOTIFY visualThemeChanged)
    // taskbar-backdrop= in vela.conf: a floating taskbar's strip blurred under maximized windows.
    Q_PROPERTY(bool taskbarBackdrop READ taskbarBackdrop WRITE setTaskbarBackdrop NOTIFY visualThemeChanged)
    // quicksettings-slider-press-zoom= in vela.conf: a slider's knob zooms a
    // little, with a bounce, while it's held.
    Q_PROPERTY(bool sliderPressZoom READ sliderPressZoom WRITE setSliderPressZoom NOTIFY visualThemeChanged)
    // taskbar-activate-hop= in vela.conf: the button of an open app hops when it is clicked.
    Q_PROPERTY(bool activateHop READ activateHop WRITE setActivateHop NOTIFY visualThemeChanged)
    // Personalization > Animations (animations=, animation-speed= in percent, nav-pill-animation=,
    // files-pill-animation=, snap-drag-peek= in vela.conf).
    Q_PROPERTY(bool animations READ animations WRITE setAnimations NOTIFY animationsChanged)
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationsChanged)
    Q_PROPERTY(bool navPillAnimation READ navPillAnimation WRITE setNavPillAnimation NOTIFY animationsChanged)
    Q_PROPERTY(bool filesPillAnimation READ filesPillAnimation WRITE setFilesPillAnimation NOTIFY animationsChanged)
    Q_PROPERTY(bool snapDragPeek READ snapDragPeek WRITE setSnapDragPeek NOTIFY animationsChanged)
    // Light by day, dark by night: the hours ("07:00", "19:00") the shell's
    // timer applies by running `vela-settings --apply-theme-schedule`.
    Q_PROPERTY(bool themeSchedule READ themeSchedule WRITE setThemeSchedule NOTIFY themeScheduleChanged)
    Q_PROPERTY(QString themeLightFrom READ themeLightFrom WRITE setThemeLightFrom NOTIFY themeScheduleChanged)
    Q_PROPERTY(QString themeDarkFrom READ themeDarkFrom WRITE setThemeDarkFrom NOTIFY themeScheduleChanged)
    // cursor-theme= and cursor-size= in vela.conf: a cursor theme's directory
    // name ("": $XCURSOR_THEME) and its size in pixels. The compositor applies
    // them and passes them on to the apps it starts.
    Q_PROPERTY(QString cursorTheme READ cursorTheme WRITE setCursorTheme NOTIFY cursorChanged)
    Q_PROPERTY(int cursorSize READ cursorSize WRITE setCursorSize NOTIFY cursorChanged)
    // font-family= in vela.conf: the text font of the apps ("": Vela's own).
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    // window-blur*= in vela.conf: blur behind windows that don't ask for it,
    // for all of them or only the included app ids; the excluded ones never.
    Q_PROPERTY(bool windowBlur READ windowBlur WRITE setWindowBlur NOTIFY windowBlurChanged)
    Q_PROPERTY(bool windowBlurAll READ windowBlurAll WRITE setWindowBlurAll NOTIFY windowBlurChanged)
    Q_PROPERTY(QStringList windowBlurInclude READ windowBlurInclude WRITE setWindowBlurInclude NOTIFY windowBlurChanged)
    // window-rule= lines, "app|key=value,...": repeatable, so saved apart.
    Q_PROPERTY(QStringList windowRules READ windowRules WRITE setWindowRules NOTIFY windowRulesChanged)
    Q_PROPERTY(QStringList windowBlurExclude READ windowBlurExclude WRITE setWindowBlurExclude NOTIFY windowBlurChanged)
    // window-opacity= (percent, 30-100): the same windows drawn translucent.
    Q_PROPERTY(int windowOpacity READ windowOpacity WRITE setWindowOpacity NOTIFY windowBlurChanged)
    // window-blur-strength= (percent, 50-300) and window-blur-app= lines
    // ("app|strength=N,opacity=N"; 0: the general value), kept as [{app, strength, opacity}].
    Q_PROPERTY(int windowBlurStrength READ windowBlurStrength WRITE setWindowBlurStrength NOTIFY windowBlurChanged)
    Q_PROPERTY(QVariantList windowBlurApps READ windowBlurApps NOTIFY windowBlurChanged)
    // taskbar-/quicksettings- blur (50-300) and opacity (0-200) in percent of the theme's.
    Q_PROPERTY(QVariantMap panelLook READ panelLook NOTIFY windowBlurChanged)
    Q_PROPERTY(bool showDesktopCorner READ showDesktopCorner WRITE setShowDesktopCorner NOTIFY taskbarChanged)
    // start/solid (0-100): the Start menu's background, more solid than the theme.
    Q_PROPERTY(int startSolid READ startSolid WRITE setStartSolid NOTIFY taskbarChanged)
    // --- Notifications ---
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    // --- Power ---
    Q_PROPERTY(int screenOffMinutes READ screenOffMinutes WRITE setScreenOffMinutes NOTIFY idleChanged)
    Q_PROPERTY(bool lockOnIdle READ lockOnIdle WRITE setLockOnIdle NOTIFY idleChanged)
    // Vela's language (language= in vela.conf): "", "it", "en" or "tr". Empty: like
    // the system. The apps apply it themselves (shell/src/language.h).
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // --- Keyboard ---
    // [{layout, variant}] in the chosen order; the first is the starting one.
    Q_PROPERTY(QVariantList keyboardLayouts READ keyboardLayouts NOTIFY keyboardChanged)
    Q_PROPERTY(int repeatDelay READ repeatDelay WRITE setRepeatDelay NOTIFY keyboardChanged)
    Q_PROPERTY(int repeatRate READ repeatRate WRITE setRepeatRate NOTIFY keyboardChanged)
    // --- Display: night light and games ---
    Q_PROPERTY(bool nightLight READ nightLight WRITE setNightLight NOTIFY nightLightChanged)
    Q_PROPERTY(int nightStrength READ nightStrength WRITE setNightStrength NOTIFY nightLightChanged)
    Q_PROPERTY(bool nightNvidiaFix READ nightNvidiaFix WRITE setNightNvidiaFix NOTIFY nightLightChanged)
    // "no", "sunset" (from sunset to sunrise) or "hours"
    Q_PROPERTY(QString nightSchedule READ nightSchedule WRITE setNightSchedule NOTIFY nightLightChanged)
    Q_PROPERTY(QString nightFrom READ nightFrom WRITE setNightFrom NOTIFY nightLightChanged) // "21:00"
    Q_PROPERTY(QString nightTo READ nightTo WRITE setNightTo NOTIFY nightLightChanged)
    // Today's sun times (computed by the compositor), empty if unknown.
    Q_PROPERTY(QString sunset READ sunset NOTIFY nightLightChanged)
    Q_PROPERTY(QString sunrise READ sunrise NOTIFY nightLightChanged)
    Q_PROPERTY(bool tearing READ tearing WRITE setTearing NOTIFY accessibilityChanged)
    // Variable refresh rate: "no", "games" (fullscreen apps), "always".
    Q_PROPERTY(QString vrr READ vrr WRITE setVrr NOTIFY accessibilityChanged)
    Q_PROPERTY(QString directScanout READ directScanout WRITE setDirectScanout NOTIFY accessibilityChanged)
    Q_PROPERTY(bool scanoutOverridden READ scanoutOverridden NOTIFY accessibilityChanged)
    // --- Mouse and touchpad (vela.conf) ---
    Q_PROPERTY(int mouseSpeed READ mouseSpeed WRITE setMouseSpeed NOTIFY inputChanged) // 1-20
    Q_PROPERTY(bool mousePrecision READ mousePrecision WRITE setMousePrecision NOTIFY inputChanged)
    Q_PROPERTY(bool mouseLeftHanded READ mouseLeftHanded WRITE setMouseLeftHanded NOTIFY inputChanged)
    Q_PROPERTY(int wheelLines READ wheelLines WRITE setWheelLines NOTIFY inputChanged)
    // Middle-click autoscroll (compositor/src/autoscroll.h), speed 1-10.
    Q_PROPERTY(bool middleClickAutoscroll READ middleClickAutoscroll WRITE setMiddleClickAutoscroll NOTIFY inputChanged)
    Q_PROPERTY(int autoscrollSpeed READ autoscrollSpeed WRITE setAutoscrollSpeed NOTIFY inputChanged)
    Q_PROPERTY(bool autoscrollClick READ autoscrollClick WRITE setAutoscrollClick NOTIFY inputChanged)
    Q_PROPERTY(bool autoscrollHold READ autoscrollHold WRITE setAutoscrollHold NOTIFY inputChanged)
    Q_PROPERTY(bool hasTouchpad READ hasTouchpad NOTIFY inputChanged) // the compositor says so
    Q_PROPERTY(bool touchpad READ touchpad WRITE setTouchpad NOTIFY inputChanged)
    Q_PROPERTY(bool touchpadWithMouse READ touchpadWithMouse WRITE setTouchpadWithMouse NOTIFY inputChanged)
    Q_PROPERTY(int touchpadSpeed READ touchpadSpeed WRITE setTouchpadSpeed NOTIFY inputChanged)
    Q_PROPERTY(bool touchpadTap READ touchpadTap WRITE setTouchpadTap NOTIFY inputChanged)
    // The top-left corner opens Task View.
    Q_PROPERTY(bool hotCorner READ hotCorner WRITE setHotCorner NOTIFY shortcutsChanged)
    Q_PROPERTY(bool hotCornerGlow READ hotCornerGlow WRITE setHotCornerGlow NOTIFY shortcutsChanged)
    Q_PROPERTY(bool hotCornerAnimation READ hotCornerAnimation WRITE setHotCornerAnimation NOTIFY shortcutsChanged)
    // An indicator on the taskbar while the screen is shared or recorded.
    Q_PROPERTY(bool captureIndicator READ captureIndicator WRITE setCaptureIndicator NOTIFY shortcutsChanged)
    Q_PROPERTY(bool captureDnd READ captureDnd WRITE setCaptureDnd NOTIFY shortcutsChanged)
    Q_PROPERTY(bool fullscreenDnd READ fullscreenDnd WRITE setFullscreenDnd NOTIFY shortcutsChanged)
    // Dragging a window to the top of the screen opens the snap layouts.
    Q_PROPERTY(bool rememberWindowPositions READ rememberWindowPositions WRITE setRememberWindowPositions NOTIFY shortcutsChanged)
    Q_PROPERTY(bool snapDragLayouts READ snapDragLayouts WRITE setSnapDragLayouts NOTIFY shortcutsChanged)
    // How close to the top (px, 4-200) and how far beside them (px, 0-960)
    // they open, and their size (percent, 60-200).
    Q_PROPERTY(int snapDragDistance READ snapDragDistance WRITE setSnapDragDistance NOTIFY shortcutsChanged)
    Q_PROPERTY(int snapDragWidth READ snapDragWidth WRITE setSnapDragWidth NOTIFY shortcutsChanged)
    Q_PROPERTY(int snapDragScale READ snapDragScale WRITE setSnapDragScale NOTIFY shortcutsChanged)
    Q_PROPERTY(bool touchpadNatural READ touchpadNatural WRITE setTouchpadNatural NOTIFY inputChanged)
    // "app", "desktop" or "no"
    Q_PROPERTY(QString threeFingers READ threeFingers WRITE setThreeFingers NOTIFY inputChanged)
    Q_PROPERTY(QString fourFingers READ fourFingers WRITE setFourFingers NOTIFY inputChanged)
    // --- Clipboard (vela-shell.conf) ---
    Q_PROPERTY(bool clipboardHistory READ clipboardHistory WRITE setClipboardHistory NOTIFY clipboardChanged)
    // The Win+V panel opens next to the pointer, not at the bottom right.
    Q_PROPERTY(bool clipboardAtPointer READ clipboardAtPointer WRITE setClipboardAtPointer NOTIFY clipboardChanged)
    // The history outlives the session; for `clipboardDays` days (0: no
    // limit) and `clipboardLimit` items, pinned ones aside.
    Q_PROPERTY(bool clipboardKeep READ clipboardKeep WRITE setClipboardKeep NOTIFY clipboardChanged)
    Q_PROPERTY(int clipboardDays READ clipboardDays WRITE setClipboardDays NOTIFY clipboardChanged)
    Q_PROPERTY(int clipboardLimit READ clipboardLimit WRITE setClipboardLimit NOTIFY clipboardChanged)
    // --- Accessibility ---
    Q_PROPERTY(bool magnifier READ magnifier WRITE setMagnifier NOTIFY accessibilityChanged)
    Q_PROPERTY(int magnifierStep READ magnifierStep WRITE setMagnifierStep NOTIFY accessibilityChanged)
    Q_PROPERTY(bool colorFilter READ colorFilter WRITE setColorFilter NOTIFY accessibilityChanged)
    // grayscale, deuteranopia, protanopia, tritanopia
    Q_PROPERTY(QString colorFilterKind READ colorFilterKind WRITE setColorFilterKind NOTIFY accessibilityChanged)
    Q_PROPERTY(bool colorFilterShortcut READ colorFilterShortcut WRITE setColorFilterShortcut NOTIFY accessibilityChanged)
    Q_PROPERTY(bool stickyKeys READ stickyKeys WRITE setStickyKeys NOTIFY accessibilityChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    QString wallpaper() const { return m_wallpaper; }
    QString defaultWallpaper() const { return vela::defaults::wallpaper(); }
    QStringList recentWallpapers() const { return m_recentWallpapers; }
    QStringList systemWallpapers() const;
    Q_INVOKABLE void setWallpaper(const QString& pathOrUrl);
    QString wallpaperFolder() const { return m_wallpaperFolder; }
    QStringList folderWallpapers() const;
    Q_INVOKABLE void setWallpaperFolder(const QString& pathOrUrl); // "" forgets it
    QString wallpaperFolderSort() const { return m_wallpaperFolderSort; }
    void setWallpaperFolderSort(const QString& sort);

    QColor accent() const { return m_accent; }
    void setAccent(const QColor& color);
    bool accentFromWallpaper() const { return m_accentFromWallpaper; }
    void setAccentFromWallpaper(bool on);

    QString appTheme() const { return m_appTheme; }
    void setAppTheme(const QString& theme);
    QString shellTheme() const { return m_shellTheme; }
    void setShellTheme(const QString& theme);
    bool light() const { return m_appTheme == QLatin1String("light"); }

    QString taskbarAlignment() const { return m_taskbarAlignment; }
    void setTaskbarAlignment(const QString& alignment);
    QString taskbarStyle() const { return m_taskbarStyle; }
    void setTaskbarStyle(const QString& style);
    QString taskbarPosition() const { return m_taskbarPosition; }
    void setTaskbarPosition(const QString& position);
    bool endTask() const { return m_endTask; }
    void setEndTask(bool on);
    bool taskView() const { return m_taskView; }
    void setTaskView(bool on);
    bool taskbarWindowBounce() const { return m_taskbarWindowBounce; }
    void setTaskbarWindowBounce(bool on);
    bool portalAutofix() const { return m_portalAutofix; }
    void setPortalAutofix(bool on);
    bool trayOverflow() const { return m_trayOverflow; }
    void setTrayOverflow(bool on);
    bool taskbarAllDesktops() const { return m_taskbarAllDesktops; }
    void setTaskbarAllDesktops(bool on);
    bool taskbarAllScreens() const { return m_taskbarAllScreens; }
    void setTaskbarAllScreens(bool on);
    bool taskbarAutoHide() const { return m_taskbarAutoHide; }
    void setTaskbarAutoHide(bool on);
    QString taskbarCombine() const { return m_taskbarCombine; }
    void setTaskbarCombine(const QString& mode);

    bool weather() const { return m_weather; }
    void setWeather(bool on);
    QString weatherCity() const { return m_weatherCity; }
    QString weatherUnit() const { return m_weatherUnit; }
    void setWeatherUnit(const QString& unit);
    bool weatherSystemInfo() const { return m_weatherSystemInfo; }
    void setWeatherSystemInfo(bool on);
    Q_INVOKABLE void setWeatherPlace(const QString& city, const QString& lat, const QString& lon);

    QString iconTheme() const { return m_iconTheme; }
    QString visualTheme() const { return m_visualTheme; }
    void setVisualTheme(const QString& theme);
    QVariantMap surfaceStyles() const { return m_surfaceStyles; }
    bool taskbarBackdrop() const { return m_taskbarBackdrop; }
    void setTaskbarBackdrop(bool on);
    bool sliderPressZoom() const { return m_sliderPressZoom; }
    void setSliderPressZoom(bool on);
    bool activateHop() const { return m_activateHop; }
    void setActivateHop(bool on);
    bool animations() const { return m_animations; }
    void setAnimations(bool on);
    int animationSpeed() const { return m_animationSpeed; }
    void setAnimationSpeed(int percent);
    bool navPillAnimation() const { return m_navPillAnimation; }
    void setNavPillAnimation(bool on);
    bool filesPillAnimation() const { return m_filesPillAnimation; }
    void setFilesPillAnimation(bool on);
    bool snapDragPeek() const { return m_snapDragPeek; }
    void setSnapDragPeek(bool on);
    Q_INVOKABLE void resetAnimations();
    bool themeSchedule() const { return m_themeSchedule; }
    void setThemeSchedule(bool on);
    QString themeLightFrom() const { return m_themeLightFrom; }
    void setThemeLightFrom(const QString& time);
    QString themeDarkFrom() const { return m_themeDarkFrom; }
    void setThemeDarkFrom(const QString& time);
    // Sets the light or dark mode the schedule says for the time of day (the
    // shell's timer calls it through --apply-theme-schedule).
    void applyThemeSchedule();
    // Themes page's "Reset": Vela's own theme, styles, accent and the
    // switches; the light/dark mode stays as chosen.
    Q_INVOKABLE void resetAppearance();
    Q_INVOKABLE void setSurfaceStyle(const QString& surface, const QString& style);
    // The installed themes, or a surface's styles ("taskbar"...): { name, text }.
    Q_INVOKABLE QVariantList visualThemes(const QString& surface = QString()) const;
    // The styles of a surface that have screenshots (themes/screenshots/<surface>/<name>[-qs|-nc].png):
    // { name, text, images: [file URLs] }, for Themes > Gallery.
    Q_INVOKABLE QVariantList themeGallery(const QString& surface) const;
    void setIconTheme(const QString& theme);
    // The installed icon themes: [{name, text}], by text; "X-dark"/"X-Dark"
    // are inside "X" (chosen by the mode).
    Q_INVOKABLE QVariantList iconThemes() const;

    QString cursorTheme() const { return m_cursorTheme; }
    void setCursorTheme(const QString& theme);
    int cursorSize() const { return m_cursorSize; }
    void setCursorSize(int size);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& family);
    // The installed cursor themes (with a cursors/ directory): [{name, text}].
    Q_INVOKABLE QVariantList cursorThemes() const;
    // The theme's arrow at about `size` pixels as a data: URL, "" if unreadable.
    Q_INVOKABLE QString cursorPreview(const QString& theme, int size) const;

    bool windowBlur() const { return m_windowBlur; }
    void setWindowBlur(bool on);
    bool windowBlurAll() const { return m_windowBlurAll; }
    void setWindowBlurAll(bool on);
    QStringList windowBlurInclude() const { return m_windowBlurInclude; }
    void setWindowBlurInclude(const QStringList& apps);
    QStringList windowRules() const { return m_windowRules; }
    void setWindowRules(const QStringList& rules);
    QStringList windowBlurExclude() const { return m_windowBlurExclude; }
    void setWindowBlurExclude(const QStringList& apps);
    int windowBlurStrength() const { return m_windowBlurStrength; }
    void setWindowBlurStrength(int percent);
    QVariantList windowBlurApps() const { return m_windowBlurApps; }
    Q_INVOKABLE void setWindowBlurApp(const QString& app, int strength, int opacity);
    Q_INVOKABLE void removeWindowBlurApp(const QString& app);
    QVariantMap panelLook() const;
    Q_INVOKABLE void setPanelLook(const QString& key, int percent);
    int windowOpacity() const { return m_windowOpacity; }
    void setWindowOpacity(int percent);
    // The open windows, one per app: [{appId, title}] (the compositor's
    // "state"); empty if it doesn't answer.
    Q_INVOKABLE QVariantList openWindows() const;
    // The user clicks a window on screen ("pick-window" on the compositor's
    // socket); windowPicked comes later, with empty strings if cancelled.
    Q_INVOKABLE void pickWindow();
    bool showDesktopCorner() const { return m_showDesktopCorner; }
    void setShowDesktopCorner(bool on);
    int startSolid() const { return m_startSolid; }
    void setStartSolid(int percent);

    bool doNotDisturb() const { return m_doNotDisturb; }
    void setDoNotDisturb(bool on);

    int screenOffMinutes() const { return m_screenOffMinutes; }
    void setScreenOffMinutes(int minutes);
    bool lockOnIdle() const { return m_lockOnIdle; }
    void setLockOnIdle(bool on);
    // A vela.conf program choice for DefaultApps (system tools): the chosen
    // program, "" for automatic. Empty writes remove the key.
    QString compositorValue(const QString& key) const;
    void setCompositorValue(const QString& key, const QString& program);
    QString language() const { return m_language; }
    void setLanguage(const QString& language);

    // Keyboard shortcuts (vela.conf: shortcut-<action>, shortcut-custom-N).
    // Each entry of `shortcuts`: id, title, combo (current, "" if off),
    // defaultCombo, changed. Each of `customShortcuts`: number, combo, command.
    Q_PROPERTY(QVariantList shortcuts READ shortcuts NOTIFY shortcutsChanged)
    Q_PROPERTY(QVariantList customShortcuts READ customShortcuts NOTIFY shortcutsChanged)
    // The global shortcuts apps asked for (portal): number, combo, app, description.
    Q_PROPERTY(QVariantList appShortcuts READ appShortcuts NOTIFY shortcutsChanged)
    QVariantList shortcuts() const { return m_shortcuts; }
    QVariantList customShortcuts() const { return m_customShortcuts; }
    QVariantList appShortcuts() const { return m_appShortcuts; }
    Q_INVOKABLE QString setAppShortcut(int number, const QString& combo); // "" for no key
    Q_INVOKABLE void removeAppShortcut(int number);
    Q_INVOKABLE QVariantList fixedShortcuts() const; // listed, not editable: title, combo
    Q_INVOKABLE QString normalizeShortcut(const QString& text) const; // "" if it isn't valid
    // These return an error text, or "" when done.
    Q_INVOKABLE QString setShortcut(const QString& id, const QString& combo);
    Q_INVOKABLE void resetShortcut(const QString& id);
    Q_INVOKABLE QString addCustomShortcut(const QString& combo, const QString& command);
    Q_INVOKABLE QString updateCustomShortcut(int number, const QString& combo, const QString& command);
    Q_INVOKABLE void removeCustomShortcut(int number);

    QVariantList keyboardLayouts() const { return m_layouts; }
    Q_INVOKABLE void addKeyboardLayout(const QString& layout, const QString& variant);
    Q_INVOKABLE void removeKeyboardLayout(int index);
    Q_INVOKABLE void moveKeyboardLayoutUp(int index);
    int repeatDelay() const { return m_repeatDelay; }
    void setRepeatDelay(int ms);
    int repeatRate() const { return m_repeatRate; }
    void setRepeatRate(int perSecond);

    bool nightLight() const { return m_nightLight; }
    void setNightLight(bool on);
    int nightStrength() const { return m_nightStrength; }
    void setNightStrength(int strength);
    /** Whether Night Light uses Vulkan tinting instead of the DRM gamma LUT. */
    bool nightNvidiaFix() const { return m_nightNvidiaFix; }
    void setNightNvidiaFix(bool on);
    QString nightSchedule() const { return m_nightSchedule; }
    void setNightSchedule(const QString& schedule);
    QString nightFrom() const { return m_nightFrom; }
    void setNightFrom(const QString& time);
    QString nightTo() const { return m_nightTo; }
    void setNightTo(const QString& time);
    QString sunset() const { return m_sunset; }
    QString sunrise() const { return m_sunrise; }
    bool tearing() const { return m_tearing; }
    void setTearing(bool on);
    QString vrr() const { return m_vrr; }
    void setVrr(const QString& mode);
    QString directScanout() const { return m_directScanout; }
    void setDirectScanout(const QString& mode);
    bool scanoutOverridden() const { return m_scanoutOverridden; }

    bool magnifier() const { return m_magnifier; }
    void setMagnifier(bool on);
    int magnifierStep() const { return m_magnifierStep; }
    void setMagnifierStep(int percent);
    bool colorFilter() const { return m_colorFilter; }
    void setColorFilter(bool on);
    QString colorFilterKind() const { return m_colorFilterKind; }
    void setColorFilterKind(const QString& kind);
    bool colorFilterShortcut() const { return m_colorFilterShortcut; }
    void setColorFilterShortcut(bool on);
    bool stickyKeys() const { return m_stickyKeys; }
    void setStickyKeys(bool on);

    int mouseSpeed() const { return m_mouseSpeed; }
    void setMouseSpeed(int value);
    bool mousePrecision() const { return m_mousePrecision; }
    void setMousePrecision(bool on);
    bool mouseLeftHanded() const { return m_mouseLeftHanded; }
    void setMouseLeftHanded(bool on);
    int wheelLines() const { return m_wheelLines; }
    bool middleClickAutoscroll() const { return m_middleClickAutoscroll; }
    void setMiddleClickAutoscroll(bool on);
    int autoscrollSpeed() const { return m_autoscrollSpeed; }
    void setAutoscrollSpeed(int value);
    bool autoscrollClick() const { return m_autoscrollClick; }
    void setAutoscrollClick(bool on);
    bool autoscrollHold() const { return m_autoscrollHold; }
    void setAutoscrollHold(bool on);
    void setWheelLines(int lines);
    bool hasTouchpad() const { return m_hasTouchpad; }
    bool touchpad() const { return m_touchpad; }
    void setTouchpad(bool on);
    bool touchpadWithMouse() const { return m_touchpadWithMouse; }
    void setTouchpadWithMouse(bool on);
    int touchpadSpeed() const { return m_touchpadSpeed; }
    void setTouchpadSpeed(int value);
    bool touchpadTap() const { return m_touchpadTap; }
    void setTouchpadTap(bool on);
    bool hotCorner() const { return m_hotCorner; }
    void setHotCorner(bool on);
    bool captureIndicator() const { return m_captureIndicator; }
    void setCaptureIndicator(bool on);
    bool captureDnd() const { return m_captureDnd; }
    void setCaptureDnd(bool on);
    bool fullscreenDnd() const { return m_fullscreenDnd; }
    void setFullscreenDnd(bool on);
    bool hotCornerGlow() const { return m_hotCornerGlow; }
    void setHotCornerGlow(bool on);
    bool hotCornerAnimation() const { return m_hotCornerAnimation; }
    void setHotCornerAnimation(bool on);
    bool rememberWindowPositions() const { return m_rememberWindowPositions; }
    void setRememberWindowPositions(bool on);
    bool snapDragLayouts() const { return m_snapDragLayouts; }
    void setSnapDragLayouts(bool on);
    int snapDragDistance() const { return m_snapDragDistance; }
    void setSnapDragDistance(int value);
    int snapDragWidth() const { return m_snapDragWidth; }
    void setSnapDragWidth(int value);
    int snapDragScale() const { return m_snapDragScale; }
    void setSnapDragScale(int value);
    bool touchpadNatural() const { return m_touchpadNatural; }
    void setTouchpadNatural(bool on);
    QString threeFingers() const { return m_threeFingers; }
    void setThreeFingers(const QString& action);
    QString fourFingers() const { return m_fourFingers; }
    void setFourFingers(const QString& action);

    bool clipboardHistory() const { return m_clipboardHistory; }
    bool clipboardAtPointer() const { return m_clipboardAtPointer; }
    void setClipboardAtPointer(bool on);
    bool clipboardKeep() const { return m_clipboardKeep; }
    void setClipboardKeep(bool on);
    int clipboardDays() const { return m_clipboardDays; }
    void setClipboardDays(int days);
    int clipboardLimit() const { return m_clipboardLimit; }
    void setClipboardLimit(int limit);
    void setClipboardHistory(bool on);
    // "Clear clipboard data": the history (except pinned items), in the shell.
    Q_INVOKABLE void clearClipboard();


    // Rereads everything (the shell or another program may have changed
    // something).
    Q_INVOKABLE void reload();

signals:
    void wallpaperChanged();
    void wallpaperFolderChanged();
    void accentChanged();
    void appThemeChanged();
    void taskbarChanged();
    void iconThemeChanged();
    void visualThemeChanged();
    void animationsChanged();
    void themeScheduleChanged();
    void cursorChanged();
    void fontChanged();
    void windowBlurChanged();
    void windowRulesChanged();
    void windowPicked(const QString& appId, const QString& title);
    void doNotDisturbChanged();
    void idleChanged();
    void languageChanged();
    void keyboardChanged();
    void shortcutsChanged();
    void nightLightChanged();
    void inputChanged();
    void clipboardChanged();
    void accessibilityChanged();

private:
    void setShell(const QString& key, const QVariant& value);
    // vela.conf: reread, only these keys changed, then "reload-config".
    void saveCompositor(const QList<QPair<QString, QString>>& changes);
    void saveWindowBlurApps(const QVariantList& apps);
    void saveCompositorKeys(const QStringList& keys); // the current values of these keys
    void reloadCompositor(); // vela.conf and the compositor's state
    void queryCompositor(); // night light, magnifier...: the current state
    void watchConfig();
    void saveLayouts();
    void loadShortcuts(); // from m_compositor
    QString shortcutConflict(const QString& combo, const QString& exceptKey) const;

    QString m_wallpaper;
    QStringList m_recentWallpapers;
    QString m_wallpaperFolder;
    QString m_wallpaperFolderSort;
    QColor m_accent;
    bool m_accentFromWallpaper = false;
    QString m_appTheme;
    QString m_shellTheme;
    QString m_taskbarAlignment;
    QString m_taskbarStyle = QStringLiteral("classic");
    QString m_taskbarPosition = QStringLiteral("bottom");
    bool m_endTask = true;
    bool m_taskView = true;
    bool m_taskbarWindowBounce = false;
    bool m_portalAutofix = true;
    bool m_trayOverflow = true;
    bool m_taskbarAllScreens = true;
    bool m_taskbarAllDesktops = false;
    bool m_taskbarAutoHide = false;
    QString m_taskbarCombine = QStringLiteral("always");
    bool m_weather = true;
    QString m_weatherCity;
    QString m_weatherUnit = QStringLiteral("c");
    bool m_weatherSystemInfo = true;
    QString m_iconTheme;
    QString m_visualTheme;
    QVariantMap m_surfaceStyles;
    bool m_taskbarBackdrop = false;
    bool m_sliderPressZoom = true;
    bool m_activateHop = true;
    bool m_animations = true;
    int m_animationSpeed = 100;
    bool m_navPillAnimation = true;
    bool m_filesPillAnimation = false;
    bool m_snapDragPeek = true;
    bool m_themeSchedule = false;
    QString m_themeLightFrom = QStringLiteral("07:00");
    QString m_themeDarkFrom = QStringLiteral("19:00");
    QString m_cursorTheme;
    int m_cursorSize = 24;
    QString m_fontFamily;
    bool m_windowBlur = false;
    bool m_windowBlurAll = false;
    QStringList m_windowBlurInclude;
    QStringList m_windowBlurExclude;
    QStringList m_windowRules;
    int m_windowOpacity = 100;
    int m_windowBlurStrength = 100;
    QVariantList m_windowBlurApps;
    QMap<QString, int> m_panelLook;
    bool m_showDesktopCorner = true;
    int m_startSolid = 0;
    bool m_doNotDisturb = false;
    int m_screenOffMinutes = 10;
    bool m_lockOnIdle = true;
    QString m_language;
    QVariantList m_layouts;
    QVariantList m_shortcuts;
    QVariantList m_customShortcuts;
    QVariantList m_appShortcuts;
    int m_repeatDelay = 400;
    int m_repeatRate = 30;
    bool m_nightLight = false;
    int m_nightStrength = 48;
    bool m_nightNvidiaFix = false;
    QString m_nightSchedule = QStringLiteral("no");
    QString m_nightFrom = QStringLiteral("21:00");
    QString m_nightTo = QStringLiteral("07:00");
    QString m_sunset;
    QString m_sunrise;
    bool m_tearing = true;
    QString m_vrr = QStringLiteral("games");
    QString m_directScanout = QStringLiteral("auto");
    bool m_scanoutOverridden = false;
    bool m_magnifier = false;
    int m_magnifierStep = 100;
    bool m_colorFilter = false;
    QString m_colorFilterKind = QStringLiteral("grayscale");
    bool m_colorFilterShortcut = false;
    bool m_stickyKeys = false;
    int m_mouseSpeed = 10;
    bool m_mousePrecision = true;
    bool m_mouseLeftHanded = false;
    int m_wheelLines = 3;
    bool m_middleClickAutoscroll = false;
    int m_autoscrollSpeed = 5;
    bool m_autoscrollClick = true;
    bool m_autoscrollHold = true;
    bool m_hasTouchpad = false;
    bool m_touchpad = true;
    bool m_touchpadWithMouse = true;
    int m_touchpadSpeed = 10;
    bool m_touchpadTap = true;
    bool m_touchpadNatural = true;
    bool m_hotCorner = true;
    bool m_hotCornerGlow = true;
    bool m_captureIndicator = true;
    bool m_captureDnd = true;
    bool m_fullscreenDnd = true;
    bool m_hotCornerAnimation = true;
    bool m_rememberWindowPositions = true;
    bool m_snapDragLayouts = true;
    int m_snapDragDistance = 64;
    int m_snapDragWidth = 160;
    int m_snapDragScale = 100;
    QString m_threeFingers = QStringLiteral("app");
    QString m_fourFingers = QStringLiteral("desktop");
    bool m_clipboardHistory = false;
    bool m_clipboardAtPointer = true;
    bool m_clipboardKeep = false;
    int m_clipboardDays = 0;
    int m_clipboardLimit = 25;
    QList<QPair<QString, QString>> m_compositor; // vela.conf, in file order
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

// Sends a command to the compositor (vela-<display>.sock).
void sendToCompositor(const QByteArray& command);
