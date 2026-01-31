#include "onboardingdialog.h"
#include "gameicon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QProgressBar>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

static QPixmap roundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull()) {
        return QPixmap();
    }
    QPixmap rounded(source.size());
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0, 0, source.width(), source.height(), radius, radius);
    p.setClipPath(path);
    p.drawPixmap(0, 0, source);
    return rounded;
}

OnboardingDialog::OnboardingDialog(const QList<GameInfo> &detectedGames,
                                   QWidget *parent)
    : QDialog(parent)
    , m_detectedGames(detectedGames)
    , m_loading(detectedGames.isEmpty())
{
    setWindowTitle("Welcome to Game Rewind");
    setMinimumSize(650, 500);
    resize(720, 560);
    setupUI();
    if (!m_loading) {
        populateGamesGrid();
    }
}

void OnboardingDialog::setDetectedGames(const QList<GameInfo> &games)
{
    m_detectedGames = games;
    m_loading = false;
    populateGamesGrid();
    updateNavigation();
}

bool OnboardingDialog::isLoading() const
{
    return m_loading;
}

void OnboardingDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 12);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(createWelcomePage());
    m_stackedWidget->addWidget(createGamesPage());
    m_stackedWidget->addWidget(createQuickStartPage());
    mainLayout->addWidget(m_stackedWidget, 1);

    // Separator
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Navigation bar
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setContentsMargins(16, 8, 16, 0);

    m_pageIndicator = new QLabel("1 / 4", this);
    QPalette indPal = m_pageIndicator->palette();
    QColor indColor = indPal.color(QPalette::WindowText);
    indColor.setAlphaF(0.4);
    indPal.setColor(QPalette::WindowText, indColor);
    m_pageIndicator->setPalette(indPal);

    m_backButton = new QPushButton("Back", this);
    m_nextButton = new QPushButton("Next", this);
    m_nextButton->setDefault(true);

    navLayout->addWidget(m_pageIndicator);
    navLayout->addStretch();
    navLayout->addWidget(m_backButton);
    navLayout->addWidget(m_nextButton);
    mainLayout->addLayout(navLayout);

    connect(m_backButton, &QPushButton::clicked, this, &OnboardingDialog::onBack);
    connect(m_nextButton, &QPushButton::clicked, this, &OnboardingDialog::onNext);

    updateNavigation();
}

QWidget *OnboardingDialog::createWelcomePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(40, 20, 40, 20);

    QLabel *iconLabel = new QLabel(page);
    QIcon appIcon = QIcon::fromTheme("document-save", QIcon::fromTheme("applications-games"));
    iconLabel->setPixmap(appIcon.pixmap(72, 72));
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("Welcome to Game Rewind", page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *descLabel = new QLabel(
        "Automatically detect your games, back up your saves,\n"
        "and never lose progress again.",
        page);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    QFont descFont = descLabel->font();
    descFont.setPointSize(descFont.pointSize() + 1);
    descLabel->setFont(descFont);
    QPalette descPal = descLabel->palette();
    QColor descColor = descPal.color(QPalette::WindowText);
    descColor.setAlphaF(0.7);
    descPal.setColor(QPalette::WindowText, descColor);
    descLabel->setPalette(descPal);

    layout->addStretch(2);
    layout->addWidget(iconLabel);
    layout->addSpacing(20);
    layout->addWidget(titleLabel);
    layout->addSpacing(10);
    layout->addWidget(descLabel);
    layout->addStretch(3);

    return page;
}

