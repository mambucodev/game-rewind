#ifndef ONBOARDINGDIALOG_H
#define ONBOARDINGDIALOG_H

#include <QDialog>
#include <QList>
#include "core/gameinfo.h"

class QStackedWidget;
class QLabel;
class QPushButton;

class OnboardingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OnboardingDialog(const QList<GameInfo> &detectedGames,
                              QWidget *parent = nullptr);

private slots:
    void onNext();
    void onBack();

private:
    void setupUI();
    QWidget *createWelcomePage();
    QWidget *createGamesPage();
    QWidget *createToolbarGuidePage();
    QWidget *createFinishPage();
    void updateNavigation();

    QStackedWidget *m_stackedWidget;
    QPushButton *m_backButton;
    QPushButton *m_nextButton;
    QLabel *m_pageIndicator;
    QList<GameInfo> m_detectedGames;
};

#endif // ONBOARDINGDIALOG_H
