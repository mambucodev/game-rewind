#include "ui/mainwindow.h"
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
    QApplication app(argc, argv);

    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    app.setApplicationName("Game Rewind");
    app.setOrganizationName("GameRewind");
    app.setApplicationVersion("0.3.0");

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

    MainWindow window;
    window.show();

    return app.exec();
}
