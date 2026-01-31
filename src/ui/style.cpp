#include "style.h"
#include <QApplication>
#include <QStyle>
#include <QPalette>
#include <QStyleFactory>
void AppStyle::apply()
{
    QString styleName = QApplication::style()->objectName().toLower();

#ifdef Q_OS_WIN
    // Windows: always apply custom Fusion style
    bool needsCustomStyle = true;
#else
    // Linux: apply if no DE theme is active, or if the user explicitly overrode the style
    bool needsCustomStyle = (styleName == "windows" || styleName == "fusion")
                            || !qEnvironmentVariable("QT_STYLE_OVERRIDE").isEmpty();
#endif

    if (!needsCustomStyle) {
        return;
    }

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    // Window & base
    palette.setColor(QPalette::Window, QColor(43, 43, 43));
    palette.setColor(QPalette::WindowText, QColor(208, 208, 208));
    palette.setColor(QPalette::Base, QColor(53, 53, 53));
    palette.setColor(QPalette::AlternateBase, QColor(60, 60, 60));

    // Text
    palette.setColor(QPalette::Text, QColor(208, 208, 208));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));

    // Buttons
    palette.setColor(QPalette::Button, QColor(60, 60, 60));
    palette.setColor(QPalette::ButtonText, QColor(208, 208, 208));

    // Selections
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::white);

    // Tooltips
    palette.setColor(QPalette::ToolTipBase, QColor(70, 70, 70));
    palette.setColor(QPalette::ToolTipText, QColor(208, 208, 208));

    // Links
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::LinkVisited, QColor(150, 100, 200));

    // Disabled
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));

    QApplication::setPalette(palette);

    // Modern minimal stylesheet
    qobject_cast<QApplication *>(QApplication::instance())->setStyleSheet(QStringLiteral(R"(
        /* Global */
        * {
            outline: none;
        }
        QToolTip {
            background-color: #3a3a3a;
            color: #d0d0d0;
            border: 1px solid #505050;
            border-radius: 4px;
            padding: 4px 8px;
        }

        /* Toolbar */
        QToolBar {
            background: #2b2b2b;
            border: none;
            spacing: 2px;
            padding: 2px 4px;
        }
        QToolBar::separator {
            width: 1px;
            background: #444;
            margin: 4px 4px;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 5px 10px;
            color: #d0d0d0;
        }
        QToolButton:hover {
            background: rgba(255, 255, 255, 0.08);
            border-color: rgba(255, 255, 255, 0.06);
        }
        QToolButton:pressed {
            background: rgba(255, 255, 255, 0.04);
        }

        /* Tree & List views (main window panels) */
        QTreeWidget, QListWidget, QTreeView, QListView {
            background-color: #2f2f2f;
            border: 1px solid #3a3a3a;
            border-radius: 8px;
            padding: 4px;
        }
        QTreeWidget::item, QListWidget::item {
            border-top-left-radius: 6px;
            border-bottom-left-radius: 6px;
            border-top-right-radius: 0px;
            border-bottom-right-radius: 0px;
            padding: 2px;
        }
        QTreeWidget::item:selected, QListWidget::item:selected {
            background-color: rgba(42, 130, 218, 0.25);
        }
        QTreeWidget::item:hover:!selected, QListWidget::item:hover:!selected {
            background-color: rgba(255, 255, 255, 0.04);
        }

        /* Table views (config/profile dialogs) */
        QTableWidget, QTableView {
            background-color: #2f2f2f;
            border: 1px solid #3a3a3a;
            border-radius: 8px;
            gridline-color: #3a3a3a;
            selection-background-color: rgba(42, 130, 218, 0.25);
            selection-color: #d0d0d0;
            padding: 2px;
        }
        QTableWidget::item, QTableView::item {
            padding: 6px 8px;
            border: none;
        }
        QTableWidget::item:selected, QTableView::item:selected {
            background-color: rgba(42, 130, 218, 0.25);
        }
        QHeaderView {
            background-color: #2f2f2f;
            border: none;
        }
        QHeaderView::section {
            background-color: #353535;
            color: #999;
            border: none;
            border-bottom: 1px solid #3a3a3a;
            border-right: 1px solid #3a3a3a;
            padding: 6px 10px;
            font-weight: bold;
        }
        QHeaderView::section:last {
            border-right: none;
        }
        QHeaderView::section:hover {
            background-color: #3c3c3c;
        }

        /* Scroll bars */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 0.15);
            min-height: 30px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(255, 255, 255, 0.25);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
            border: none;
            height: 0;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0 4px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(255, 255, 255, 0.15);
            min-width: 30px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(255, 255, 255, 0.25);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
            border: none;
            width: 0;
        }

        /* Push buttons */
        QPushButton {
            background-color: #3c3c3c;
            border: 1px solid #4a4a4a;
            border-radius: 6px;
            padding: 6px 16px;
            color: #d0d0d0;
            min-height: 22px;
        }
        QPushButton:hover {
            background-color: #454545;
            border-color: #5a5a5a;
        }
        QPushButton:pressed {
            background-color: #333;
        }
        QPushButton:disabled {
            background-color: #333;
            border-color: #3a3a3a;
            color: #666;
        }
        QPushButton:default, QPushButton:focus {
            border-color: #2a82da;
        }

        /* Input fields */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #353535;
            border: 1px solid #4a4a4a;
            border-radius: 6px;
            padding: 5px 8px;
            color: #d0d0d0;
            selection-background-color: #2a82da;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #2a82da;
        }

        /* SpinBox */
        QSpinBox, QDoubleSpinBox {
            background-color: #353535;
            border: 1px solid #4a4a4a;
            border-radius: 6px;
            padding: 5px 8px;
            padding-right: 24px;
            color: #d0d0d0;
            selection-background-color: #2a82da;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #2a82da;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 20px;
            border: none;
            border-left: 1px solid #4a4a4a;
            border-top-right-radius: 6px;
            background: #3c3c3c;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover {
            background: #454545;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url(:/icons/arrow-up.png);
            width: 8px;
            height: 8px;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 20px;
            border: none;
            border-left: 1px solid #4a4a4a;
            border-bottom-right-radius: 6px;
            background: #3c3c3c;
        }
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background: #454545;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url(:/icons/arrow-down.png);
            width: 8px;
            height: 8px;
        }

        /* Combo box */
        QComboBox {
            background-color: #353535;
            border: 1px solid #4a4a4a;
            border-radius: 6px;
            padding: 5px 8px;
            padding-right: 24px;
            color: #d0d0d0;
            selection-background-color: #2a82da;
        }
        QComboBox:focus, QComboBox:on {
            border-color: #2a82da;
        }
        QComboBox::drop-down {
            subcontrol-origin: border;
            subcontrol-position: center right;
            width: 24px;
            border: none;
        }
        QComboBox::down-arrow {
            image: url(:/icons/arrow-down.png);
            width: 8px;
            height: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #353535;
            border: 1px solid #4a4a4a;
            border-radius: 4px;
            selection-background-color: rgba(42, 130, 218, 0.3);
            padding: 2px;
        }

        /* Checkbox */
        QCheckBox {
            spacing: 8px;
            color: #d0d0d0;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #4a4a4a;
            background-color: #353535;
        }
        QCheckBox::indicator:hover {
            border-color: #5a5a5a;
            background-color: #3c3c3c;
        }
        QCheckBox::indicator:checked {
            background-color: #2a82da;
            border-color: #2a82da;
            image: url(:/icons/checkbox-check.png);
        }
        QCheckBox::indicator:checked:hover {
            background-color: #3592e8;
            border-color: #3592e8;
        }

        /* Radio button */
        QRadioButton {
            spacing: 8px;
            color: #d0d0d0;
        }
        QRadioButton::indicator {
            width: 18px;
            height: 18px;
            border-radius: 9px;
            border: 1px solid #4a4a4a;
            background-color: #353535;
        }
        QRadioButton::indicator:hover {
            border-color: #5a5a5a;
        }
        QRadioButton::indicator:checked {
            background-color: #2a82da;
            border-color: #2a82da;
        }

        /* Splitter */
        QSplitter::handle {
            background: #3a3a3a;
            width: 1px;
        }

        /* Status bar */
        QStatusBar {
            background: #2b2b2b;
            border-top: 1px solid #3a3a3a;
            color: #888;
            font-size: 12px;
        }
        QStatusBar QLabel {
            color: #888;
            padding: 0 4px;
        }
        QStatusBar::item {
            border: none;
        }

        /* Progress bar */
        QProgressBar {
            background-color: #353535;
            border: none;
            border-radius: 4px;
            text-align: center;
            color: #d0d0d0;
        }
        QProgressBar::chunk {
            background-color: #2a82da;
            border-radius: 4px;
        }

        /* Tab widget */
        QTabWidget::pane {
            border: 1px solid #3a3a3a;
            border-radius: 6px;
            background: #2f2f2f;
        }
        QTabBar::tab {
            background: #353535;
            border: 1px solid #3a3a3a;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 6px 16px;
            color: #888;
        }
        QTabBar::tab:selected {
            background: #2f2f2f;
            color: #d0d0d0;
        }
        QTabBar::tab:hover:!selected {
            background: #3c3c3c;
            color: #bbb;
        }

        /* Dialogs */
        QDialog {
            background-color: #2b2b2b;
        }

        /* Group box */
        QGroupBox {
            border: 1px solid #3a3a3a;
            border-radius: 8px;
            margin-top: 12px;
            padding: 20px 12px 12px 12px;
            color: #d0d0d0;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            color: #bbb;
        }

        /* Menu */
        QMenu {
            background-color: #353535;
            border: 1px solid #4a4a4a;
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 28px 6px 20px;
            border-radius: 4px;
            color: #d0d0d0;
        }
        QMenu::item:selected {
            background-color: rgba(42, 130, 218, 0.3);
        }
        QMenu::item:disabled {
            color: #666;
        }
        QMenu::separator {
            height: 1px;
            background: #4a4a4a;
            margin: 4px 8px;
        }
        QMenu::icon {
            padding-left: 8px;
        }

        /* Dialog button box */
        QDialogButtonBox QPushButton {
            min-width: 80px;
        }

        /* Frame separator */
        QFrame[frameShape="4"], QFrame[frameShape="5"] {
            color: #3a3a3a;
            max-height: 1px;
        }

        /* Label - no background leak */
        QLabel {
            background: transparent;
        }
    )"));
}

QIcon AppStyle::icon(const QString &name)
{
    QIcon themed = QIcon::fromTheme(name);
    if (!themed.isNull()) {
        return themed;
    }
    return QIcon(QStringLiteral(":/icons/") + name + QStringLiteral(".svg"));
}
