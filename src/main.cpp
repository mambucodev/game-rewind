#include "ui/mainwindow.h"
#include "ui/style.h"
#include <QApplication>
#include <QStyleFactory>
#include <QLocalSocket>
#include <csignal>

static const char *SERVER_NAME = "game-rewind";

static void signalHandler(int)
{
    QApplication::quit();
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(icons);
    QApplication app(argc, argv);

#ifndef Q_OS_WIN
    std::signal(SIGTERM, signalHandler);
#endif
    std::signal(SIGINT, signalHandler);

    app.setApplicationName("Game Rewind");
    app.setOrganizationName("GameRewind");
    app.setApplicationVersion("0.4.0");

    // Single-instance check: try to connect to an existing instance
    QLocalSocket socket;
    socket.connectToServer(SERVER_NAME);
    if (socket.waitForConnected(500)) {
        // Another instance is running — ask it to activate
        socket.write("activate");
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
    }

    AppStyle::apply();

    MainWindow window;
    window.show();

    return app.exec();
}
