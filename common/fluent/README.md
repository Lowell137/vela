# Fluent icons

The command icons of Vela's apps (Notepad and the ones built like it): Fluent
UI System Icons (https://github.com/microsoft/fluentui-system-icons), the
20 px "regular" drawings, MIT licensed (LICENSE); shape-curve, shape-polygon,
shape-star4, shape-star6 and audio-input-microphone are drawn here, in the same style. Each file has the name of
the theme icon it stands for (`document-open.svg`), so the QML asks for the
same names as the rest of Vela.

An app that puts them in its resources under `:/fluent` (see
notepad/CMakeLists.txt) gets them from `image://icon/` before the theme's, in
the text's color (shell/src/iconprovider.h). Other apps keep the theme's.