QWidget *OnboardingDialog::createGamesPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 16, 24, 8);

    QLabel *titleLabel = new QLabel("Your Games", page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    m_gamesSubtitleLabel = new QLabel(page);
    m_gamesSubtitleLabel->setAlignment(Qt::AlignCenter);
    m_gamesSubtitleLabel->setTextFormat(Qt::RichText);
    QPalette subPal = m_gamesSubtitleLabel->palette();
    QColor subColor = subPal.color(QPalette::WindowText);
    subColor.setAlphaF(0.65);
    subPal.setColor(QPalette::WindowText, subColor);
    m_gamesSubtitleLabel->setPalette(subPal);
    layout->addWidget(m_gamesSubtitleLabel);
    layout->addSpacing(8);

    m_gamesPageStack = new QStackedWidget(page);

    // Page 0: Loading state
    QWidget *loadingPage = new QWidget(this);
    QVBoxLayout *loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);

    QProgressBar *loadingBar = new QProgressBar(loadingPage);
    loadingBar->setRange(0, 0); // Indeterminate
    loadingBar->setMaximumWidth(250);
    loadingBar->setMaximumHeight(16);

    QLabel *loadingLabel = new QLabel("Detecting games...", loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    QPalette loadPal = loadingLabel->palette();
    QColor loadColor = loadPal.color(QPalette::WindowText);
    loadColor.setAlphaF(0.5);
    loadPal.setColor(QPalette::WindowText, loadColor);
    loadingLabel->setPalette(loadPal);

    loadingLayout->addWidget(loadingBar, 0, Qt::AlignCenter);
    loadingLayout->addSpacing(8);
    loadingLayout->addWidget(loadingLabel);

    m_gamesPageStack->addWidget(loadingPage); // index 0

    // Page 1: Games grid (populated later via populateGamesGrid)
    QWidget *gridPage = new QWidget(this);
    QVBoxLayout *gridPageLayout = new QVBoxLayout(gridPage);
    gridPageLayout->setContentsMargins(0, 0, 0, 0);

    m_gamesScrollArea = new QScrollArea(gridPage);
    m_gamesScrollArea->setWidgetResizable(true);
    m_gamesScrollArea->setFrameShape(QFrame::NoFrame);
    m_gamesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_gamesGridContainer = new QWidget();
    m_gamesScrollArea->setWidget(m_gamesGridContainer);
    gridPageLayout->addWidget(m_gamesScrollArea, 1);

    m_gamesPageStack->addWidget(gridPage); // index 1

    // Show loading or grid depending on state
    m_gamesPageStack->setCurrentIndex(m_loading ? 0 : 1);
    m_gamesSubtitleLabel->setText(m_loading ? "Scanning your system for games..." : "");

    layout->addWidget(m_gamesPageStack, 1);

    return page;
}

