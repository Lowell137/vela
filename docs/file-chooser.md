# File chooser

Apps ask for a file through xdg-desktop-portal's FileChooser. In Vela the answer is
the File Explorer itself, in the shape of a file chooser: `vela-files --pick request.json`.

- **Backend**: `shell/src/filechooser.cpp`, the `org.freedesktop.impl.portal.FileChooser`
  interface of the portal name the shell already owns (`session/vela.portal`, the same
  object as GlobalShortcuts). `OpenFile`, `SaveFile` and `SaveFiles`; the reply comes when
  the Explorer exits. `Request.Close` stops it.
- **Explorer**: `explorer/src/picker.h` reads the request; `explorer/qml/PickBar.qml` is
  the bottom bar (name, type filter, Open / Save / Cancel). One tab, no tab strip. A double
  click or Enter chooses a file; on a folder it goes in. Saving over a file asks first.
- **Request** (JSON): `mode` (`open`, `save`, `savefiles`), `title`, `accept`, `multiple`,
  `directory`, `folder`, `name`, `names`, `filters` (`[{name, globs}]`), `filter` (the
  chosen one). **Answer**: one line on stdout, `{"uris": [...], "filter": n}`, exit 0;
  no line means Cancel.
- **Configuration**: `session/vela-portals.conf` prefers `vela`, then `kde` and `gtk`.

Not done: the portal's `choices`, `xdg-foreign` parent windows and a preview.
