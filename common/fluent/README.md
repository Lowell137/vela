# Fluent icons

The command icons of Notepad, Calculator, Photos and the Media Player: Fluent
UI System Icons (https://github.com/microsoft/fluentui-system-icons), the
20 px "regular" drawings, MIT licensed (LICENSE); shape-curve, shape-polygon,
shape-star4 and shape-star6 are drawn here, in the same style. Each file has the name of
the theme icon it stands for (`document-open.svg`), so the QML asks for the
same names as the rest of Vela.

An app that puts them in its resources under `:/fluent` (see
photos/CMakeLists.txt) gets them from `image://icon/` before the theme's, in
the text's color (shell/src/iconprovider.h). Other apps keep the theme's.