void OnboardingDialog::populateGamesGrid()
{
    // Update subtitle
    if (m_detectedGames.isEmpty()) {
        m_gamesSubtitleLabel->setText("No games detected. You can add games manually later.");
    } else {
        m_gamesSubtitleLabel->setText(
            QString("We found <b>%1</b> game%2 with save data on your system.")
                .arg(m_detectedGames.size())
                .arg(m_detectedGames.size() != 1 ? "s" : ""));
    }

    // Replace the grid container contents
    QWidget *newContainer = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(newContainer);
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(8, 4, 8, 4);

    int columns = 5;
    int thumbnailWidth = 90;
    int thumbnailHeight = 135;

    int maxGames = qMin(m_detectedGames.size(), 30);
    for (int i = 0; i < maxGames; ++i) {
        const GameInfo &game = m_detectedGames[i];
        QPixmap capsule = GameIconProvider::getHighResCapsule(game);

        QWidget *card = new QWidget(newContainer);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(2);

        QLabel *imageLabel = new QLabel(card);
        imageLabel->setFixedSize(thumbnailWidth, thumbnailHeight);
        imageLabel->setAlignment(Qt::AlignCenter);

        if (!capsule.isNull() && capsule.width() > 64) {
            QPixmap scaled = capsule.scaled(thumbnailWidth, thumbnailHeight,
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
            imageLabel->setPixmap(roundedPixmap(scaled, 8));
        } else {
            QIcon icon = GameIconProvider::getIconForGame(game);
            QPixmap placeholder(thumbnailWidth, thumbnailHeight);
            placeholder.fill(Qt::transparent);
            QPainter pp(&placeholder);
            pp.setRenderHint(QPainter::Antialiasing);
            pp.setBrush(card->palette().mid());
            pp.setPen(Qt::NoPen);
            QPainterPath placeholderPath;
            placeholderPath.addRoundedRect(0, 0, thumbnailWidth, thumbnailHeight, 8, 8);
            pp.drawPath(placeholderPath);
            QPixmap iconPx = icon.pixmap(32, 32);
            pp.drawPixmap((thumbnailWidth - iconPx.width()) / 2,
                          (thumbnailHeight - iconPx.height()) / 2,
                          iconPx);
            pp.end();
            imageLabel->setPixmap(placeholder);
        }
        cardLayout->addWidget(imageLabel, 0, Qt::AlignHCenter);

        QLabel *nameLabel = new QLabel(game.name, card);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        nameLabel->setFixedWidth(thumbnailWidth + 4);
        QFont nameFont = nameLabel->font();
        nameFont.setPointSize(nameFont.pointSize() - 2);
        nameLabel->setFont(nameFont);
        nameLabel->setMaximumHeight(28);
        cardLayout->addWidget(nameLabel, 0, Qt::AlignHCenter);

        gridLayout->addWidget(card, i / columns, i % columns, Qt::AlignTop | Qt::AlignHCenter);
    }

    if (m_detectedGames.size() > maxGames) {
        QLabel *moreLabel = new QLabel(
            QString("+ %1 more").arg(m_detectedGames.size() - maxGames),
            newContainer);
        moreLabel->setAlignment(Qt::AlignCenter);
        QFont moreFont = moreLabel->font();
        moreFont.setBold(true);
        moreLabel->setFont(moreFont);
        QPalette pal = moreLabel->palette();
        QColor muted = pal.color(QPalette::WindowText);
        muted.setAlphaF(0.4);
        pal.setColor(QPalette::WindowText, muted);
        moreLabel->setPalette(pal);
        gridLayout->addWidget(moreLabel, (maxGames / columns) + 1, 0, 1, columns);
    }

    QWidget *oldContainer = m_gamesScrollArea->takeWidget();
    m_gamesGridContainer = newContainer;
    m_gamesScrollArea->setWidget(newContainer);
    delete oldContainer;

    // Switch from loading to grid
    m_gamesPageStack->setCurrentIndex(1);
}

QWidget *OnboardingDialog::createQuickStartPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 16, 40, 20);

    // Title
    QLabel *titleLabel = new QLabel("Quick Start", page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addSpacing(16);

    // Three-step workflow
    struct Step {
        QString iconName;
        QString title;
        QString description;
    };

    QList<Step> steps = {
        {"applications-games", "1. Pick a game",
         "Select a game from the left panel to see its backups."},
        {"document-save", "2. Create a backup",
         "Press <b>Ctrl+B</b> or click <b>Create Backup</b> to save your progress."},
        {"document-revert", "3. Restore anytime",
         "Select a backup and press <b>Ctrl+R</b> to restore it."},
    };

    for (const Step &step : steps) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(12);

        QLabel *iconLabel = new QLabel(page);
        iconLabel->setPixmap(QIcon::fromTheme(step.iconName).pixmap(28, 28));
        iconLabel->setFixedSize(36, 36);
        iconLabel->setAlignment(Qt::AlignCenter);

        QLabel *textLabel = new QLabel(
            QString("<b>%1</b><br>%2").arg(step.title, step.description), page);
        textLabel->setWordWrap(true);
        textLabel->setTextFormat(Qt::RichText);

        row->addWidget(iconLabel);
        row->addWidget(textLabel, 1);
        layout->addLayout(row);
        layout->addSpacing(8);
    }

    layout->addSpacing(12);

    // Keyboard shortcut cards
    QWidget *shortcutsWidget = new QWidget(page);
    QHBoxLayout *shortcutsLayout = new QHBoxLayout(shortcutsWidget);
    shortcutsLayout->setSpacing(16);
    shortcutsLayout->setContentsMargins(0, 0, 0, 0);

    struct ShortcutCard {
        QString key;
        QString label;
        QString iconName;
    };

    QList<ShortcutCard> shortcuts = {
        {"Ctrl+B", "Backup", "document-save"},
        {"Ctrl+R", "Restore", "document-revert"},
        {"Del", "Delete", "edit-delete"},
        {"F5", "Refresh", "view-refresh"},
    };

    for (const ShortcutCard &sc : shortcuts) {
        QWidget *card = new QWidget(shortcutsWidget);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setAlignment(Qt::AlignCenter);
        cardLayout->setSpacing(4);
        cardLayout->setContentsMargins(12, 10, 12, 10);

        card->setStyleSheet(
            "QWidget { background: palette(mid); border-radius: 8px; }");

        QLabel *cardIcon = new QLabel(card);
        cardIcon->setPixmap(QIcon::fromTheme(sc.iconName).pixmap(20, 20));
        cardIcon->setAlignment(Qt::AlignCenter);
        cardIcon->setStyleSheet("background: transparent;");

        QLabel *keyLabel = new QLabel(sc.key, card);
        QFont keyFont = keyLabel->font();
        keyFont.setBold(true);
        keyFont.setPointSize(keyFont.pointSize() + 1);
        keyLabel->setFont(keyFont);
        keyLabel->setAlignment(Qt::AlignCenter);
        keyLabel->setStyleSheet("background: transparent;");

        QLabel *descLabel = new QLabel(sc.label, card);
        descLabel->setAlignment(Qt::AlignCenter);
        QFont descFont = descLabel->font();
        descFont.setPointSize(descFont.pointSize() - 1);
        descLabel->setFont(descFont);
        descLabel->setStyleSheet("background: transparent;");

        QPalette descPal = descLabel->palette();
        QColor descColor = descPal.color(QPalette::WindowText);
        descColor.setAlphaF(0.6);
        descPal.setColor(QPalette::WindowText, descColor);
        descLabel->setPalette(descPal);

        cardLayout->addWidget(cardIcon);
        cardLayout->addWidget(keyLabel);
        cardLayout->addWidget(descLabel);

        shortcutsLayout->addWidget(card);
    }

    layout->addWidget(shortcutsWidget, 0, Qt::AlignCenter);
    layout->addSpacing(20);

    // Hint
    QLabel *hintLabel = new QLabel("Pick a game from the left panel to get started.", page);
    hintLabel->setAlignment(Qt::AlignCenter);
    QPalette hintPal = hintLabel->palette();
    QColor hintColor = hintPal.color(QPalette::WindowText);
    hintColor.setAlphaF(0.5);
    hintPal.setColor(QPalette::WindowText, hintColor);
    hintLabel->setPalette(hintPal);
    layout->addWidget(hintLabel);
    layout->addStretch();

    return page;
}

void OnboardingDialog::onNext()
{
    int current = m_stackedWidget->currentIndex();
    int last = m_stackedWidget->count() - 1;
    if (current == last) {
        accept();
    } else {
        m_stackedWidget->setCurrentIndex(current + 1);
        updateNavigation();
    }
}

void OnboardingDialog::onBack()
{
    int current = m_stackedWidget->currentIndex();
    if (current > 0) {
        m_stackedWidget->setCurrentIndex(current - 1);
        updateNavigation();
    }
}

void OnboardingDialog::updateNavigation()
{
    int current = m_stackedWidget->currentIndex();
    int last = m_stackedWidget->count() - 1;

    m_backButton->setEnabled(current > 0);
    m_nextButton->setText(current == last ? "Get Started" : "Next");

    // Block Next on games page (index 1) while still loading
    bool blocked = (current == 1 && m_loading);
    m_nextButton->setEnabled(!blocked);

    m_pageIndicator->setText(QString("%1 / %2")
                                .arg(current + 1)
                                .arg(m_stackedWidget->count()));
}
