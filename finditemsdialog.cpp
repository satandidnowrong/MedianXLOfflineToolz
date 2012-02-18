#include "finditemsdialog.h"
#include "itemdatabase.h"
#include "propertiesdisplaymanager.h"
#include "structs.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDesktopWidget>

#include <QRegExp>
#include <QSettings>

#ifndef QT_NO_DEBUG
#include <QDebug>
#endif


FindItemsDialog::FindItemsDialog(QWidget *parent) : QDialog(parent), _searchPerformed(false), _searchResultsChanged(false), _resultsWidget(new FindResultsWidget(this)), _lastResultsHeight(-1)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    updateWindowTitle();

    QGridLayout *checkboxGrid = new QGridLayout;
    checkboxGrid->addWidget(ui.caseSensitiveCheckBox, 0, 0);
    checkboxGrid->addWidget(ui.searchPropsCheckBox, 1, 0);
    checkboxGrid->addWidget(ui.wrapAroundCheckBox, 2, 0);
    checkboxGrid->addWidget(ui.regexCheckBox, 0, 1);
    checkboxGrid->addWidget(ui.minimalMatchCheckBox, 1, 1);
    checkboxGrid->addWidget(ui.multilineMatchCheckBox, 2, 1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(ui.nextButton);
    vbox->addWidget(ui.previousButton);
    vbox->addWidget(ui.searchResultsButton);
    vbox->addStretch();
    vbox->addWidget(ui.helpButton);
    vbox->addWidget(ui.closeButton);

    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->addWidget(ui.searchComboBox, 0, 0, Qt::AlignTop);
    mainGrid->addLayout(checkboxGrid, 1, 0);
    mainGrid->addLayout(vbox, 0, 1, 2, 1, Qt::AlignRight);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(mainGrid);
    mainLayout->addWidget(_resultsWidget);

    toggleResults();
    _resultsWidget->hide();
    
    ui.nextButton->setToolTip(QKeySequence(QKeySequence::FindNext).toString(QKeySequence::NativeText));
    ui.previousButton->setToolTip(QKeySequence(QKeySequence::FindPrevious).toString(QKeySequence::NativeText));

    connect(ui.nextButton, SIGNAL(clicked()), SLOT(findNext()));
    connect(ui.previousButton, SIGNAL(clicked()), SLOT(findPrevious()));
    connect(ui.searchResultsButton, SIGNAL(clicked()), SLOT(toggleResults()));
    connect(ui.searchComboBox, SIGNAL(editTextChanged(const QString &)), SLOT(searchTextChanged()));
    connect(_resultsWidget, SIGNAL(showItem(ItemInfo *)), SLOT(updateCurrentIndexForItem(ItemInfo *)));

    QList<QCheckBox *> checkBoxes = QList<QCheckBox *>() << ui.caseSensitiveCheckBox << ui.minimalMatchCheckBox << ui.regexCheckBox << ui.multilineMatchCheckBox << ui.searchPropsCheckBox;
    foreach (QCheckBox *checkBox, checkBoxes)
        connect(checkBox, SIGNAL(toggled(bool)), SLOT(resetSearchStatus()));

    loadSettings();
    adjustSize();
    setMaximumHeight(qApp->desktop()->availableGeometry().height() - 50);
}

void FindItemsDialog::clearResults()
{
    _searchResult.clear();
    searchTextChanged(); // text doesn't actually change here, I just don't want to create new method that only calls this one
    nothingFound(false);
}

void FindItemsDialog::findNext()
{
    if (_searchPerformed)
    {
        if (!_searchResult.isEmpty())
        {
            if (_currentIndex + 1 == _searchResult.size())
            {
                if (ui.wrapAroundCheckBox->isChecked())
                {
                    _currentIndex = -1;
                    qApp->beep();
                }
                else
                {
                    WARNING_BOX(tr("No more items found"));
                    return;
                }
            }
            ++_currentIndex;
            changeItem();
        }
        else
        {
            nothingFound();
        }
    }
    else
    {
        performSearch();
        _currentIndex = -1;
        findNext(); // show the first result
    }
}

