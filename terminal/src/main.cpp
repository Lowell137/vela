// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-terminal: Vela's Terminal, the one of Windows 11's.
//
//   vela-terminal [-d folder] [-e command [arguments...]]
//
// Built like Notepad. The terminals are painted with OpenGL (src/terminal.h).

#include "controls.h"
#include "host.h"
#include "iconprovider.h"
#include "language.h"
#include "terminal.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <csignal>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

namespace {
// A word of the command line as the shell would have to be given it again.
QString quoted(const QString& word)
{
    QString out = word;
    out.replace(QLatin1Char('"'), QLatin1String("\"\"\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    // What the programs inside are told they're in.
    qputenv("TERM", "xterm-256color");
    qputenv("COLORTERM", "truecolor");
    // Programs that ended aren't waited for: the pty closing says so.
    struct sigaction ignore = {};
    ignore.sa_handler = SIG_IGN;
    ignore.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &ignore, nullptr);

    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    // The terminals paint into a framebuffer object, which only OpenGL has.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setDefaultAlphaBuffer(true); // the background can be seen through

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-terminal"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-terminal"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Terminal", "Terminal"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Terminal", "Terminal"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-terminal"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    // Everything after -e is the command, as in the other terminals.
    QString command;
    QString directory;
    const QStringList arguments = QCoreApplication::arguments();
    for (int i = 1; i < arguments.size(); i++) {
        if ((arguments[i] == QLatin1String("-d") || arguments[i] == QLatin1String("--working-directory")) && i + 1 < arguments.size()) {
            directory = arguments[++i];
        } else if (arguments[i] == QLatin1String("-e") || arguments[i] == QLatin1String("--")) {
            QStringList words;
            for (int j = i + 1; j < arguments.size(); j++) {
                words.append(quoted(arguments[j]));
            }
            command = words.join(QLatin1Char(' '));
            break;
        }
    }
    Host host(command, directory);

    qmlRegisterType<Terminal>("Vela.Terminal.Backend", 1, 0, "Terminal");

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    engine.rootContext()->setContextProperty(QStringLiteral("Host"), &host);
    engine.loadFromModule("Vela.Terminal", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
