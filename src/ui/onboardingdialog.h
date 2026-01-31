#ifndef ONBOARDINGDIALOG_H
#define ONBOARDINGDIALOG_H

#include <QDialog>
#include <QList>
#include "core/gameinfo.h"

class QStackedWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QScrollArea;

class OnboardingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OnboardingDialog(const QList<GameInfo> &detectedGames,
                              QWidget *parent = nullptr);

    void setDetectedGames(const QList<GameInfo> &games);
    bool isLoading() const;

private slots:
    void onNext();
    void onBack();

private:
    void setupUI();
    QWidget *createWelcomePage();
    QWidget *createGamesPage();
    QWidget *createQuickStartPage();
    void updateNavigation();
    void populateGamesGrid();

    QStackedWidget *m_stackedWidget;
    QPushButton *m_backButton;
    QPushButton *m_nextButton;
    QLabel *m_pageIndicator;
    QList<GameInfo> m_detectedGames;

    // Games page loading state
    bool m_loading = false;
    QStackedWidget *m_gamesPageStack = nullptr;
    QLabel *m_gamesSubtitleLabel = nullptr;
    QScrollArea *m_gamesScrollArea = nullptr;
    QWidget *m_gamesGridContainer = nullptr;
    QLabel *m_gamesMoreLabel = nullptr;
};

#endif // ONBOARDINGDIALOG_H