void FindItemsDialog::findPrevious()
{
    if (_searchPerformed)
    {
        if (!_searchResult.isEmpty())
        {
            if (_currentIndex - 1 == -1)
            {
                if (ui.wrapAroundCheckBox->isChecked())
                {
                    _currentIndex = _searchResult.size();
                    qApp->beep();
                }
                else
                {
                    WARNING_BOX(tr("No more items found"));
                    return;
                }
            }
            --_currentIndex;
            changeItem();
        }
        else
        {
            nothingFound();
        }
    }
    else
    {
        performSearch();
        _currentIndex = _searchResult.size();
        findPrevious(); // show the first result
    }
}

void FindItemsDialog::toggleResults()
{
    if (_resultsWidget->isVisible())
        _lastResultsHeight = _resultsWidget->height();
    _resultsWidget->setVisible(!_resultsWidget->isVisible());

    if (_resultsWidget->isVisible())
    {
        ui.searchResultsButton->setText(tr("Hide results"));

        QSize oldSize = size();
        int newHeight = oldSize.height() + (_lastResultsHeight == -1 ? _resultsWidget->sizeHint().height() : _lastResultsHeight);
        setMinimumHeight(minimumHeight() + _resultsWidget->minimumHeight());
        resize(oldSize.width(), newHeight);

        if (_searchResultsChanged)
        {
            _resultsWidget->updateItems(&_searchResult);
            _resultsWidget->selectItem(_searchResult[_currentIndex].first);
            _searchResultsChanged = false;
        }
    }
    else
    {
        ui.searchResultsButton->setText(tr("Show results"));

        QSize oldSize = size();
        int newHeight = oldSize.height() - _lastResultsHeight;
        setMinimumHeight(newHeight);
        resize(oldSize.width(), newHeight);
    }
}

void FindItemsDialog::updateCurrentIndexForItem(ItemInfo *item)
{
    _currentIndex = 0;
    foreach (const SearchResultItem &foundItem, _searchResult)
    {
        if (foundItem.first == item)
            break;
        ++_currentIndex;
    }
    changeItem(false);
}

void FindItemsDialog::searchTextChanged()
{
    resetSearchStatus();
    updateWindowTitle();
}

void FindItemsDialog::performSearch()
{
    QString searchText = ui.searchComboBox->currentText();
    _searchResult.clear();
    foreach (ItemInfo *item, *ItemDataBase::currentCharacterItems)
    {
        QString itemText = ui.searchPropsCheckBox->isChecked() ? PropertiesDisplayManager::completeItemDescription(item) : ItemDataBase::completeItemName(item, false);
        Qt::CaseSensitivity cs = static_cast<Qt::CaseSensitivity>(ui.caseSensitiveCheckBox->isChecked());
        if (ui.regexCheckBox->isChecked())
        {
            QRegExp rx(searchText, cs, QRegExp::RegExp2);
            rx.setMinimal(ui.minimalMatchCheckBox->isChecked());
            if (ui.multilineMatchCheckBox->isChecked())
            {
                int matchIndex = rx.indexIn(itemText);
                if (matchIndex != -1)
                {
                    int previousLineBreak = itemText.lastIndexOf("\n", matchIndex) + 1, nextLineBreak = itemText.indexOf("\n", matchIndex + rx.cap().length());
                    QString matchedLine = nextLineBreak != -1 ? itemText.mid(previousLineBreak, nextLineBreak - previousLineBreak) : itemText.mid(previousLineBreak);
                    matchIndex = rx.indexIn(matchedLine);
                    matchedLine.insert(matchIndex, "<b>");
                    matchedLine.insert(matchIndex + rx.cap().length() + 3, "</b>");
                    matchedLine.replace("\n", htmlLineBreak);
                    _searchResult += qMakePair(item, matchedLine);
                }
            }
            else
            {
                QStringList lines = itemText.split("\n");
                foreach (QString line, lines)
                {
                    int matchIndex = rx.indexIn(line);
                    if (matchIndex != -1)
                    {
                        line.insert(matchIndex, "<b>");
                        line.insert(matchIndex + rx.cap().length() + 3, "</b>");
                        _searchResult += qMakePair(item, line);
                    }
                }
            }
        }
        else
        {
            int matchIndex = itemText.indexOf(searchText, 0, cs);
            if (matchIndex != -1)
            {
                int previousLineBreak = itemText.lastIndexOf("\n", matchIndex) + 1, nextLineBreak = itemText.indexOf("\n", matchIndex + searchText.length());
                QString matchedLine = nextLineBreak != -1 ? itemText.mid(previousLineBreak, nextLineBreak - previousLineBreak) : itemText.mid(previousLineBreak);
                matchIndex = matchedLine.indexOf(searchText, 0, cs);
                matchedLine.insert(matchIndex, "<b>");
                matchedLine.insert(matchIndex + searchText.length() + 3, "</b>");
                _searchResult += qMakePair(item, matchedLine);
            }
        }
    }

    _searchPerformed = _searchResultsChanged = true;
    ui.searchResultsButton->setEnabled(!_searchResult.isEmpty());
    _resultsWidget->updateItems(&_searchResult);

    // move the search string to the top of the last searches list if it is present there and not on the top
    if (ui.searchComboBox->currentIndex() > 0)
    {
        QStringList history;
        for (int i = 0; i < ui.searchComboBox->count(); ++i)
            history += ui.searchComboBox->itemText(i);
        history.move(ui.searchComboBox->currentIndex(), 0);

        ui.searchComboBox->clear();
        ui.searchComboBox->addItems(history);
    }
}

