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
{
    setWindowTitle("Welcome to Game Rewind");
    setMinimumSize(650, 500);
    resize(720, 560);
    setupUI();
}

void OnboardingDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 12);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(createWelcomePage());
    m_stackedWidget->addWidget(createGamesPage());
    m_stackedWidget->addWidget(createToolbarGuidePage());
    m_stackedWidget->addWidget(createFinishPage());
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

    QString subtitle = QString("We found <b>%1</b> game%2 with save data on your system.")
                          .arg(m_detectedGames.size())
                          .arg(m_detectedGames.size() != 1 ? "s" : "");
    QLabel *subtitleLabel = new QLabel(subtitle, page);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setTextFormat(Qt::RichText);
    QPalette subPal = subtitleLabel->palette();
    QColor subColor = subPal.color(QPalette::WindowText);
    subColor.setAlphaF(0.65);
    subPal.setColor(QPalette::WindowText, subColor);
    subtitleLabel->setPalette(subPal);
    layout->addWidget(subtitleLabel);
    layout->addSpacing(8);

    if (m_detectedGames.isEmpty()) {
        QLabel *emptyLabel = new QLabel(
            "No games detected yet.\n\n"
            "Use the Add Game button in the toolbar to add games manually.",
            page);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setWordWrap(true);
        QPalette pal = emptyLabel->palette();
        QColor muted = pal.color(QPalette::WindowText);
        muted.setAlphaF(0.45);
        pal.setColor(QPalette::WindowText, muted);
        emptyLabel->setPalette(pal);
        layout->addStretch();
        layout->addWidget(emptyLabel);
        layout->addStretch();
        return page;
    }

    // Scrollable grid of capsule thumbnails
    QScrollArea *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *gridContainer = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(8, 4, 8, 4);

    int columns = 5;
    int thumbnailWidth = 90;
    int thumbnailHeight = 135;

    int maxGames = qMin(m_detectedGames.size(), 30);
    for (int i = 0; i < maxGames; ++i) {
        const GameInfo &game = m_detectedGames[i];
        QPixmap capsule = GameIconProvider::getHighResCapsule(game);

        QWidget *card = new QWidget(gridContainer);
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
            // Placeholder with platform icon
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
            gridContainer);
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

    scrollArea->setWidget(gridContainer);
    layout->addWidget(scrollArea, 1);

    return page;
}

QWidget *OnboardingDialog::createToolbarGuidePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 16, 32, 8);

    QLabel *titleLabel = new QLabel("How It Works", page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addSpacing(20);

    struct GuideEntry {
        QString iconName;
        QString title;
        QString description;
    };

    QList<GuideEntry> entries = {
        {"document-save", "Create Backup",
         "Select a game, press <b>Ctrl+B</b> to back up your saves."},
        {"document-revert", "Restore",
         "Select a backup, press <b>Ctrl+R</b> to restore it."},
        {"edit-delete", "Delete",
         "Remove old backups with the <b>Delete</b> key."},
        {"list-add", "Add Game",
         "Manually add games that weren't auto-detected."},
        {"view-refresh", "Refresh",
         "Press <b>F5</b> to re-scan for new games."},
        {"applications-games", "Hide",
         "Right-click a game to hide it from the list."},
    };

    for (const GuideEntry &entry : entries) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(12);

        QLabel *iconLabel = new QLabel(page);
        QIcon icon = QIcon::fromTheme(entry.iconName);
        iconLabel->setPixmap(icon.pixmap(22, 22));
        iconLabel->setFixedSize(28, 28);
        iconLabel->setAlignment(Qt::AlignCenter);

        QLabel *textLabel = new QLabel(
            QString("<b>%1</b> &mdash; %2").arg(entry.title, entry.description), page);
        textLabel->setWordWrap(true);
        textLabel->setTextFormat(Qt::RichText);

        row->addWidget(iconLabel);
        row->addWidget(textLabel, 1);
        layout->addLayout(row);
        layout->addSpacing(6);
    }

    layout->addStretch();
    return page;
}

QWidget *OnboardingDialog::createFinishPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(40, 20, 40, 20);

    QLabel *iconLabel = new QLabel(page);
    QIcon checkIcon = QIcon::fromTheme("emblem-default",
                         QIcon::fromTheme("dialog-ok-apply",
                             QIcon::fromTheme("checkmark")));
    iconLabel->setPixmap(checkIcon.pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("You're All Set", page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    // Quick-reference shortcut cards
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

    QLabel *hintLabel = new QLabel("Pick a game from the left panel to get started.", page);
    hintLabel->setAlignment(Qt::AlignCenter);
    QPalette hintPal = hintLabel->palette();
    QColor hintColor = hintPal.color(QPalette::WindowText);
    hintColor.setAlphaF(0.5);
    hintPal.setColor(QPalette::WindowText, hintColor);
    hintLabel->setPalette(hintPal);

    layout->addStretch(2);
    layout->addWidget(iconLabel);
    layout->addSpacing(12);
    layout->addWidget(titleLabel);
    layout->addSpacing(24);
    layout->addWidget(shortcutsWidget, 0, Qt::AlignCenter);
    layout->addSpacing(20);
    layout->addWidget(hintLabel);
    layout->addStretch(3);

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
    m_pageIndicator->setText(QString("%1 / %2")
                                .arg(current + 1)
                                .arg(m_stackedWidget->count()));
}
