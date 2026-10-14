# Settings: where each setting lives

Vela Settings follows Windows 11's grouping. The sidebar lists eight sections;
a page lives under exactly one of them, and a setting lives on exactly one
page. Related pages elsewhere are reached with "Related settings" links, not
duplicated controls. Page names (`vela-settings --page <name>`, the Start
menu's search, the shell's menu entries) are the keys of `pages` in
`settings/qml/Main.qml`; old names stay valid through `aliases`.

## Before

| Section | Pages | Notes |
| --- | --- | --- |
| System | Display (Night light), Sound (Properties, Volume mixer), Notifications, Power, Clipboard, About | |
| Bluetooth & devices | Mouse, Touchpad | |
| Network & internet | | |
| Personalization | Background, Colors, Taskbar, Keyboard shortcuts, Icons, Mouse pointer, Window blur, Window rules | a flat list of eight links; the Taskbar page also held Task View hot corner, Screen sharing and Snap windows |
| Apps | Installed apps, Default apps | |
| Time & language | Date & time, Keyboard | |
| Accessibility | | |

## After

| Section | Pages | What changed |
| --- | --- | --- |
| System | Display (Night light), Sound, Notifications, Power, **Multitasking** (Window rules), Clipboard, About | new Multitasking page; Window rules moved here |
| Bluetooth & devices | Mouse, Touchpad | |
| Network & internet | | |
| Personalization | Background, Colors, Icons, Window blur, Taskbar | link list split into "Appearance" and "Taskbar"; pointer, rules and shortcuts are "Related settings" links |
| Apps | Installed apps, Default apps | |
| Time & language | Date & time, Keyboard (Keyboard shortcuts) | Keyboard shortcuts moved under Keyboard |
| Accessibility | Mouse pointer | Mouse pointer moved here, like Windows 11 |

### Settings that changed page

| Setting | From | To |
| --- | --- | --- |
| Hot corner, glow, animation | Taskbar | System > Multitasking |
| Snap layouts (drag to top, distances, size) | Taskbar | System > Multitasking |
| Screen sharing indicator, silence notifications while sharing | Taskbar | System > Display |

Preference keys and `vela.conf` / `vela-shell.conf` entries are unchanged.

### Where to put a new setting

1. Pick the section a Windows 11 user would look in.
2. Use an existing page if one fits; a page that grows past about five groups
   gets split by topic, not extended.
3. Add the page to `pages` (with its parent) and an entry to `searchIndex` in
   `Main.qml`, and to `settingsIndex` in `shell/qml/StartMenu.qml` if it is a
   top-level destination. `settings/tests/check_pages.py` verifies that the
   names agree.