void FindItemsDialog::nothingFound(bool wasSearchDone /*= true*/)
{
    emit itemFound(0);

    if (wasSearchDone)
        setButtonsDisabled(true);

    _resultsWidget->updateItems(&_searchResult);
    if (_resultsWidget->isVisible())
        toggleResults();

    if (wasSearchDone)
        ERROR_BOX(tr("No items found"));
}

void FindItemsDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup("findDialog");
    if (settings.contains("pos"))
        move(settings.value("pos").toPoint());
    ui.searchComboBox->addItems(settings.value("searchHistory").toStringList());
    ui.caseSensitiveCheckBox->setChecked(settings.value("caseSensitive").toBool());
    ui.minimalMatchCheckBox->setChecked(settings.value("regexMinimalMatch").toBool());
    ui.multilineMatchCheckBox->setChecked(settings.value("regexMultilineMatch").toBool());
    ui.regexCheckBox->setChecked(settings.value("regex").toBool());
    ui.searchPropsCheckBox->setChecked(settings.value("searchProps").toBool());
    ui.wrapAroundCheckBox->setChecked(settings.value("wrapAround", true).toBool());
    settings.endGroup();
}

void FindItemsDialog::saveSettings()
{
    QStringList history; // save 10 strings max
    for (int i = 0; i < ui.searchComboBox->count() && i < 10; ++i)
        history += ui.searchComboBox->itemText(i);

    QSettings settings;
    settings.beginGroup("findDialog");
    settings.setValue("pos", pos());
    settings.setValue("searchHistory", history);
    settings.setValue("caseSensitive", ui.caseSensitiveCheckBox->isChecked());
    settings.setValue("regexMinimalMatch", ui.minimalMatchCheckBox->isChecked());
    settings.setValue("regexMultilineMatch", ui.multilineMatchCheckBox->isChecked());
    settings.setValue("regex", ui.regexCheckBox->isChecked());
    settings.setValue("searchProps", ui.searchPropsCheckBox->isChecked());
    settings.setValue("wrapAround", ui.wrapAroundCheckBox->isChecked());
    settings.endGroup();
}

void FindItemsDialog::updateWindowTitle()
{
    QString title = tr("Find items");
    if (_searchPerformed)
        title += QString(" [%1/%2]").arg(_currentIndex + 1).arg(_searchResult.size());
    setWindowTitle(title);
}

void FindItemsDialog::changeItem(bool changeResultsSelection /*= true*/)
{
    ItemInfo *item = _searchResult[_currentIndex].first;
    emit itemFound(item);
    updateWindowTitle();

    if (changeResultsSelection && _resultsWidget)
        _resultsWidget->selectItem(item);
}

void FindItemsDialog::setButtonsDisabled(bool disabled, bool updateResultButton /*= true*/)
{
    ui.nextButton->setDisabled(disabled);
    ui.previousButton->setDisabled(disabled);
    if (updateResultButton)
        ui.searchResultsButton->setDisabled(disabled);
}

void FindItemsDialog::resetSearchStatus()
{
    _searchPerformed = false;
    setButtonsDisabled(ui.searchComboBox->currentText().isEmpty(), false);
}

void FindItemsDialog::showEvent(QShowEvent *e)
{
    if (ui.searchComboBox->currentIndex() == -1 || ui.searchComboBox->currentText().isEmpty())
        ui.searchComboBox->setCurrentIndex(0);
    ui.searchComboBox->lineEdit()->selectAll();
}
