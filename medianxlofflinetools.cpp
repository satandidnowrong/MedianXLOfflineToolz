#include "medianxlofflinetools.h"
#include "colorsmanager.hpp"
#include "qd2charrenamer.h"
#include "helpers.h"
#include "itemsviewerdialog.h"
#include "itemdatabase.h"
#include "propertiesviewerwidget.h"
#include "finditemsdialog.h"
#include "resourcepathmanager.hpp"
#include "reversebitwriter.h"
#include "itemparser.h"
#include "reversebitreader.h"
#include "itemspropertiessplitter.h"
#include "characterinfo.hpp"
#include "skillplandialog.h"
#include "fileassociationmanager.h"
#include "messagecheckbox.h"
#include "experienceindicatorgroupbox.h"

#include <QCloseEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QGridLayout>
#include <QFileDialog>
#include <QLabel>

#include <QSettings>
#include <QFile>
#include <QDataStream>
#include <QTranslator>
#include <QUrl>
#include <QTimer>
#include <QDate>

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <cmath>


// additional defines

#ifdef QT_NO_DEBUG
#define RELEASE_DATE "26.08.2012" // TODO: don't forget to change
#else
#include <QDebug>

#define RELEASE_DATE QDate::currentDate().toString("dd.MM.yyyy") // :)
#endif

//#define MAKE_HC
//#define ENABLE_PERSONALIZE
//#define MAKE_FINISHED_CHARACTER
//#define DISABLE_CRC_CHECK


// static const

static const QString kLastSavePathKey("lastSavePath"), kBackupExtension("bak"), kReadonlyCss("background-color: rgb(227, 227, 227)"), kTimeFormatReadable("yyyyMMdd-hhmmss");
static const QByteArray kMercHeader("jf");

const QString MedianXLOfflineTools::kCompoundFormat("%1, %2");
const QString MedianXLOfflineTools::kCharacterExtension("d2s");
const QString MedianXLOfflineTools::kCharacterExtensionWithDot("." + kCharacterExtension);
const quint32 MedianXLOfflineTools::kFileSignature = 0xAA55AA55;
const int MedianXLOfflineTools::kSkillsNumber = 30;
const int MedianXLOfflineTools::kDifficultiesNumber = 3;
const int MedianXLOfflineTools::kStatPointsPerLevel = 5;
const int MedianXLOfflineTools::kSkillPointsPerLevel = 1;
const int MedianXLOfflineTools::kMaxRecentFiles = 10;


// ctor

MedianXLOfflineTools::MedianXLOfflineTools(const QString &cmdPath, QWidget *parent, Qt::WFlags flags) : QMainWindow(parent, flags), _findItemsDialog(0), _backupLimitsGroup(new QActionGroup(this)), _backupFormatsGroup(new QActionGroup(this)),
    hackerDetected(tr("1337 hacker detected! Please, play legit.")), //difficulties(QStringList() << tr("Hatred") << tr("Terror") << tr("Destruction")),
    maxValueFormat(tr("Max: %1")), minValueFormat(tr("Min: %1")), investedValueFormat(tr("Invested: %1")),
    kForumThreadHtmlLinks(tr("<a href=\"http://modsbylaz.14.forumer.com/viewtopic.php?t=23147\">Official Median XL Forum thread</a><br>"
                             "<a href=\"http://forum.worldofplayers.ru/showthread.php?t=34489\">Official Russian Median XL Forum thread</a>")), _isLoaded(false)
{
    ui.setupUi(this);

    ui.actionBackups1->setData(1);
    ui.actionBackups2->setData(2);
    ui.actionBackups5->setData(5);
    ui.actionBackups10->setData(10);

    _backupLimitsGroup->setExclusive(true);
    _backupLimitsGroup->addAction(ui.actionBackups1);
    _backupLimitsGroup->addAction(ui.actionBackups2);
    _backupLimitsGroup->addAction(ui.actionBackups5);
    _backupLimitsGroup->addAction(ui.actionBackups10);
    _backupLimitsGroup->addAction(ui.actionBackupsUnlimited);

    ui.actionBackupFormatReadable ->setText(tr("<filename>_<%1>", "param is date format expressed in yyyy, MM, hh, etc.").arg(kTimeFormatReadable) + "." + kBackupExtension);
    ui.actionBackupFormatTimestamp->setText(tr("<filename>_<UNIX timestamp>") + "." + kBackupExtension);

    _backupFormatsGroup->setExclusive(true);
    _backupFormatsGroup->addAction(ui.actionBackupFormatReadable);
    _backupFormatsGroup->addAction(ui.actionBackupFormatTimestamp);

    ui.actionFindNext->setShortcut(QKeySequence::FindNext);
    ui.actionFindPrevious->setShortcut(QKeySequence::FindPrevious);

    // TODO: [0.4] remove when implementing export info
    ui.menuExport->removeAction(ui.actionExportCharacterInfo);
    ui.mainToolBar->removeAction(ui.actionExportCharacterInfo);

#ifdef Q_WS_WIN32
    setAppUserModelID(); // is actually used only in Windows 7 and later
#endif

    loadData();
    createLanguageMenu();
    createLayout();
    loadSettings();
    fillMaps();
    connectSignals();

#if defined(Q_WS_WIN32) || defined(Q_WS_MACX)
    bool isDefault = FileAssociationManager::isApplicationDefaultForExtension(kCharacterExtensionWithDot);
    if (!isDefault)
    {
        if (ui.actionCheckFileAssociations->isChecked())
        {
            MessageCheckBox box(tr("%1 is not associated with %2 files.\n\nDo you want to do it?").arg(qApp->applicationName(), kCharacterExtensionWithDot), ui.actionCheckFileAssociations->text(), this);
            box.setChecked(true);
            int result = box.exec();
            ui.actionCheckFileAssociations->setChecked(box.isChecked());
            if (result)
            {
                FileAssociationManager::makeApplicationDefaultForExtension(kCharacterExtensionWithDot);
                isDefault = true;
            }
        }
    }

    updateAssociateAction(isDefault);
#else
#warning Add implementation to check file association to e.g. fileassociationmanager_linux.cpp or comment this line
#endif

#ifdef Q_WS_WIN32
    syncWindowsTaskbarRecentFiles(); // is actually used only in Windows 7 and later
#endif

    if (ui.actionCheckForUpdateOnStart->isChecked())
        checkForUpdate();

#ifdef Q_WS_MACX
    QTimer::singleShot(500, this, SLOT(moveUpdateActionToAppleMenu())); // needs a slight delay to create menu
#endif

    bool didModVersionChange = SkillplanDialog::didModVersionChange(); // must be called before the following conditions because it should load planner/readable versions
    if (!cmdPath.isEmpty())
        loadFile(cmdPath);
    else if (ui.actionLoadLastUsedCharacter->isChecked() && !_recentFilesList.isEmpty() && !didModVersionChange)
        loadFile(_recentFilesList.at(0));
    else
    {
#ifdef Q_WS_WIN32
        QSettings settings;
        settings.beginGroup("recentItems");
        if (!settings.contains(kLastSavePathKey))
        {
            QSettings d2Settings("HKEY_CURRENT_USER\\Software\\Blizzard Entertainment\\Diablo II", QSettings::NativeFormat);
            QString d2SavePath = d2Settings.value("Save Path").toString();
            if (!d2SavePath.isEmpty())
                settings.setValue(kLastSavePathKey, d2SavePath);
        }
#endif
        updateWindowTitle();
    }
}


// slots

bool MedianXLOfflineTools::loadFile(const QString &charPath)
{
    bool unsupportedFile = !charPath.endsWith(kCharacterExtensionWithDot);
    if (charPath.isEmpty() || unsupportedFile || !maybeSave())
    {
        if (!charPath.isEmpty() && unsupportedFile)
            ERROR_BOX(tr("'%1' files are not supported", "param is file extension").arg(charPath.right(4)));
        return false;
    }

    // don't call slot a lot of times while loading character
    disconnect(ui.mercTypeComboBox);
    disconnect(ui.mercNameComboBox);

    _charPath = charPath;
    bool result;
    if ((result = processSaveFile()))
    {
        addToRecentFiles();
        updateUI();

        raise();
        activateWindow();

        // it is here because currentIndexChanged signal is emitted when items are added to the combobox
        connect(ui.mercTypeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(modify()));
        connect(ui.mercNameComboBox, SIGNAL(currentIndexChanged(int)), SLOT(modify()));

        if (_itemsDialog)
        {
            _itemsDialog->updateItems(getPlugyStashesExistenceHash(), true);
            _itemsDialog->updateButtonsState();
        }
        if (_itemsDialog || ui.actionOpenItemsAutomatically->isChecked())
            showItems();

        QSettings settings;
        settings.beginGroup("recentItems");
        settings.setValue(kLastSavePathKey, QDir::toNativeSeparators(QFileInfo(_charPath).canonicalPath()));
    }
    else
    {
        _saveFileContents.clear();
        _charPath.clear();

        clearUI();
        updateWindowTitle();
    }

    setModified(false);
#ifdef MAKE_FINISHED_CHARACTER
    ui.actionSaveCharacter->setEnabled(true);
#endif

    if (_findItemsDialog)
        _findItemsDialog->clearResults();

    return result;
}

void MedianXLOfflineTools::switchLanguage(QAction *languageAction)
{
    QByteArray newLocale = languageAction->statusTip().toLatin1();
    if (LanguageManager::instance().currentLocale != newLocale)
    {
        LanguageManager::instance().currentLocale = newLocale;

        static bool wasMessageBoxShown = false;
        if (!wasMessageBoxShown)
        {
            INFO_BOX(tr("Language will be changed next time you run the application"));
            wasMessageBoxShown = true;
        }
    }
}

void MedianXLOfflineTools::setModified(bool modified)
{
    setWindowModified(modified);
    ui.actionSaveCharacter->setEnabled(modified);
}

void MedianXLOfflineTools::eatSignetsOfLearning(int signetsEaten)
{
    int newSignetsEaten = ui.signetsOfLearningEatenLineEdit->text().toInt() + signetsEaten;
    ui.signetsOfLearningEatenLineEdit->setText(QString::number(newSignetsEaten));
    CharacterInfo::instance().basicInfo.statsDynamicData.setProperty("SignetsOfLearningEaten", newSignetsEaten);

    foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
        spinBox->setMaximum(spinBox->maximum() + signetsEaten);

    ui.freeStatPointsLineEdit->setText(QString::number(ui.freeStatPointsLineEdit->text().toInt() + signetsEaten));
    QString s = ui.freeStatPointsLineEdit->statusTip();
    int start = s.indexOf(": ") + 2, end = s.indexOf(","), total = s.mid(start, end - start).toInt();
    updateMaxCompoundStatusTip(ui.freeStatPointsLineEdit, total + signetsEaten, investedStatPoints());
}

void MedianXLOfflineTools::loadCharacter()
{
    QSettings settings;
    settings.beginGroup("recentItems");
    QString lastSavePath = settings.value(kLastSavePathKey).toString();

    QString charPath = QFileDialog::getOpenFileName(this, tr("Load Character"), lastSavePath, tr("Diablo 2 Save Files") + QString(" (*%1)").arg(kCharacterExtensionWithDot));
    if (loadFile(QDir::toNativeSeparators(charPath)))
        ui.statusBar->showMessage(tr("Character loaded"), 3000);
}

void MedianXLOfflineTools::openRecentFile()
{
    loadFile(qobject_cast<QAction *>(sender())->statusTip());
}

void MedianXLOfflineTools::reloadCharacter(bool notify /*= true*/)
{
    if (loadFile(_charPath) && notify)
        ui.statusBar->showMessage(tr("Character reloaded"), 3000);
}

void MedianXLOfflineTools::saveCharacter()
{
    CharacterInfo &charInfo = CharacterInfo::instance();
#ifdef MAKE_FINISHED_CHARACTER
    charInfo.basicInfo.level = 120;
    ui.levelSpinBox->setMaximum(120);
    ui.levelSpinBox->setValue(120);
    charInfo.basicInfo.level = 1;
    ui.freeSkillPointsLineEdit->setText("134");
    ui.freeStatPointsLineEdit->setText("1110");
    ui.signetsOfLearningEatenLineEdit->setText("500");
    ui.signetsOfSkillEatenLineEdit->setText("3");
#endif

    QByteArray statsBytes = statisticBytes();
    if (statsBytes.isEmpty())
        return;

    QByteArray tempFileContents(_saveFileContents);
    tempFileContents.replace(Enums::Offsets::StatsData, charInfo.skillsOffset - Enums::Offsets::StatsData, statsBytes);
    int diff = Enums::Offsets::StatsData + statsBytes.size() - charInfo.skillsOffset;
    charInfo.skillsOffset = Enums::Offsets::StatsData + statsBytes.size();
    charInfo.itemsOffset += diff;
    charInfo.itemsEndOffset += diff;

    if (ui.respecSkillsCheckBox->isChecked())
        tempFileContents.replace(charInfo.skillsOffset + 2, kSkillsNumber, QByteArray(kSkillsNumber, 0));

#ifndef MAKE_FINISHED_CHARACTER
    if (ui.activateWaypointsCheckBox->isChecked())
#endif
    {
        QByteArray activatedWaypointsBytes(5, 0xFF); // 40 x '1'
        for (int startPos = Enums::Offsets::WaypointsData + 2, i = 0; i < kDifficultiesNumber; ++i, startPos += 24)
            tempFileContents.replace(startPos, activatedWaypointsBytes.size(), activatedWaypointsBytes);
    }

    if (ui.convertToSoftcoreCheckBox->isChecked())
        charInfo.basicInfo.isHardcore = false;

#ifdef MAKE_HC
    charInfo.basicInfo.isHardcore = true;
    charInfo.basicInfo.hadDied = false;
#endif
    char statusValue = tempFileContents[Enums::Offsets::Status];
    if (charInfo.basicInfo.hadDied)
        statusValue |= Enums::StatusBits::HadDied;
    else
        statusValue &= ~Enums::StatusBits::HadDied;
    if (charInfo.basicInfo.isHardcore)
        statusValue |= Enums::StatusBits::IsHardcore;
    else
        statusValue &= ~Enums::StatusBits::IsHardcore;
    tempFileContents[Enums::Offsets::Status] = statusValue;

    QDataStream outputDataStream(&tempFileContents, QIODevice::ReadWrite);
    outputDataStream.setByteOrder(QDataStream::LittleEndian);

    //quint8 curDiff[difficultiesNumber] = {0, 0, 0};
    //curDiff[ui.currentDifficultyComboBox->currentIndex()] = 128 + ui.currentActSpinBox->value() - 1; // 10000xxx
    //outputDataStream.skipRawData(Enums::Offsets::CurrentLocation);
    //for (int i = 0; i < difficultiesNumber; ++i)
    //    outputDataStream << curDiff[i];

#ifdef MAKE_FINISHED_CHARACTER
    outputDataStream.skipRawData(Enums::Offsets::Progression);
    outputDataStream << static_cast<quint16>(15); // become (m|p)atriarch

    quint16 one = 1;
    // enable all acts in all difficulties
    for (int i = 0; i < kDifficultiesNumber; ++i)
    {
        outputDataStream.device()->seek(Enums::Offsets::QuestsData + i * Enums::Quests::Size); // A1 (0)
        outputDataStream << one;
        outputDataStream.skipRawData(6);  // Cain (8)
        outputDataStream << one;
        outputDataStream.skipRawData(4);  // A2 (14)
        outputDataStream << one;
        outputDataStream.skipRawData(14); // A3 (30)
        outputDataStream << one;
        outputDataStream.skipRawData(12); // Mephisto (44) + A4 (46)
        outputDataStream << one << one;
        outputDataStream.skipRawData(4);  // A4Q3 (52)
        outputDataStream << one;
        outputDataStream.skipRawData(2);  // A5 (56)
        outputDataStream << one;
    }

    // set current difficulty to Destruction and act to 5
    outputDataStream.device()->seek(Enums::Offsets::CurrentLocation);
    outputDataStream << static_cast<quint8>(0);   // Hatred
    outputDataStream << static_cast<quint8>(0);   // Terror
    outputDataStream << static_cast<quint8>(132); // Destruction (10000100)

    // complete skill/stat points quests
    quint16 questComplete = static_cast<quint16>(0) | Enums::Quests::IsCompleted;
    for (int i = 0; i < kDifficultiesNumber; ++i)
    {
        int baseOffset = Enums::Offsets::QuestsData + i * Enums::Quests::Size;
        outputDataStream.device()->seek(baseOffset + Enums::Quests::DenOfEvil);
        outputDataStream << questComplete;
        outputDataStream.device()->seek(baseOffset + Enums::Quests::Radament);
        outputDataStream << questComplete;
        outputDataStream.device()->seek(baseOffset + Enums::Quests::LamEsensTome);
        outputDataStream << questComplete;
        outputDataStream.device()->seek(baseOffset + Enums::Quests::Izual);
        outputDataStream << questComplete;
    }
#endif
    
#ifdef ENABLE_PERSONALIZE
    for (int i = 0; i < kDifficultiesNumber; ++i)
    {
        outputDataStream.device()->seek(Enums::Offsets::QuestsData + i * Enums::Quests::Size + Enums::Quests::Nihlathak);
        outputDataStream << quint16(Enums::Quests::IsTaskDone);
    }
#endif

    QString newName = charInfo.basicInfo.newName;
    bool hasNameChanged = !newName.isEmpty() && charInfo.basicInfo.originalName != newName;
    if (hasNameChanged)
    {
        outputDataStream.device()->seek(Enums::Offsets::Name);
#ifdef Q_WS_MACX
        QByteArray newNameByteArray = ColorsManager::macTextCodec()->fromUnicode(newName);
#else
        QByteArray newNameByteArray = newName.toLocal8Bit();
#endif
        newNameByteArray += QByteArray(QD2CharRenamer::kMaxNameLength + 1 - newName.length(), '\0'); // add trailing nulls
        outputDataStream.writeRawData(newNameByteArray.constData(), newNameByteArray.length());
    }
    else
        newName = charInfo.basicInfo.originalName;

    quint8 newClvl = ui.levelSpinBox->value();
#ifndef MAKE_FINISHED_CHARACTER
    if (charInfo.basicInfo.level != newClvl)
#endif
    {
        charInfo.basicInfo.level = newClvl;
        charInfo.basicInfo.totalSkillPoints = ui.freeSkillPointsLineEdit->text().toUShort();
        recalculateStatPoints();

        outputDataStream.device()->seek(Enums::Offsets::Level);
        outputDataStream << newClvl;

        ui.levelSpinBox->setMaximum(newClvl);
    }

    if (charInfo.mercenary.exists)
    {
        quint16 newMercValue = Enums::Mercenary::mercBaseValueFromCode(charInfo.mercenary.code) + ui.mercTypeComboBox->currentIndex();
        charInfo.mercenary.code = Enums::Mercenary::mercCodeFromValue(newMercValue);
        charInfo.mercenary.nameIndex = ui.mercNameComboBox->currentIndex();
        outputDataStream.device()->seek(Enums::Offsets::Mercenary + 4);
        outputDataStream << charInfo.mercenary.nameIndex << newMercValue;
    }

    // put corpse items on a character
    ItemsList corpseItems = ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Corpse), equippedItems = ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Equipped);
    for (int i = 0; i < corpseItems.size(); ++i)
    {
        ItemInfo *item = corpseItems[i];
        bool found = false;
        foreach (ItemInfo *equippedItem, equippedItems)
        {
            if (equippedItem->whereEquipped == item->whereEquipped)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            item->location = Enums::ItemLocation::Equipped;
            corpseItems.removeAt(i--);
        }
    }

    int characterItemsSize = 2, mercItemsSize = 0, corpseItemsSize = 0;
    ItemsList characterItems, mercItems;
    QHash<Enums::ItemStorage::ItemStorageEnum, ItemsList> plugyItemsHash;
    foreach (ItemInfo *item, charInfo.items.character)
    {
        switch (item->storage)
        {
        case Enums::ItemStorage::PersonalStash: case Enums::ItemStorage::SharedStash: case Enums::ItemStorage::HCStash:
            plugyItemsHash[static_cast<Enums::ItemStorage::ItemStorageEnum>(item->storage)] += item;
            break;
        default:
        {
            int *pItemsSize;
            ItemsList *pItems;
            switch (item->location)
            {
            case Enums::ItemLocation::Merc:
                pItemsSize = &mercItemsSize;
                pItems = &mercItems;
                item->location = Enums::ItemLocation::Equipped;
                break;
            case Enums::ItemLocation::Corpse:
                pItemsSize = &corpseItemsSize;
                pItems = 0;
                break;
            default:
                pItemsSize = &characterItemsSize;
                pItems = &characterItems;
                break;
            }

            if (pItems)
                pItems->append(item);
            *pItemsSize += 2 + item->bitString.length() / 8; // JM + item bytes
            foreach (ItemInfo *socketableItem, item->socketablesInfo)
                *pItemsSize += 2 + socketableItem->bitString.length() / 8; // JM + item bytes
            break;
        }
        }
    }

    // write character items
    tempFileContents.replace(charInfo.itemsOffset, charInfo.itemsEndOffset - charInfo.itemsOffset, QByteArray(characterItemsSize, 0));
    outputDataStream.device()->seek(charInfo.itemsOffset);
    outputDataStream << static_cast<quint16>(characterItems.size());
    ItemParser::writeItems(characterItems, outputDataStream);

    // write corpse items
    int mercItemsOffset = tempFileContents.lastIndexOf(kMercHeader);
    outputDataStream.skipRawData(2); // JM
    if (!corpseItems.isEmpty())
        outputDataStream.skipRawData(2 + 12 + 2); // number of corpses + unknown corpse data + JM
    outputDataStream << static_cast<quint16>(corpseItems.size());
    int pos = outputDataStream.device()->pos();
    tempFileContents.replace(pos, mercItemsOffset - pos, QByteArray(corpseItemsSize, 0));
    ItemParser::writeItems(corpseItems, outputDataStream);

    // write merc items
    Q_ASSERT(tempFileContents.mid(outputDataStream.device()->pos(), 2) == kMercHeader);
    if (charInfo.mercenary.exists)
    {
        outputDataStream.skipRawData(2 + 2); // jf + JM
        outputDataStream << static_cast<quint16>(mercItems.size());
        int terminatorOffset = tempFileContents.size() - 3;
        pos = outputDataStream.device()->pos();
        tempFileContents.replace(pos, terminatorOffset - pos, QByteArray(mercItemsSize, 0));
        ItemParser::writeItems(mercItems, outputDataStream);
    }

    // write file size & checksum
    quint32 fileSize = tempFileContents.size();
    outputDataStream.device()->seek(Enums::Offsets::FileSize);
    outputDataStream << fileSize;
    outputDataStream << checksum(tempFileContents);

    // save plugy stashes if changed
    for (QHash<Enums::ItemStorage::ItemStorageEnum, PlugyStashInfo>::iterator iter = _plugyStashesHash.begin(); iter != _plugyStashesHash.end(); ++iter)
    {
        const ItemsList &items = plugyItemsHash[iter.key()];
        if (std::find_if(items.constBegin(), items.constEnd(), hasChanged) == items.constEnd())
            continue;

        PlugyStashInfo &info = iter.value();
        QFile inputFile(info.path);
        if (!inputFile.exists())
            continue;

        backupFile(inputFile);
        if (!inputFile.open(QIODevice::WriteOnly))
        {
            showErrorMessageBoxForFile(tr("Error creating file '%1'"), inputFile);
            continue;
        }

        ItemsList::const_iterator maxPageIter = std::max_element(items.constBegin(), items.constEnd(), compareItemsByPlugyPage);
        info.lastPage = maxPageIter == items.constEnd() ? 0 : (*maxPageIter)->plugyPage;

        QDataStream plugyFileDataStream(&inputFile);
        plugyFileDataStream.setByteOrder(QDataStream::LittleEndian);
        plugyFileDataStream.writeRawData(info.header.constData(), info.header.size());
        plugyFileDataStream << info.version;
        if (info.hasGold)
            plugyFileDataStream << info.gold;
        plugyFileDataStream << info.lastPage;

        for (quint32 page = 1; page <= info.lastPage; ++page)
        {
            ItemsList pageItems;
            foreach (ItemInfo *anItem, items)
                if (anItem->plugyPage == page)
                    pageItems += anItem;

            plugyFileDataStream.writeRawData(ItemParser::kPlugyPageHeader.constData(), ItemParser::kPlugyPageHeader.size() + 1); // write '\0'
            plugyFileDataStream.writeRawData(ItemParser::kItemHeader.constData(), ItemParser::kItemHeader.size()); // do not write '\0'
            plugyFileDataStream << static_cast<quint16>(pageItems.size());
            ItemParser::writeItems(pageItems, plugyFileDataStream);
        }
    }

    // save the character
    QSettings settings;
    settings.beginGroup("recentItems");
    QString savePath = settings.value(kLastSavePathKey).toString() + QDir::separator(), fileName = savePath + newName, saveFileName = fileName + kCharacterExtensionWithDot;
    QFile outputFile(saveFileName);
    if (hasNameChanged)
    {
        QFile oldFile(QString("%1%2.%3").arg(savePath, charInfo.basicInfo.originalName, kCharacterExtension));
        backupFile(oldFile);
    }
    else
        backupFile(outputFile);

    if (outputFile.open(QIODevice::WriteOnly))
    {
        int bytesWritten = outputFile.write(tempFileContents);
        if (bytesWritten == static_cast<int>(fileSize)) // shut the compiler up
        {
            outputFile.flush();
            _saveFileContents = tempFileContents;

            if (hasNameChanged)
            {
                // delete .d2s and rename all other related files like .d2x, .key, .ma0, etc.
                bool isOldNameEmpty = QRegExp(QString("[ %1]+").arg(QChar(QChar::Nbsp))).exactMatch(charInfo.basicInfo.originalName);
                bool hasNonAsciiChars = false;
                for (int i = 0; i < charInfo.basicInfo.originalName.length(); ++i)
                    if (charInfo.basicInfo.originalName.at(i).unicode() > 255)
                    {
                        hasNonAsciiChars = true;
                        break;
                    }

                bool isStrangeName = hasNonAsciiChars || isOldNameEmpty;
                QDir sourceFileDir(savePath, isStrangeName ? "*" : charInfo.basicInfo.originalName + ".*");
                foreach (const QFileInfo &fileInfo, sourceFileDir.entryInfoList())
                {
                    QString extension = fileInfo.suffix();
                    if ((isStrangeName && fileInfo.baseName() != charInfo.basicInfo.originalName) || extension == kBackupExtension)
                        continue;

                    QFile sourceFile(fileInfo.canonicalFilePath());
                    if (extension == kCharacterExtension) // delete
                    {
                        if (!sourceFile.remove())
                            showErrorMessageBoxForFile(tr("Error removing file '%1'"), sourceFile);
                    }
                    else // rename
                    {
                        if (!sourceFile.rename(fileName + "." + extension) && !isOldNameEmpty)
                            showErrorMessageBoxForFile(tr("Error renaming file '%1'"), sourceFile);
                    }
                }

                charInfo.basicInfo.originalName = newName;
#ifdef Q_WS_WIN32
                removeFromWindowsRecentFiles(_recentFilesList.at(0)); // old file doesn't exist any more
#endif
                _recentFilesList[0] = saveFileName;
                updateRecentFilesActions();
            }

            _charPath = saveFileName;
            setModified(false);
            reloadCharacter(false); // update all UI at once by reloading the file

            INFO_BOX(tr("File '%1' successfully saved!").arg(QDir::toNativeSeparators(saveFileName)));
        }
        else
            showErrorMessageBoxForFile(tr("Error writing file '%1'"), outputFile);
    }
    else
        showErrorMessageBoxForFile(tr("Error creating file '%1'"), outputFile);
}

void MedianXLOfflineTools::statChanged(int newValue)
{
    QSpinBox *senderSpinBox = qobject_cast<QSpinBox *>(sender());
    int minimum = senderSpinBox->minimum();
    updateMinCompoundStatusTip(senderSpinBox, minimum, newValue - minimum);

    int *pOldValue = &_oldStatValues[_spinBoxesStatsMap.key(senderSpinBox)], diff = newValue - *pOldValue;
    if (_isLoaded && diff)
    {
        *pOldValue = newValue;

        ui.freeStatPointsLineEdit->setText(QString::number(ui.freeStatPointsLineEdit->text().toUInt() - diff));
        QString s = ui.freeStatPointsLineEdit->statusTip();
        int start = s.indexOf(": ") + 2, end = s.indexOf(","), total = s.mid(start, end - start).toInt();
        updateMaxCompoundStatusTip(ui.freeStatPointsLineEdit, total, investedStatPoints());

        foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
            if (spinBox != senderSpinBox)
                spinBox->setMaximum(spinBox->maximum() - diff);
        // explicitly update status bar
        QStatusTipEvent event(senderSpinBox->statusTip());
        qApp->sendEvent(senderSpinBox, &event);

        updateTableStats(_baseStatsMap[CharacterInfo::instance().basicInfo.classCode].statsPerPoint, diff, senderSpinBox);
        setModified(true);
    }
}

void MedianXLOfflineTools::respecStats()
{
    for (int i = Enums::CharacterStats::Strength; i <= Enums::CharacterStats::Vitality; ++i)
    {
        Enums::CharacterStats::StatisticEnum statCode = static_cast<Enums::CharacterStats::StatisticEnum>(i);
        int baseStat = _baseStatsMap[CharacterInfo::instance().basicInfo.classCode].statsAtStart.statFromCode(statCode);
        _spinBoxesStatsMap[statCode]->setValue(baseStat);
    }
    ui.statusBar->clearMessage();
    setModified(true);
}

void MedianXLOfflineTools::respecSkills(bool shouldRespec)
{
    quint16 skills = CharacterInfo::instance().basicInfo.totalSkillPoints;
    quint32 freeSkills = CharacterInfo::instance().valueOfStatistic(Enums::CharacterStats::FreeSkillPoints);
    ui.freeSkillPointsLineEdit->setText(QString::number(shouldRespec ? skills : freeSkills));
    updateMaxCompoundStatusTip(ui.freeSkillPointsLineEdit, skills, shouldRespec ? 0 : skills - freeSkills);
    setModified(true);
}

void MedianXLOfflineTools::rename()
{
    QString &newName = CharacterInfo::instance().basicInfo.newName;
    QD2CharRenamer renameWidget(newName, ui.actionWarnWhenColoredName->isChecked(), this);
    if (renameWidget.exec())
    {
        newName = renameWidget.name();
        QD2CharRenamer::updateNamePreview(ui.charNamePreview, newName);
        setModified(true);
    }
}

void MedianXLOfflineTools::levelChanged(int newClvl)
{
    int lvlDiff = _oldClvl - newClvl;
    const CharacterInfo::CharacterInfoBasic &basicInfo = CharacterInfo::instance().basicInfo;
    if (_isLoaded && qAbs(lvlDiff) < basicInfo.level)
    {
        updateTableStats(_baseStatsMap[basicInfo.classCode].statsPerLevel, -lvlDiff);

        _oldClvl = newClvl;
        int statsDiff = lvlDiff * kStatPointsPerLevel, newFreeStats = ui.freeStatPointsLineEdit->text().toInt() - statsDiff;
        if (newFreeStats < 0)
        {
            respecStats();
            newFreeStats = ui.freeStatPointsLineEdit->text().toInt() - statsDiff;
        }
        ui.freeStatPointsLineEdit->setText(QString::number(newFreeStats));
        foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
            spinBox->setMaximum(spinBox->maximum() - statsDiff);

        bool hasLevelChanged = newClvl != basicInfo.level;
        if (_resurrectionPenalty != ResurrectPenaltyDialog::Skills)
        {
            ui.respecSkillsCheckBox->setChecked(hasLevelChanged);
            ui.respecSkillsCheckBox->setDisabled(hasLevelChanged);
        }

        int newSkillPoints = basicInfo.totalSkillPoints, investedSkillPoints = 0;
        if (hasLevelChanged)
        {
            newSkillPoints -= (basicInfo.level - newClvl) * kSkillPointsPerLevel;
            ui.freeSkillPointsLineEdit->setText(QString::number(newSkillPoints));
        }
        else if (_resurrectionPenalty == ResurrectPenaltyDialog::Skills)
            respecSkills(true);
        else
            investedSkillPoints = newSkillPoints - CharacterInfo::instance().valueOfStatistic(Enums::CharacterStats::FreeSkillPoints);

        int investedStats = investedStatPoints();
        updateStatusTips(newFreeStats + investedStats, investedStats, newSkillPoints, investedSkillPoints);

        updateCharacterExperienceProgressbar(experienceTable.at(newClvl - 1));
    }
}

void MedianXLOfflineTools::resurrect()
{
    CharacterInfo::CharacterInfoBasic &basicInfo = CharacterInfo::instance().basicInfo;
    ResurrectPenaltyDialog dlg(this);
    if (dlg.exec())
    {
        basicInfo.hadDied = false;
        updateHardcoreUIElements();
        updateCharacterTitle(true);

        ui.levelSpinBox->setMaximum(basicInfo.level);
        ui.levelSpinBox->setValue(basicInfo.level);

        _resurrectionPenalty = dlg.resurrectionPenalty();
        switch (_resurrectionPenalty)
        {
        case ResurrectPenaltyDialog::Levels:
        {
            int newLevel = basicInfo.level - ResurrectPenaltyDialog::kLevelPenalty;
            if (newLevel < 1)
                newLevel = 1;
            ui.levelSpinBox->setMaximum(newLevel); // setValue() is invoked implicitly

            break;
        }
        case ResurrectPenaltyDialog::Skills:
        {
            if (!ui.respecSkillsCheckBox->isChecked())
                ui.respecSkillsCheckBox->setChecked(true);
            ui.respecSkillsCheckBox->setDisabled(true);

            ushort newSkillPoints = ui.freeSkillPointsLineEdit->text().toUShort();
            newSkillPoints -= newSkillPoints * ResurrectPenaltyDialog::kSkillPenalty;
            basicInfo.totalSkillPoints = newSkillPoints;

            ui.freeSkillPointsLineEdit->setText(QString::number(newSkillPoints));
            updateMaxCompoundStatusTip(ui.freeSkillPointsLineEdit, newSkillPoints, 0);

            break;
        }
        case ResurrectPenaltyDialog::Stats:
        {
            respecStats();

            ushort newStatPoints = ui.freeStatPointsLineEdit->text().toUShort(), diff = newStatPoints * ResurrectPenaltyDialog::kStatPenalty;
            newStatPoints -= diff;
            basicInfo.totalStatPoints = newStatPoints;

            ui.freeStatPointsLineEdit->setText(QString::number(newStatPoints));
            updateMaxCompoundStatusTip(ui.freeStatPointsLineEdit, newStatPoints, 0);

            foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
                spinBox->setMaximum(spinBox->maximum() - diff);

            break;
        }
        default:
            break;
        }

        // for Ultimative, don't allow resurrected characters stay on hardcore
        if (isUltimative())
            convertToSoftcore(true);
        else
            setModified(true);
    }
}

void MedianXLOfflineTools::convertToSoftcore(bool isSoftcore)
{
    updateCharacterTitle(!isSoftcore);
    setModified(true);
}

//void MedianXLOfflineTools::currentDifficultyChanged(int newDifficulty)
//{
//    quint8 progression = CharacterInfo::instance().basicInfo.titleCode, maxDifficulty = ui.currentDifficultyComboBox->count() - 1, maxAct;
//    if (progression == Enums::Progression::Completed - 1 || newDifficulty != maxDifficulty)
//        maxAct = 5;
//    else
//    {
//        maxAct = 1 + (progression - maxDifficulty) % 4;
//        if (maxAct == 4)
//        {
//            // check if A5 is enabled
//            if (_saveFileContents.at(Enums::Offsets::QuestsData + newDifficulty * Enums::Quests::Size + Enums::Quests::Act5Offset))
//                ++maxAct;
//        }
//    }
//    ui.currentActSpinBox->setMaximum(maxAct);
//}

void MedianXLOfflineTools::findItem()
{
    if (!_findItemsDialog)
    {
        _findItemsDialog = new FindItemsDialog(this);
        connect(ui.actionFindNext, SIGNAL(triggered()), _findItemsDialog, SLOT(findNext()));
        connect(ui.actionFindPrevious, SIGNAL(triggered()), _findItemsDialog, SLOT(findPrevious()));
        connect(_findItemsDialog, SIGNAL(itemFound(ItemInfo *)), SLOT(showFoundItem(ItemInfo *)));
    }
    _findItemsDialog->show();
    _findItemsDialog->activateWindow();
}

void MedianXLOfflineTools::showFoundItem(ItemInfo *item)
{
    ui.actionFindNext->setDisabled(!item);
    ui.actionFindPrevious->setDisabled(!item);
    if (item)
    {
        showItems(false);
        _itemsDialog->showItem(item);
    }
}

void MedianXLOfflineTools::showItems(bool activate /*= true*/)
{
    if (_itemsDialog)
    {
        if (_itemsDialog->isMinimized())
            _itemsDialog->isMaximized() ? _itemsDialog->showMaximized() : _itemsDialog->showNormal(); // this is not a mistake: window can indeed be minimized and maximized at the same time
        if (activate)
            _itemsDialog->activateWindow();
    }
    else
    {
        _itemsDialog = new ItemsViewerDialog(getPlugyStashesExistenceHash(), this);
        connect(_itemsDialog->tabWidget(), SIGNAL(currentChanged(int)), SLOT(itemStorageTabChanged(int)));
        connect(_itemsDialog, SIGNAL(cubeDeleted(bool)), ui.actionGiveCube, SLOT(setEnabled(bool)));
        connect(_itemsDialog, SIGNAL(closing(bool)), ui.menuGoToPage, SLOT(setDisabled(bool)));
        connect(_itemsDialog, SIGNAL(itemsChanged(bool)), SLOT(setModified(bool)));
        connect(_itemsDialog, SIGNAL(signetsOfLearningEaten(int)), SLOT(eatSignetsOfLearning(int)));
        _itemsDialog->show();

        if (!activate)
        {
            _findItemsDialog->activateWindow();
            _findItemsDialog->raise();
        }
    }
}

void MedianXLOfflineTools::itemStorageTabChanged(int tabIndex)
{
    bool isPlugyStorage = _itemsDialog->isPlugyStorageIndex(tabIndex);
    ui.menuGoToPage->setEnabled(isPlugyStorage);

    static const QList<QAction *> plugyNavigationActions = QList<QAction *>() << ui.actionPrevious10  << ui.actionPreviousPage << ui.actionNextPage << ui.actionNext10
                                                                              << ui.actionPrevious100 << ui.actionFirstPage    << ui.actionLastPage << ui.actionNext100;
    foreach (QAction *action, plugyNavigationActions)
        action->disconnect();

    if (isPlugyStorage)
    {
        static const QList<const char *> plugyNavigationSlots = QList<const char *>() << SLOT(previous10Pages())  << SLOT(previousPage()) << SLOT(nextPage()) << SLOT(next10Pages())
                                                                                      << SLOT(previous100Pages()) << SLOT(firstPage())    << SLOT(lastPage()) << SLOT(next100Pages());
        ItemsPropertiesSplitter *plugyTab = _itemsDialog->splitterAtIndex(tabIndex);
        for (int i = 0; i < plugyNavigationActions.size(); ++i)
            connect(plugyNavigationActions[i], SIGNAL(triggered()), plugyTab, plugyNavigationSlots[i]);
    }
}

void MedianXLOfflineTools::giveCube()
{
    ItemInfo *cube = ItemDataBase::loadItemFromFile("cube");
    if (!cube)
        return;
    
    if (!ItemDataBase::storeItemIn(cube, Enums::ItemStorage::Inventory, ItemsViewerDialog::rowsInStorageAtIndex(Enums::ItemStorage::Inventory)) &&
        !ItemDataBase::storeItemIn(cube, Enums::ItemStorage::Stash,     ItemsViewerDialog::rowsInStorageAtIndex(Enums::ItemStorage::Stash)))
    {
        ERROR_BOX(tr("You have no free space in inventory and stash to store the Cube"));
        delete cube;
        return;
    }

    // predefined position is (0,0) in inventory
//    if (cube->column)
//        ReverseBitWriter::replaceValueInBitString(cube->bitString, Enums::ItemOffsets::Column, cube->column);
//    if (cube->row)
//        ReverseBitWriter::replaceValueInBitString(cube->bitString, Enums::ItemOffsets::Row, cube->row);

    QHash<int, bool> plugyStashesExistenceHash = getPlugyStashesExistenceHash();
    if (cube->storage != Enums::ItemStorage::Inventory && plugyStashesExistenceHash[Enums::ItemStorage::PersonalStash])
    {
//        ReverseBitWriter::replaceValueInBitString(cube->bitString, Enums::ItemOffsets::Storage, cube->storage);

        ItemInfo *plugyCube = new ItemInfo(*cube);
        plugyCube->storage = Enums::ItemStorage::PersonalStash;
        plugyCube->plugyPage = 1;
        CharacterInfo::instance().items.character += plugyCube;
    }
    CharacterInfo::instance().items.character += cube;

    if (_itemsDialog)
        _itemsDialog->updateItems(plugyStashesExistenceHash, false);

    ui.actionGiveCube->setDisabled(true);
    setModified(true);
    INFO_BOX(ItemParser::itemStorageAndCoordinatesString(tr("Cube has been stored in %1 at (%2,%3)"), cube));
}

void MedianXLOfflineTools::getSkillPlan()
{
    SkillplanDialog dlg(this);
    dlg.exec();
}

void MedianXLOfflineTools::backupSettingTriggered(bool checked)
{
    if (!checked && QUESTION_BOX_YESNO(tr("Are you sure you want to disable automatic backups? Then don't blame me if your character gets corrupted."), QMessageBox::No) == QMessageBox::No)
        ui.actionBackup->setChecked(true);
    else
    {
        ui.menuBackupsLimit->setEnabled(checked);
        ui.menuBackupNameFormat->setEnabled(checked);
    }
}

void MedianXLOfflineTools::associateFiles()
{
#if defined(Q_WS_WIN32) || defined(Q_WS_MACX)
    FileAssociationManager::makeApplicationDefaultForExtension(kCharacterExtension);
#else
#warning Add implementation to check file association to e.g. fileassociationmanager_linux.cpp or comment this line
#endif
    updateAssociateAction(true);
}

void MedianXLOfflineTools::checkForUpdate()
{
    _isManuallyCheckingForUpdate = sender() != 0;
    checkForUpdateFromUrl(QUrl("http://modsbylaz.14.forumer.com/viewforum.php?f=21"));
}

void MedianXLOfflineTools::aboutApp()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About %1").arg(qApp->applicationName()));
    aboutBox.setIconPixmap(windowIcon().pixmap(64));
    aboutBox.setTextFormat(Qt::RichText);
    QString appFullName = qApp->applicationName() + " " + qApp->applicationVersion();
    aboutBox.setText(QString("<b>%1</b>%2").arg(appFullName, kHtmlLineBreak) + tr("Released: %1").arg(RELEASE_DATE));
    QString email("decapitator@ukr.net");
    aboutBox.setInformativeText(
        tr("<i>Author:</i> Filipenkov Andrey (<b>kambala</b>)") + QString("%1<i>ICQ:</i> 287764961%1<i>E-mail:</i> <a href=\"mailto:%2?subject=%3\">%2</a>%1%1").arg(kHtmlLineBreak, email, appFullName) +
        kForumThreadHtmlLinks + kHtmlLineBreak + kHtmlLineBreak +
        tr("<b>Credits:</b>"
           "<ul>"
             "<li><a href=\"http://modsbylaz.hugelaser.com/\">BrotherLaz</a> for this awesome mod</li>"
             "<li><a href=\"http://modsbylaz.14.forumer.com/profile.php?mode=viewprofile&u=33805\">grig</a> for the Perl source of "
                 "<a href=\"http://grig.vlexofree.com/\">Median XL Online Tools</a> and tips</li>"
             "<li><a href=\"http://phrozenkeep.hugelaser.com/index.php?ind=reviews&op=section_view&idev=4\">Phrozen Keep File Guides</a> for tons of useful information on txt sources</li>"
             "<li><a href=\"http://modsbylaz.14.forumer.com/profile.php?mode=viewprofile&u=44046\">FixeR</a>, <a href=\"http://forum.worldofplayers.ru/member.php?u=84592\">Zelgadiss</a> and "
                 "<a href=\"http://modsbylaz.14.forumer.com/profile.php?mode=viewprofile&u=44840\">moonra</a> for intensive testing and tips on GUI & functionality</li>"
           "</ul>")
    );
    aboutBox.exec();
}


// overridden protected methods

void MedianXLOfflineTools::closeEvent(QCloseEvent *e)
{
    if (maybeSave())
    {
        saveSettings();
        e->accept();
    }
    else
        e->ignore();
}

void MedianXLOfflineTools::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QString path = mimeData->urls().at(0).toLocalFile();
        if (path.right(4).toLower() == kCharacterExtensionWithDot)
            event->acceptProposedAction();
    }
}

void MedianXLOfflineTools::dropEvent(QDropEvent *event)
{
    loadFile(QDir::toNativeSeparators(event->mimeData()->urls().at(0).toLocalFile()));
    event->acceptProposedAction();
}


// private methods

void MedianXLOfflineTools::loadData()
{
    loadExpTable();
    loadMercNames();
    loadBaseStats();
}

void MedianXLOfflineTools::loadExpTable()
{
    QFile f;
    if (!ItemDataBase::createUncompressedTempFile(ResourcePathManager::dataPathForFileName("exptable.dat"), tr("Experience table data not loaded."), &f))
        return;

    QList<QByteArray> expLines = f.readAll().split('\n');
    experienceTable.reserve(Enums::CharacterStats::MaxLevel);
    foreach (const QByteArray &numberString, expLines)
        if (!numberString.isEmpty())
            experienceTable.append(numberString.trimmed().toUInt());
    f.remove();
}

void MedianXLOfflineTools::loadMercNames()
{
    QFile f;
    if (!ItemDataBase::createUncompressedTempFile(ResourcePathManager::localizedPathForFileName("mercs"), tr("Mercenary names not loaded."), &f))
        return;

    QList<QByteArray> mercLines = f.readAll().split('\n');
    QStringList actNames;
    mercNames.reserve(4);
    foreach (const QByteArray &mercName, mercLines)
    {
        if (mercName.startsWith("-"))
        {
            mercNames += actNames;
            actNames.clear();
        }
        else
            actNames += QString::fromUtf8(mercName.trimmed());
    }
    f.remove();
}

void MedianXLOfflineTools::loadBaseStats()
{
    QFile f;
    if (!ItemDataBase::createUncompressedTempFile(ResourcePathManager::dataPathForFileName("basestats.dat"), tr("Base stats data not loaded, using predefined one."), &f))
    {
        _baseStatsMap[Enums::ClassName::Amazon]      = BaseStats(BaseStats::StatsAtStart(25, 25, 20, 15, 84), BaseStats::StatsPerLevel(100, 40, 60), BaseStats::StatsPerPoint( 8, 8, 18));
        _baseStatsMap[Enums::ClassName::Sorceress]   = BaseStats(BaseStats::StatsAtStart(10, 25, 15, 35, 74), BaseStats::StatsPerLevel(100, 40, 60), BaseStats::StatsPerPoint( 8, 8, 18));
        _baseStatsMap[Enums::ClassName::Necromancer] = BaseStats(BaseStats::StatsAtStart(15, 25, 20, 25, 79), BaseStats::StatsPerLevel( 80, 20, 80), BaseStats::StatsPerPoint( 4, 8, 24));
        _baseStatsMap[Enums::ClassName::Paladin]     = BaseStats(BaseStats::StatsAtStart(25, 20, 25, 15, 89), BaseStats::StatsPerLevel(120, 60, 40), BaseStats::StatsPerPoint(12, 8, 12));
        _baseStatsMap[Enums::ClassName::Barbarian]   = BaseStats(BaseStats::StatsAtStart(30, 20, 30,  5, 92), BaseStats::StatsPerLevel(120, 60, 40), BaseStats::StatsPerPoint(12, 8, 12));
        _baseStatsMap[Enums::ClassName::Druid]       = BaseStats(BaseStats::StatsAtStart(25, 20, 15, 25, 84), BaseStats::StatsPerLevel( 80, 20, 80), BaseStats::StatsPerPoint( 4, 8, 24));
        _baseStatsMap[Enums::ClassName::Assassin]    = BaseStats(BaseStats::StatsAtStart(20, 35, 15, 15, 95), BaseStats::StatsPerLevel(100, 40, 60), BaseStats::StatsPerPoint( 8, 8, 18));
        return;
    }

    QList<QByteArray> lines = f.readAll().split('\n');
    foreach (const QByteArray &s, lines)
    {
        if (!s.isEmpty() && s.at(0) != '#')
        {
            QList<QByteArray> numbers = s.trimmed().split('\t');
            _baseStatsMap[static_cast<Enums::ClassName::ClassNameEnum>(numbers.at(0).toUInt())] = BaseStats
                (
                BaseStats::StatsAtStart (numbers.at(1).toInt(), numbers.at(2).toInt(),  numbers.at(4).toInt(), numbers.at(3).toInt(), numbers.at(5).toInt()), // order is correct - energy value comes before vitality in the file
                BaseStats::StatsPerLevel(numbers.at(6).toInt(), numbers.at(7).toInt(),  numbers.at(8).toInt()),
                BaseStats::StatsPerPoint(numbers.at(9).toInt(), numbers.at(10).toInt(), numbers.at(11).toInt())
                );
        }
    }
    f.remove();
}

void MedianXLOfflineTools::createLanguageMenu()
{
    QString appTranslationName = qApp->applicationName().remove(' ').toLower();
    QStringList fileNames = QDir(LanguageManager::instance().translationsPath, QString("%1_*.qm").arg(appTranslationName)).entryList(QDir::Files);
    if (!fileNames.isEmpty())
    {
        QMenu *languageMenu = new QMenu(tr("&Language", "Language menu"), this);
        ui.menuOptions->addSeparator();
        ui.menuOptions->addMenu(languageMenu);

        QActionGroup *languageActionGroup = new QActionGroup(this);
        connect(languageActionGroup, SIGNAL(triggered(QAction *)), SLOT(switchLanguage(QAction *)));

        // HACK: insert English language
        fileNames.prepend(QString("%1_%2.qm").arg(appTranslationName, LanguageManager::instance().defaultLocale));
        foreach (const QString &fileName, fileNames)
        {
            QTranslator translator;
            translator.load(fileName, LanguageManager::instance().translationsPath);
            QString language = translator.translate("Language", "English", "Your language name");
            if (language.isEmpty())
                language = "English";

            QAction *action = new QAction(language, this);
            action->setCheckable(true);
            QString locale = fileName.mid(fileName.indexOf('_') + 1, 2);
            action->setStatusTip(locale);
            languageMenu->addAction(action);
            languageActionGroup->addAction(action);

            if (locale == LanguageManager::instance().currentLocale)
                action->setChecked(true);
        }
    }
}

void MedianXLOfflineTools::createLayout()
{
    QList<QWidget *> widgetsToFixSize = QList<QWidget *>() << ui.charNamePreview << ui.classLineEdit << ui.titleLineEdit << ui.freeSkillPointsLineEdit
                                                           << ui.freeStatPointsLineEdit << ui.signetsOfLearningEatenLineEdit << ui.signetsOfSkillEatenLineEdit << ui.strengthSpinBox << ui.dexteritySpinBox
                                                           << ui.vitalitySpinBox << ui.energySpinBox << ui.inventoryGoldLineEdit << ui.stashGoldLineEdit << ui.mercLevelLineEdit;
    foreach (QWidget *w, widgetsToFixSize)
        w->setFixedSize(w->size());

    QList<QLineEdit *> readonlyLineEdits = QList<QLineEdit *>() << ui.freeSkillPointsLineEdit << ui.freeStatPointsLineEdit << ui.signetsOfLearningEatenLineEdit << ui.signetsOfSkillEatenLineEdit
                                                                << ui.inventoryGoldLineEdit << ui.stashGoldLineEdit << ui.mercLevelLineEdit << ui.classLineEdit;
    foreach (QLineEdit *lineEdit, readonlyLineEdits)
        lineEdit->setStyleSheet(kReadonlyCss);

    createCharacterGroupBoxLayout();
    createMercGroupBoxLayout();
    createStatsGroupBoxLayout();
    createQuestsGroupBoxLayout();

    QGridLayout *grid = new QGridLayout(centralWidget());
    grid->addWidget(ui.characterGroupBox, 0, 0);
    grid->addWidget(ui.mercGroupBox, 1, 0);
    grid->addWidget(ui.statsGroupBox, 0, 1);
    grid->addWidget(_questsGroupBox, 1, 1);

    //QVBoxLayout *vbl = new QVBoxLayout;
    //vbl->addWidget(ui.characterGroupBox);
    ////vbl->addStretch();
    ////vbl->addWidget(_questsGroupBox);
    //vbl->addWidget(ui.mercGroupBox);

    //QVBoxLayout *vbl1 = new QVBoxLayout;
    //vbl1->addWidget(ui.statsGroupBox);
    //vbl1->addWidget(_questsGroupBox);

    //QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget());
    //mainLayout->addLayout(vbl);
    //mainLayout->addLayout(vbl1);
    ////mainLayout->addWidget(ui.statsGroupBox);

    ui.statsTableWidget->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui.statsTableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    ui.statsTableWidget->setFixedHeight(ui.statsTableWidget->height());

    _charPathLabel = new QLabel(this);
    ui.statusBar->addPermanentWidget(_charPathLabel);

    // on Mac OS X some UI elements become ugly if main window is set to minimumSize()
#ifndef Q_WS_MACX
    resize(minimumSizeHint());
#endif
}

void MedianXLOfflineTools::createCharacterGroupBoxLayout()
{
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->addWidget(new QLabel(tr("Name")), 0, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.charNamePreview, 0, 1);
    gridLayout->addWidget(ui.renameButton, 0, 2);

    gridLayout->addWidget(new QLabel(tr("Class")), 1, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.classLineEdit, 1, 1);
    gridLayout->addWidget(new QLabel(tr("Level")), 1, 2, Qt::AlignCenter);

    gridLayout->addWidget(new QLabel(tr("Title", "Character title - Slayer/Champion/etc.")), 2, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.titleLineEdit, 2, 1);
    gridLayout->addWidget(ui.levelSpinBox, 2, 2);

    //QHBoxLayout *hbl1 = new QHBoxLayout;
    //hbl1->addWidget(new QLabel(tr("Difficulty")), 0, Qt::AlignRight);
    //hbl1->addWidget(ui.currentDifficultyComboBox);
    //hbl1->addWidget(new QLabel(tr("Act")), 0, Qt::AlignRight);
    //hbl1->addWidget(ui.currentActSpinBox);

    QHBoxLayout *hbl2 = new QHBoxLayout(ui.hardcoreGroupBox);
    hbl2->addWidget(ui.convertToSoftcoreCheckBox);
    hbl2->addStretch();
    hbl2->addWidget(ui.resurrectButton);

    _expGroupBox = new ExperienceIndicatorGroupBox(this);

    QVBoxLayout *vbl = new QVBoxLayout(ui.characterGroupBox);
    vbl->addLayout(gridLayout);
    //vbl->addLayout(hbl1);
    vbl->addWidget(_expGroupBox);
    vbl->addWidget(ui.hardcoreGroupBox);
}

void MedianXLOfflineTools::createMercGroupBoxLayout()
{
    QHBoxLayout *hbl = new QHBoxLayout;
    hbl->addWidget(new QLabel(tr("Type")));
    hbl->addWidget(ui.mercTypeComboBox);
    hbl->addWidget(new QLabel(tr("Level")));
    hbl->addWidget(ui.mercLevelLineEdit);
    hbl->addWidget(new QLabel(tr("Name")));
    hbl->addWidget(ui.mercNameComboBox);

    _mercExpGroupBox = new ExperienceIndicatorGroupBox(this);

    QVBoxLayout *vbl = new QVBoxLayout(ui.mercGroupBox);
    vbl->addLayout(hbl);
    vbl->addStretch();
    vbl->addWidget(_mercExpGroupBox);
}

void MedianXLOfflineTools::createStatsGroupBoxLayout()
{
    QGridLayout *gridLayout = new QGridLayout(ui.statsGroupBox);
    gridLayout->addWidget(new QLabel(tr("Inventory Gold")), 0, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.inventoryGoldLineEdit, 0, 1);
    gridLayout->addWidget(ui.activateWaypointsCheckBox, 0, 2);
    gridLayout->addWidget(new QLabel(tr("Stash Gold")), 0, 3, Qt::AlignRight);
    gridLayout->addWidget(ui.stashGoldLineEdit, 0, 4);

    gridLayout->addWidget(new QLabel(tr("Free Skills")), 1, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.freeSkillPointsLineEdit, 1, 1);
    gridLayout->addWidget(ui.respecSkillsCheckBox, 1, 2);
    gridLayout->addWidget(new QLabel(tr("Signets of Skill")), 1, 3, Qt::AlignRight);
    gridLayout->addWidget(ui.signetsOfSkillEatenLineEdit, 1, 4);

    gridLayout->addWidget(ui.statsTableWidget, 2, 2, 4, 3, Qt::AlignCenter);
    gridLayout->addWidget(new QLabel(tr("Strength")), 2, 0, Qt::AlignRight);
    gridLayout->addWidget(new QLabel(tr("Dexterity")), 3, 0, Qt::AlignRight);
    gridLayout->addWidget(new QLabel(tr("Vitality")), 4, 0, Qt::AlignRight);
    gridLayout->addWidget(new QLabel(tr("Energy")), 5, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.strengthSpinBox, 2, 1);
    gridLayout->addWidget(ui.dexteritySpinBox, 3, 1);
    gridLayout->addWidget(ui.vitalitySpinBox, 4, 1);
    gridLayout->addWidget(ui.energySpinBox, 5, 1);

    gridLayout->addWidget(new QLabel(tr("Free Stats")), 6, 0, Qt::AlignRight);
    gridLayout->addWidget(ui.freeStatPointsLineEdit, 6, 1);
    gridLayout->addWidget(ui.respecStatsButton, 6, 2);
    gridLayout->addWidget(new QLabel(tr("Signets of Learning")), 6, 3, Qt::AlignRight);
    gridLayout->addWidget(ui.signetsOfLearningEatenLineEdit, 6, 4);
}

void MedianXLOfflineTools::createQuestsGroupBoxLayout()
{
    _questsGroupBox = new QGroupBox(tr("Quests"), this);

    QList<int> questKeys = QList<int>() << Enums::Quests::DenOfEvil << Enums::Quests::Radament << Enums::Quests::Izual << Enums::Quests::LamEsensTome;
    foreach (int quest, questKeys)
    {
        QCheckBox *hatredCheckBox = new QCheckBox(tr("Hatred"), _questsGroupBox), *terrorCheckBox = new QCheckBox(tr("Terror"), _questsGroupBox), *destCheckBox = new QCheckBox(tr("Destruction"), _questsGroupBox);
        hatredCheckBox->setDisabled(true); terrorCheckBox->setDisabled(true); destCheckBox->setDisabled(true);
        _checkboxesQuestsHash[quest] = QList<QCheckBox *>() << hatredCheckBox << terrorCheckBox << destCheckBox;
    }

    QGridLayout *gridLayout = new QGridLayout(_questsGroupBox);
    gridLayout->addWidget(new QLabel(tr("Den of Evil")), 0, 0);
    gridLayout->addWidget(new QLabel(tr("Radament")), 1, 0);
    gridLayout->addWidget(new QLabel(tr("Izual")), 2, 0);
    gridLayout->addWidget(new QLabel(tr("Lam Esen's Tome")), 4, 0);

    for (int i = 0; i < questKeys.size(); ++i)
        for (int j = 0; j < kDifficultiesNumber; ++j)
            gridLayout->addWidget(_checkboxesQuestsHash[questKeys.at(i)].at(j), i != 3 ? i : 4, j + 1);

    QFrame *line = new QFrame(_questsGroupBox);
    line->setFrameShape(QFrame::HLine);
    gridLayout->addWidget(line, 3, 0, 1, 4);
}

void MedianXLOfflineTools::loadSettings()
{
    QSettings settings;
    if (settings.contains("origin"))
        move(settings.value("origin").toPoint());

    settings.beginGroup("recentItems");
    _recentFilesList = settings.value("recentFiles").toStringList();
    for (int i = 0; i < _recentFilesList.size(); ++i)
        _recentFilesList[i] = QDir::toNativeSeparators(_recentFilesList.at(i));
    updateRecentFilesActions();
    settings.endGroup();

    settings.beginGroup("options");
    ui.actionLoadLastUsedCharacter->setChecked(settings.value("loadLastCharacter", true).toBool());
    ui.actionWarnWhenColoredName->setChecked(settings.value("warnWhenColoredName", true).toBool());
    ui.actionOpenItemsAutomatically->setChecked(settings.value("openItemsAutomatically").toBool());
    ui.actionReloadSharedStashes->setChecked(settings.value("reloadSharedStashes").toBool());

    bool backupsEnabled = settings.value("makeBackups", true).toBool();
    ui.actionBackup->setChecked(backupsEnabled);
    ui.menuBackupsLimit->setEnabled(backupsEnabled);
    ui.menuBackupNameFormat->setEnabled(backupsEnabled);

    (settings.value("backupFormatIsTimestamp").toBool() ? ui.actionBackupFormatTimestamp : ui.actionBackupFormatReadable)->setChecked(true);
    QVariant backupLimit = settings.value("backupLimit");
    if (backupLimit.isValid())
    {
        switch (backupLimit.toInt())
        {
        case 1:
            ui.actionBackups1->setChecked(true);
            break;
        case 2:
            ui.actionBackups2->setChecked(true);
            break;
        case 5:
            ui.actionBackups5->setChecked(true);
            break;
        case 10:
            ui.actionBackups10->setChecked(true);
            break;
        default:
            ui.actionBackupsUnlimited->setChecked(true);
            break;
        }
    }
    else
        ui.actionBackups5->setChecked(true);

    settings.beginGroup("autoOpenSharedStashes");
    ui.actionAutoOpenPersonalStash->setChecked(settings.value("personal", true).toBool());
    ui.actionAutoOpenSharedStash->setChecked(settings.value("shared", true).toBool());
    ui.actionAutoOpenHCShared->setChecked(settings.value("hcShared", true).toBool());
    settings.endGroup();

    ui.actionCheckFileAssociations->setChecked(settings.value("checkAssociations", true).toBool());
    ui.actionCheckForUpdateOnStart->setChecked(settings.value("checkUpdates", true).toBool());

    settings.endGroup();
}

void MedianXLOfflineTools::saveSettings() const
{
    QSettings settings;
    settings.setValue(LanguageManager::instance().languageKey, LanguageManager::instance().currentLocale);
    settings.setValue("origin", pos());

    settings.beginGroup("recentItems");
    settings.setValue("recentFiles", _recentFilesList);
    settings.endGroup();
    
    settings.beginGroup("options");
    settings.setValue("loadLastCharacter", ui.actionLoadLastUsedCharacter->isChecked());
    settings.setValue("warnWhenColoredName", ui.actionWarnWhenColoredName->isChecked());
    settings.setValue("openItemsAutomatically", ui.actionOpenItemsAutomatically->isChecked());
    settings.setValue("reloadSharedStashes", ui.actionReloadSharedStashes->isChecked());

    settings.setValue("makeBackups", ui.actionBackup->isChecked());
    settings.setValue("backupFormatIsTimestamp", ui.actionBackupFormatTimestamp->isChecked());
    settings.setValue("backupLimit", _backupLimitsGroup->checkedAction()->data().toInt());

    settings.beginGroup("autoOpenSharedStashes");
    settings.setValue("personal", ui.actionAutoOpenPersonalStash->isChecked());
    settings.setValue("shared", ui.actionAutoOpenSharedStash->isChecked());
    settings.setValue("hcShared", ui.actionAutoOpenHCShared->isChecked());
    settings.endGroup();

    settings.setValue("checkAssociations", ui.actionCheckFileAssociations->isChecked());
    settings.setValue("checkUpdates", ui.actionCheckForUpdateOnStart->isChecked());

    settings.endGroup();

    if (_findItemsDialog)
        _findItemsDialog->saveSettings();
    if (_itemsDialog)
        _itemsDialog->saveSettings();
}

bool compareSkillIndexes(int i, int j)
{
    SkillInfo *iSkill = ItemDataBase::Skills()->value(i), *jSkill = ItemDataBase::Skills()->value(j);
    if (iSkill->tab == jSkill->tab)
    {
        if (iSkill->col == jSkill->col)
            return iSkill->row < jSkill->row;
        else
            return iSkill->col < jSkill->col;
    }
    else
        return iSkill->tab < jSkill->tab;
}

void MedianXLOfflineTools::fillMaps()
{
    _spinBoxesStatsMap[Enums::CharacterStats::Strength] = ui.strengthSpinBox;
    _spinBoxesStatsMap[Enums::CharacterStats::Dexterity] = ui.dexteritySpinBox;
    _spinBoxesStatsMap[Enums::CharacterStats::Vitality] = ui.vitalitySpinBox;
    _spinBoxesStatsMap[Enums::CharacterStats::Energy] = ui.energySpinBox;

    _lineEditsStatsMap[Enums::CharacterStats::FreeStatPoints] = ui.freeStatPointsLineEdit;
    _lineEditsStatsMap[Enums::CharacterStats::FreeSkillPoints] = ui.freeSkillPointsLineEdit;
    _lineEditsStatsMap[Enums::CharacterStats::InventoryGold] = ui.inventoryGoldLineEdit;
    _lineEditsStatsMap[Enums::CharacterStats::StashGold] = ui.stashGoldLineEdit;
    _lineEditsStatsMap[Enums::CharacterStats::SignetsOfLearningEaten] = ui.signetsOfLearningEatenLineEdit;
    _lineEditsStatsMap[Enums::CharacterStats::SignetsOfSkillEaten] = ui.signetsOfSkillEatenLineEdit;

    QList<SkillInfo *> *skills = ItemDataBase::Skills();
    int n = skills->size();
    for (int classCode = Enums::ClassName::Amazon; classCode <= Enums::ClassName::Assassin; ++classCode)
    {
        QList<int> skillsIndexes;
        for (int i = 0; i < n; ++i)
            if (skills->at(i)->classCode == classCode)
                skillsIndexes += i;
        _characterSkillsIndexes[static_cast<Enums::ClassName::ClassNameEnum>(classCode)].first = skillsIndexes;

        qSort(skillsIndexes.begin(), skillsIndexes.end(), compareSkillIndexes);
        _characterSkillsIndexes[static_cast<Enums::ClassName::ClassNameEnum>(classCode)].second = skillsIndexes;
    }
}

void MedianXLOfflineTools::connectSignals()
{
    // files
    connect(ui.actionLoadCharacter, SIGNAL(triggered()), SLOT(loadCharacter()));
    connect(ui.actionReloadCharacter, SIGNAL(triggered()), SLOT(reloadCharacter()));
    connect(ui.actionSaveCharacter, SIGNAL(triggered()), SLOT(saveCharacter()));

    // edit
    connect(ui.actionRename, SIGNAL(triggered()), SLOT(rename()));
    connect(ui.actionFind, SIGNAL(triggered()), SLOT(findItem()));

    // items
    connect(ui.actionShowItems, SIGNAL(triggered()), SLOT(showItems()));
    connect(ui.actionGiveCube, SIGNAL(triggered()), SLOT(giveCube()));

    // export
    connect(ui.actionSkillPlan, SIGNAL(triggered()), SLOT(getSkillPlan()));

    // options
    connect(ui.actionBackup, SIGNAL(triggered(bool)), SLOT(backupSettingTriggered(bool)));
    connect(ui.actionAssociate, SIGNAL(triggered()), SLOT(associateFiles()));

    // help
    connect(ui.actionCheckForUpdate, SIGNAL(triggered()), SLOT(checkForUpdate()));
    connect(ui.actionAbout, SIGNAL(triggered()), SLOT(aboutApp()));
    connect(ui.actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    connect(ui.levelSpinBox, SIGNAL(valueChanged(int)), SLOT(levelChanged(int)));
    foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
        connect(spinBox, SIGNAL(valueChanged(int)), SLOT(statChanged(int)));

    connect(ui.respecStatsButton, SIGNAL(clicked()), SLOT(respecStats()));
    connect(ui.renameButton, SIGNAL(clicked()), SLOT(rename()));
    connect(ui.resurrectButton, SIGNAL(clicked()), SLOT(resurrect()));
    connect(ui.convertToSoftcoreCheckBox, SIGNAL(toggled(bool)), SLOT(convertToSoftcore(bool)));
    connect(ui.respecSkillsCheckBox, SIGNAL(toggled(bool)), SLOT(respecSkills(bool)));
    //connect(ui.currentDifficultyComboBox, SIGNAL(currentIndexChanged(int)), SLOT(currentDifficultyChanged(int)));
    connect(ui.activateWaypointsCheckBox, SIGNAL(clicked()), SLOT(modify()));
}

void MedianXLOfflineTools::updateRecentFilesActions()
{
    ui.menuRecentCharacters->clear();
    for (int i = 0; i < _recentFilesList.length(); ++i)
    {
        QString filePath = _recentFilesList.at(i);
        if (QFile::exists(filePath))
            ui.menuRecentCharacters->addAction(createRecentFileAction(filePath, i + 1));
        else
        {
#ifdef Q_WS_WIN32
            removeFromWindowsRecentFiles(_recentFilesList.takeAt(i));
#else
            _recentFilesList.removeAt(i);
#endif
            --i;
        }
    }
    ui.menuRecentCharacters->setDisabled(_recentFilesList.isEmpty());

#ifdef Q_WS_MACX
    extern void qt_mac_set_dock_menu(QMenu *);
    qt_mac_set_dock_menu(ui.menuRecentCharacters);
#endif
}

void MedianXLOfflineTools::addToRecentFiles()
{
    int index = _recentFilesList.indexOf(_charPath);
#ifdef Q_WS_WIN32
    // previous version didn't use native separators
    if (index == -1)
    {
        index = _recentFilesList.indexOf(QDir::fromNativeSeparators(_charPath));
        if (index != -1)
            _recentFilesList[index] = _charPath;
    }
#endif

    if (index != -1) // it's already in the list
        _recentFilesList.move(index, 0);
    else
    {
        if (_recentFilesList.length() == kMaxRecentFiles)
        {
#ifdef Q_WS_WIN32
            removeFromWindowsRecentFiles(_recentFilesList.takeLast());
#else
            _recentFilesList.removeLast();
#endif
        }
        _recentFilesList.prepend(_charPath);
    }
#ifdef Q_WS_WIN32
    addToWindowsRecentFiles(_charPath); // Windows moves file to the top itself if it already exists in the list
#endif
    updateRecentFilesActions();
}

QAction *MedianXLOfflineTools::createRecentFileAction(const QString &fileName, int index)
{
    // don't use numbers on Mac OS X because there're no mnemonics
#ifdef Q_WS_MACX
    Q_UNUSED(index);
    QAction *recentFileAction = new QAction(QFileInfo(fileName).fileName(), this);
#else
    QAction *recentFileAction = new QAction(QString("&%1 %2").arg(index).arg(QFileInfo(fileName).fileName()), this);
#endif
    recentFileAction->setStatusTip(QDir::toNativeSeparators(fileName));
    connect(recentFileAction, SIGNAL(triggered()), SLOT(openRecentFile()));
    return recentFileAction;
}

bool MedianXLOfflineTools::processSaveFile()
{
    QFile inputFile(_charPath);
    if (!inputFile.open(QIODevice::ReadOnly))
    {
        showErrorMessageBoxForFile(tr("Error opening file '%1'"), inputFile);
        return false;
    }

    _saveFileContents = inputFile.readAll();
    inputFile.close();

    QDataStream inputDataStream(_saveFileContents);
    inputDataStream.setByteOrder(QDataStream::LittleEndian);

    quint32 signature;
    inputDataStream >> signature;
    if (signature != kFileSignature)
    {
        ERROR_BOX(tr("Wrong file signature: should be 0x%1, got 0x%2.").arg(kFileSignature, 0, 16).arg(signature, 0, 16));
        return false;
    }

    inputDataStream.device()->seek(Enums::Offsets::Checksum);
    quint32 fileChecksum, computedChecksum = checksum(_saveFileContents);
    inputDataStream >> fileChecksum;
#ifndef DISABLE_CRC_CHECK
    if (fileChecksum != computedChecksum)
    {
        ERROR_BOX(tr("Character checksum doesn't match. Looks like it's corrupted."));
        return false;
    }
#endif

    CharacterInfo &charInfo = CharacterInfo::instance();
    charInfo.basicInfo.originalName = _saveFileContents.constData() + Enums::Offsets::Name;
    charInfo.basicInfo.newName = charInfo.basicInfo.originalName.replace(ColorsManager::ansiColorHeader(), ColorsManager::unicodeColorHeader());

    inputDataStream.device()->seek(Enums::Offsets::Status);
    quint8 status, progression, classCode, clvl;
    inputDataStream >> status >> progression;
    inputDataStream.device()->seek(Enums::Offsets::Class);
    inputDataStream >> classCode;
    inputDataStream.device()->seek(Enums::Offsets::Level);
    inputDataStream >> clvl;

    if (!(status & Enums::StatusBits::IsExpansion))
    {
        ERROR_BOX(tr("This is not Expansion character."));
        return false;
    }
    charInfo.basicInfo.isHardcore = status & Enums::StatusBits::IsHardcore;
    charInfo.basicInfo.hadDied = status & Enums::StatusBits::HadDied;

    if (classCode > Enums::ClassName::Assassin)
    {
        ERROR_BOX(tr("Wrong class value: got %1").arg(classCode));
        return false;
    }
    charInfo.basicInfo.classCode = static_cast<Enums::ClassName::ClassNameEnum>(classCode);

    if (progression >= Enums::Progression::Completed)
    {
        ERROR_BOX(tr("Wrong progression value: got %1").arg(progression));
        return false;
    }
    charInfo.basicInfo.titleCode = progression;

    if (!clvl || clvl > Enums::CharacterStats::MaxLevel)
    {
        ERROR_BOX(tr("Wrong level: got %1").arg(clvl));
        return false;
    }
    charInfo.basicInfo.level = clvl;

    //inputDataStream.device()->seek(Enums::Offsets::CurrentLocation);
    //for (int i = 0; i < 3; ++i)
    //{
    //    quint8 difficulty;
    //    inputDataStream >> difficulty;
    //    if (difficulty & Enums::DifficultyBits::IsActive)
    //    {
    //        charInfo.basicInfo.currentDifficulty = i;
    //        charInfo.basicInfo.currentAct = (difficulty & Enums::DifficultyBits::CurrentAct) + 1;
    //        break;
    //    }
    //}

    inputDataStream.device()->seek(Enums::Offsets::Mercenary);
    quint32 mercID;
    inputDataStream >> mercID;
    if ((charInfo.mercenary.exists = (mercID != 0)))
    {
        quint16 mercName, mercValue;
        inputDataStream >> mercName >> mercValue;
        if (mercValue > Enums::Mercenary::MaxCode)
        {
            ERROR_BOX(tr("Wrong mercenary code: got %1").arg(mercValue));
            return false;
        }
        charInfo.mercenary.code = Enums::Mercenary::mercCodeFromValue(mercValue);
        charInfo.mercenary.nameIndex = mercName;

        quint32 mercExp;
        inputDataStream >> mercExp;
        charInfo.mercenary.experience = mercExp;
        for (quint8 i = 1; i <= Enums::CharacterStats::MaxLevel; ++i)
            if (mercExp < mercExperienceForLevel(i))
            {
                charInfo.mercenary.level = i - 1;
                break;
            }
    }

    // Quests
    if (_saveFileContents.mid(Enums::Offsets::QuestsHeader, 4) != "Woo!")
    {
        ERROR_BOX(tr("Quests data not found!"));
        return false;
    }
    charInfo.questsInfo.clear();
    for (int i = 0; i < kDifficultiesNumber; ++i)
    {
        int baseOffset = Enums::Offsets::QuestsData + i * Enums::Quests::Size;
        charInfo.questsInfo.denOfEvil += static_cast<bool>(_saveFileContents.at(baseOffset + Enums::Quests::DenOfEvil) & Enums::Quests::IsCompleted);
        charInfo.questsInfo.radament += static_cast<bool>(_saveFileContents.at(baseOffset + Enums::Quests::Radament) & (Enums::Quests::IsCompleted | Enums::Quests::IsTaskDone));
        charInfo.questsInfo.lamEsensTome += static_cast<bool>(_saveFileContents.at(baseOffset + Enums::Quests::LamEsensTome) & Enums::Quests::IsCompleted);
        charInfo.questsInfo.izual += static_cast<bool>(_saveFileContents.at(baseOffset + Enums::Quests::Izual) & Enums::Quests::IsCompleted);
    }

    // WP
    if (_saveFileContents.mid(Enums::Offsets::WaypointsHeader, 2) != "WS")
    {
        ERROR_BOX(tr("Waypoint data not found!"));
        return false;
    }

    //inputDataStream.device->seek(Enums::Offsets::WaypointsData);
    //for (int i = 0; i < 3; ++i)
    //{
    //    inputDataStream.skipRawData(2);

    //    quint64 wpData;
    //    inputDataStream >> wpData;
    //    QString wpString = binaryStringFromNumber(wpData, true, 39);
    //    qDebug() << "wp data difficulty" << i << ":";
    //    int startPos = 0;
    //    for (int j = 0; j < 5; ++j)
    //    {
    //        int endPos = j == 3 ? 3 : 9;
    //        qDebug() << "act" << (j + 1) << ":" << wpString.mid(startPos, endPos);
    //        startPos += endPos;
    //    }

    //    inputDataStream.skipRawData(14);
    //}

    // NPC
    if (_saveFileContents.mid(Enums::Offsets::NPCHeader, 2) != "w4")
    {
        ERROR_BOX(tr("NPC data not found!"));
        return false;
    }

    // stats
    if (_saveFileContents.mid(Enums::Offsets::StatsHeader, 2) != "gf")
    {
        ERROR_BOX(tr("Stats data not found!"));
        return false;
    }
    inputDataStream.device()->seek(Enums::Offsets::StatsData);

    // find "if" header (skills)
    int skillsOffset = _saveFileContents.indexOf("if", Enums::Offsets::StatsData);
    if (skillsOffset == -1)
    {
        ERROR_BOX(tr("Skills data not found!"));
        return false;
    }
    charInfo.skillsOffset = skillsOffset;

    int statsSize = skillsOffset - Enums::Offsets::StatsData;
    QString statsBitData;
    statsBitData.reserve(statsSize * 8);
    for (int i = 0; i < statsSize; ++i)
    {
        quint8 aByte;
        inputDataStream >> aByte;
        statsBitData.prepend(binaryStringFromNumber(aByte));
    }

    // clear dynamic values
    QMetaEnum statisticMetaEnum = Enums::CharacterStats::statisticMetaEnum();
    for (int i = 0; i < statisticMetaEnum.keyCount(); ++i)
        charInfo.basicInfo.statsDynamicData.setProperty(statisticMetaEnum.key(i), QVariant());

    int count = 0; // to prevent infinite loop if something goes wrong
    int totalStats = 0;
    bool wasHackWarningShown = false;
    ReverseBitReader bitReader(statsBitData);
    for (; count < 20; ++count)
    {
        int statCode = bitReader.readNumber(Enums::CharacterStats::StatCodeLength);
        if (statCode == Enums::CharacterStats::End)
            break;

        int statLength = ItemDataBase::Properties()->value(statCode)->saveBits;
        if (!statLength)
        {
            QString modName("Median XL");
            if (isUltimative())
                modName += QString(" Ultimative v%1").arg(isUltimative4() ? "4" : "5+");
            ERROR_BOX(tr("Unknown statistic code found: %1. This is not %2 character.", "second param is mod name").arg(statCode).arg(modName));
            return false;
        }

        qint64 statValue = bitReader.readNumber(statLength);
        if (statCode == Enums::CharacterStats::Level && statValue != clvl)
        {
            ERROR_BOX(tr("Level in statistics (%1) isn't equal the one in header (%2).").arg(statValue).arg(clvl));
            return false;
        }
        else if (statCode >= Enums::CharacterStats::Life && statCode <= Enums::CharacterStats::BaseStamina)
            statValue >>= 8;
        else if (statCode == Enums::CharacterStats::SignetsOfLearningEaten && statValue > Enums::CharacterStats::SignetsOfLearningMax)
        {
            if (statValue > Enums::CharacterStats::SignetsOfLearningMax + 1 && !wasHackWarningShown)
            {
                WARNING_BOX(hackerDetected);
                wasHackWarningShown = true;
            }
            statValue = Enums::CharacterStats::SignetsOfLearningMax;
        }
        else if (statCode == Enums::CharacterStats::SignetsOfSkillEaten && statValue > Enums::CharacterStats::SignetsOfSkillMax)
        {
            if (statValue > Enums::CharacterStats::SignetsOfSkillMax + 1 && !wasHackWarningShown)
            {
                WARNING_BOX(hackerDetected);
                wasHackWarningShown = true;
            }
            statValue = Enums::CharacterStats::SignetsOfSkillMax;
        }
        else if (statCode >= Enums::CharacterStats::Strength && statCode <= Enums::CharacterStats::Vitality)
        {
            int baseStat = _baseStatsMap[charInfo.basicInfo.classCode].statsAtStart.statFromCode(static_cast<Enums::CharacterStats::StatisticEnum>(statCode));
            totalStats += statValue - baseStat;
        }
        else if (statCode == Enums::CharacterStats::FreeStatPoints)
            totalStats += statValue;

        charInfo.basicInfo.statsDynamicData.setProperty(Enums::CharacterStats::statisticNameFromValue(statCode), statValue);
    }
    if (count == 20)
    {
        ERROR_BOX(tr("Stats data is corrupted!"));
        return false;
    }

    int totalPossibleStats = totalPossibleStatPoints(charInfo.basicInfo.level);
    if (totalStats > totalPossibleStats) // check if stats are hacked
    {
        for (int i = Enums::CharacterStats::Strength; i <= Enums::CharacterStats::Vitality; ++i)
        {
            Enums::CharacterStats::StatisticEnum statCode = static_cast<Enums::CharacterStats::StatisticEnum>(i);
            int baseStat = _baseStatsMap[charInfo.basicInfo.classCode].statsAtStart.statFromCode(statCode);
            charInfo.basicInfo.statsDynamicData.setProperty(statisticMetaEnum.key(i), baseStat);
        }
        charInfo.basicInfo.statsDynamicData.setProperty("FreeStatPoints", totalPossibleStats);
        if (!wasHackWarningShown)
        {
            WARNING_BOX(hackerDetected);
            wasHackWarningShown = true;
        }
    }

    // skills
    quint16 skills = 0, maxPossibleSkills = totalPossibleSkillPoints();
    charInfo.basicInfo.skills.clear();
    charInfo.basicInfo.skills.reserve(kSkillsNumber);
    for (int i = 0; i < kSkillsNumber; ++i)
    {
        quint8 skillValue = _saveFileContents.at(skillsOffset + 2 + i);
        skills += skillValue;
        charInfo.basicInfo.skills += skillValue;
        //qDebug() << skillValue << ItemDataBase::Skills()->value(_characterSkillsIndeces[charInfo.basicInfo.classCode].first.at(i)).name;
    }
    skills += charInfo.valueOfStatistic(Enums::CharacterStats::FreeSkillPoints);
    if (skills > maxPossibleSkills) // check if skills are hacked
    {
        skills = maxPossibleSkills;
        charInfo.basicInfo.statsDynamicData.setProperty("FreeSkillPoints", maxPossibleSkills);
        _saveFileContents.replace(charInfo.skillsOffset + 2, kSkillsNumber, QByteArray(kSkillsNumber, 0));
        if (!wasHackWarningShown)
        {
            WARNING_BOX(hackerDetected);
            wasHackWarningShown = true;
        }
    }
    charInfo.basicInfo.totalSkillPoints = skills;

    const QPair<QList<int>, QList<int> > &skillsIndeces = _characterSkillsIndexes[charInfo.basicInfo.classCode];
    charInfo.basicInfo.skillsReadable.clear();
    charInfo.basicInfo.skillsReadable.reserve(kSkillsNumber);
    for (int i = 0; i < kSkillsNumber; ++i)
    {
        int skillIndex = skillsIndeces.second.at(i);
        charInfo.basicInfo.skillsReadable += charInfo.basicInfo.skills.at(skillsIndeces.first.indexOf(skillIndex));
        //qDebug() << charInfo.basicInfo.skillsReadable.last() << ItemDataBase::Skills()->value(skillIndex).name;
    }

    // items
    inputDataStream.skipRawData(kSkillsNumber + 2);
    int charItemsOffset = inputDataStream.device()->pos();
    if (_saveFileContents.mid(charItemsOffset, 2) != ItemParser::kItemHeader)
    {
        ERROR_BOX(tr("Items data not found!"));
        return false;
    }
    charInfo.itemsOffset = charItemsOffset + 2;
    inputDataStream.skipRawData(2); // pointing to the beginning of item data

    quint16 charItemsTotal;
    inputDataStream >> charItemsTotal;
    ItemsList itemsBuffer;
    QString corruptedItems = ItemParser::parseItemsToBuffer(charItemsTotal, inputDataStream, _saveFileContents, tr("Corrupted item detected in %1 at (%2,%3)"), &itemsBuffer);
    if (!corruptedItems.isEmpty())
        ERROR_BOX(corruptedItems.trimmed());
    charInfo.itemsEndOffset = inputDataStream.device()->pos();
    qDebug("items end offset %u", charInfo.itemsEndOffset);

    const int avoidKey = Enums::ItemProperties::Avoid1;
    quint32 avoidValue = 0;
    foreach (ItemInfo *item, itemsBuffer)
    {
        if (item->location == Enums::ItemLocation::Equipped || (item->storage == Enums::ItemStorage::Inventory && ItemDataBase::isUberCharm(item)))
        {
            ItemProperty *itemProp = item->props.value(avoidKey), *rwProp = item->rwProps.value(avoidKey);
            if (itemProp)
                avoidValue += itemProp->value;
            if (rwProp)
                avoidValue += rwProp->value;

            foreach (ItemInfo *socketableItem, item->socketablesInfo)
            {
                itemProp = socketableItem->props.value(avoidKey);
                if (itemProp)
                    avoidValue += itemProp->value;
                rwProp = socketableItem->rwProps.value(avoidKey);
                if (rwProp)
                    avoidValue += rwProp->value;
            }
        }
    }
    if (avoidValue >= 100)
    {
        QString avoidText = tr("100% avoid is kewl");
        if (avoidValue > 100)
            avoidText += QString(" (%1)").arg(tr("well, you have %1% actually", "avoid").arg(avoidValue));
        WARNING_BOX(avoidText);
    }

    // corpse data
    inputDataStream.skipRawData(2); // JM
    inputDataStream >> charInfo.basicInfo.corpses;
    if (charInfo.basicInfo.corpses)
    {
        inputDataStream.skipRawData(12 + 2); // some unknown corpse data + JM
        quint16 corpseItemsTotal;
        inputDataStream >> corpseItemsTotal;
        ItemsList corpseItems;
        ItemParser::parseItemsToBuffer(corpseItemsTotal, inputDataStream, _saveFileContents.left(_saveFileContents.indexOf(kMercHeader)), tr("Corrupted item detected in %1 in slot %4"), &corpseItems);
        foreach (ItemInfo *item, corpseItems)
            item->location = Enums::ItemLocation::Corpse;
        itemsBuffer += corpseItems;

        // after saving there can actually be only one corpse, but let's be safe
        if (charInfo.basicInfo.corpses > 1)
        {
            qWarning("more than one corpse found!");
            inputDataStream.device()->seek(_saveFileContents.indexOf(kMercHeader));
        }
    }

    // merc
    if (_saveFileContents.mid(inputDataStream.device()->pos(), 2) != kMercHeader)
    {
        ERROR_BOX(tr("Mercenary items section not found!"));
        return false;
    }
    inputDataStream.skipRawData(2);
    if (charInfo.mercenary.exists)
    {
        inputDataStream.skipRawData(2); // JM
        quint16 mercItemsTotal;
        inputDataStream >> mercItemsTotal;
        ItemsList mercItems;
        ItemParser::parseItemsToBuffer(mercItemsTotal, inputDataStream, _saveFileContents.left(_saveFileContents.size() - 3), tr("Corrupted item detected in %1 in slot %4"), &mercItems);
        foreach (ItemInfo *item, mercItems)
            item->location = Enums::ItemLocation::Merc;
        itemsBuffer += mercItems;
    }

    // end
    if (_saveFileContents.mid(inputDataStream.device()->pos(), 2) != "kf" || _saveFileContents.at(_saveFileContents.size() - 1) != 0)
    {
        ERROR_BOX(tr("Save file is not terminated correctly!"));
        return false;
    }

    // parse plugy stashes
    QString oldSharedStashPath = _plugyStashesHash[Enums::ItemStorage::SharedStash].path, oldHCStashPath = _plugyStashesHash[Enums::ItemStorage::HCStash].path;

    QFileInfo charPathFileInfo(_charPath);
    QString canonicalCharPath = charPathFileInfo.canonicalPath();
    _plugyStashesHash[Enums::ItemStorage::PersonalStash].path = ui.actionAutoOpenPersonalStash->isChecked() ? QString("%1/%2.d2x").arg(canonicalCharPath, charPathFileInfo.baseName()) : QString();
    _plugyStashesHash[Enums::ItemStorage::SharedStash].path = ui.actionAutoOpenSharedStash->isChecked() ? canonicalCharPath + "/_LOD_SharedStashSave.sss" : QString();
    _plugyStashesHash[Enums::ItemStorage::HCStash].path = ui.actionAutoOpenHCShared->isChecked() ? canonicalCharPath + "/_LOD_HC_SharedStashSave.sss" : QString();

    bool sharedStashPathChanged = oldSharedStashPath != _plugyStashesHash[Enums::ItemStorage::SharedStash].path, hcStashPathChanged = oldHCStashPath != _plugyStashesHash[Enums::ItemStorage::HCStash].path;
    if (ui.actionReloadSharedStashes->isChecked())
        sharedStashPathChanged = hcStashPathChanged = true;
    _sharedGold = 0;
    for (QHash<Enums::ItemStorage::ItemStorageEnum, PlugyStashInfo>::iterator iter = _plugyStashesHash.begin(); iter != _plugyStashesHash.end(); ++iter)
    {
        switch (iter.key())
        {
        case Enums::ItemStorage::PersonalStash:
            if (!(_plugyStashesHash[iter.key()].exists = ui.actionAutoOpenPersonalStash->isChecked()))
                continue;
            break;
        case Enums::ItemStorage::SharedStash:
            if (!(_plugyStashesHash[iter.key()].exists = ui.actionAutoOpenSharedStash->isChecked()) || !sharedStashPathChanged)
                continue;
            break;
        case Enums::ItemStorage::HCStash:
            if (!(_plugyStashesHash[iter.key()].exists = ui.actionAutoOpenHCShared->isChecked()) || !hcStashPathChanged)
                continue;
            break;
        default:
            break;
        }
        processPlugyStash(iter, &itemsBuffer);
    }

    clearItems(sharedStashPathChanged, hcStashPathChanged);
    charInfo.items.character += itemsBuffer;

    // create a lot of copies of all gems
//    int p = 6;
//    for (int i = 0; i < 100; ++i)
//    {
//        ItemInfo *rune = ItemDataBase::loadItemFromFile("runes/r10");
//        foreach (const QByteArray &gemType, QList<QByteArray>() << "gcv" << "gfv" << "gsv" << "gzv" << "gpv" << "gcb" << "gfb" << "gsb" << "glb" << "gpb" << "gcg" << "gfg" << "gsg" << "glg" << "gpg" << "gcr" << "gfr" << "gsr" << "glr" << "gpr" << "gcw" << "gfw" << "gsw" << "glw" << "gpw" << "gcy" << "gfy" << "gsy" << "gly" << "gpy" << "skc" << "skf" << "sku" << "skl" << "skz" << "yo1" << "yo2" << "yo3" << "yo4" << "yo5" << "g$a" << "g$b" << "g$c" << "g$d" << "g$e" << "9$a" << "9$b" << "9$c" << "9$d" << "9$e" << "7$a" << "7$b" << "7$c" << "7$d" << "7$e" << "5$a" << "5$b" << "5$c" << "5$d" << "5$e")
//        {
//            ItemInfo *gem = new ItemInfo(*rune);
//            gem->itemType = gemType;
//            for (int i = 0; i < 3; ++i)
//                ReverseBitWriter::replaceValueInBitString(gem->bitString, Enums::ItemOffsets::Type + i*8, gemType.at(i));
//            if (ItemDataBase::storeItemIn(gem, Enums::ItemStorage::PersonalStash, 10, p))
//                charInfo.items.character += gem;
//        }
//        delete rune;
//        ++p;
//    }

    return true;
}

quint32 MedianXLOfflineTools::checksum(const QByteArray &charByteArray) const
{
    quint32 sum = 0;
    for (int i = 0, n = charByteArray.size(); i < n; ++i)
    {
        bool mostSignificantByte = sum & 0x80000000;
        sum <<= 1;
        sum += mostSignificantByte;
        sum &= 0xFFFFFFFF;
        if (i < 12 || i > 15) // bytes 12-15 - file checksum
            sum += static_cast<quint8>(charByteArray.at(i));
    }
    return sum;
}

inline int MedianXLOfflineTools::totalPossibleStatPoints(int level) const
{
    const CharacterInfo &charInfo = CharacterInfo::instance();
    return (level - 1) * kStatPointsPerLevel + 5 * charInfo.questsInfo.lamEsensTomeQuestsCompleted() + charInfo.valueOfStatistic(Enums::CharacterStats::SignetsOfLearningEaten);
}

inline int MedianXLOfflineTools::totalPossibleSkillPoints() const
{
    const CharacterInfo &charInfo = CharacterInfo::instance();
    return (charInfo.basicInfo.level - 1) * kSkillPointsPerLevel + charInfo.questsInfo.denOfEvilQuestsCompleted() + charInfo.questsInfo.radamentQuestsCompleted() + charInfo.questsInfo.izualQuestsCompleted() * 2 +
        charInfo.valueOfStatistic(Enums::CharacterStats::SignetsOfSkillEaten);
}

int MedianXLOfflineTools::investedStatPoints()
{
    quint16 sum = 0;
    for (quint8 i = Enums::CharacterStats::Strength; i <= Enums::CharacterStats::Vitality; ++i)
    {
        Enums::CharacterStats::StatisticEnum ienum = static_cast<Enums::CharacterStats::StatisticEnum>(i);
        sum += _spinBoxesStatsMap[ienum]->value() - _baseStatsMap[CharacterInfo::instance().basicInfo.classCode].statsAtStart.statFromCode(ienum);
    }
    return sum;
}

void MedianXLOfflineTools::recalculateStatPoints()
{
    CharacterInfo::instance().basicInfo.totalStatPoints = investedStatPoints() + ui.freeStatPointsLineEdit->text().toUInt();
}

void MedianXLOfflineTools::processPlugyStash(QHash<Enums::ItemStorage::ItemStorageEnum, PlugyStashInfo>::iterator &iter, ItemsList *items)
{
    PlugyStashInfo &info = iter.value();
    QFile inputFile(info.path);
    if (!(info.exists = inputFile.exists()))
        return;
    if (!(info.exists = inputFile.open(QIODevice::ReadOnly)))
    {
        showErrorMessageBoxForFile(tr("Error opening PlugY stash '%1'"), inputFile);
        return;
    }

    QByteArray bytes = inputFile.readAll();
    inputFile.close();

    QByteArray header;
    const int headerSize = 5;
    if (iter.key() == Enums::ItemStorage::PersonalStash)
        header = QByteArray("CSTM0");
    else
    {
        char sharedStashHeader[] = {'S', 'S', 'S', '\0', '0'};
        header = QByteArray(sharedStashHeader, headerSize);
    }
    if (bytes.left(headerSize) != header)
    {
        ERROR_BOX(tr("PlugY stash '%1' has wrong header").arg(QFileInfo(info.path).fileName()));
        return;
    }
    info.header = header;

    QDataStream inputDataStream(bytes);
    inputDataStream.setByteOrder(QDataStream::LittleEndian);
    inputDataStream.skipRawData(headerSize);

    inputDataStream >> info.version;
    if ((info.hasGold = (bytes.mid(headerSize + 1 + 4, 2) != ItemParser::kPlugyPageHeader)))
        inputDataStream >> info.gold;
    if (iter.key() == Enums::ItemStorage::SharedStash)
        _sharedGold = info.gold;

    QString corruptedItems;
    inputDataStream >> info.lastPage;
    for (quint32 page = 1; page <= info.lastPage; ++page)
    {
        if (bytes.mid(inputDataStream.device()->pos(), 2) != ItemParser::kPlugyPageHeader)
        {
            ERROR_BOX(tr("Page %1 of '%2' has wrong PlugY header").arg(page).arg(QFileInfo(info.path).fileName()));
            return;
        }
        inputDataStream.skipRawData(3);
        if (bytes.mid(inputDataStream.device()->pos(), 2) != ItemParser::kItemHeader)
        {
            ERROR_BOX(tr("Page %1 of '%2' has wrong item header").arg(page).arg(QFileInfo(info.path).fileName()));
            return;
        }
        inputDataStream.skipRawData(2);

        quint16 itemsOnPage;
        inputDataStream >> itemsOnPage;
        ItemsList plugyItems;
        corruptedItems += ItemParser::parseItemsToBuffer(itemsOnPage, inputDataStream, bytes, tr("Corrupted item detected in %1 on page %4 at (%2,%3)"), &plugyItems);
        foreach (ItemInfo *item, plugyItems)
        {
            item->storage = iter.key();
            item->plugyPage = page;
        }
        items->append(plugyItems);
    }
    if (!corruptedItems.isEmpty())
        ERROR_BOX(corruptedItems.trimmed());
}

void MedianXLOfflineTools::clearUI()
{
    _isLoaded = false;
    _resurrectionPenalty = ResurrectPenaltyDialog::Nothing;

    QList<QLineEdit *> lineEdits = QList<QLineEdit *>() << ui.classLineEdit << ui.titleLineEdit << ui.mercLevelLineEdit;
    foreach (QLineEdit *lineEdit, lineEdits)
        lineEdit->clear();
    foreach (QLineEdit *lineEdit, _lineEditsStatsMap)
    {
        lineEdit->clear();
        lineEdit->setStatusTip(QString());
    }
    ui.charNamePreview->clear();

    QList<QComboBox *> comboBoxes = QList<QComboBox *>()/* << ui.currentDifficultyComboBox*/ << ui.mercNameComboBox << ui.mercTypeComboBox;
    foreach (QComboBox *combobx, comboBoxes)
        combobx->clear();

    foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
    {
        spinBox->setMinimum(0);
        spinBox->setValue(0);
        spinBox->setStatusTip(QString());
    }
    ui.levelSpinBox->setValue(1);
    //ui.currentActSpinBox->setValue(1);

    QList<QCheckBox *> checkBoxes = QList<QCheckBox *>() << ui.convertToSoftcoreCheckBox << ui.activateWaypointsCheckBox << ui.respecSkillsCheckBox;
    foreach (QList<QCheckBox *> questCheckBoxes, _checkboxesQuestsHash.values())
        checkBoxes << questCheckBoxes;
    foreach (QCheckBox *checkbox, checkBoxes)
        checkbox->setChecked(false);
    ui.respecStatsButton->setChecked(false);

    QList<QGroupBox *> groupBoxes = QList<QGroupBox *>() << ui.characterGroupBox << ui.statsGroupBox << ui.mercGroupBox << _questsGroupBox;
    foreach (QGroupBox *groupBox, groupBoxes)
        groupBox->setDisabled(true);

    QList<QAction *> actions = QList<QAction *>() << ui.actionReloadCharacter << ui.actionSaveCharacter << ui.actionRespecStats << ui.actionRespecSkills << ui.actionActivateWaypoints << ui.actionRename << ui.actionConvertToSoftcore
                                                  << ui.actionResurrect << ui.actionFind << ui.actionFindNext << ui.actionFindPrevious << ui.actionShowItems << ui.actionSkillPlan << ui.actionExportCharacterInfo;
    foreach (QAction *action, actions)
        action->setDisabled(true);

    for (int i = 0; i < ui.statsTableWidget->rowCount(); ++i)
    {
        for (int j = 0; j < ui.statsTableWidget->columnCount(); ++j)
        {
            QTableWidgetItem *item = ui.statsTableWidget->item(i, j);
            if (!item)
            {
                item = new QTableWidgetItem;
                item->setTextAlignment(Qt::AlignCenter);
                ui.statsTableWidget->setItem(i, j, item);
            }
            item->setText(QString());
        }
    }

    _expGroupBox->reset();
    _mercExpGroupBox->reset();
}

void MedianXLOfflineTools::updateUI()
{
    clearUI();

    QList<QAction *> actions = QList<QAction *>() << ui.actionReloadCharacter << ui.actionRespecStats << ui.actionRespecSkills << ui.actionActivateWaypoints
                                                  << ui.actionRename << ui.actionSkillPlan << ui.actionExportCharacterInfo;
    foreach (QAction *action, actions)
        action->setEnabled(true);
    ui.respecSkillsCheckBox->setEnabled(true);

    QList<QGroupBox *> groupBoxes = QList<QGroupBox *>() << ui.characterGroupBox << ui.statsGroupBox << _questsGroupBox;
    foreach (QGroupBox *groupBox, groupBoxes)
        groupBox->setEnabled(true);

    const CharacterInfo &charInfo = CharacterInfo::instance();
    QD2CharRenamer::updateNamePreview(ui.charNamePreview, charInfo.basicInfo.originalName);

    ui.hardcoreGroupBox->setEnabled(charInfo.basicInfo.isHardcore);
    if (charInfo.basicInfo.isHardcore)
        updateHardcoreUIElements();

    ui.classLineEdit->setText(Enums::ClassName::classes().at(charInfo.basicInfo.classCode));
    updateCharacterTitle(charInfo.basicInfo.isHardcore);

    //ui.currentDifficultyComboBox->setEnabled(true);
    //ui.currentDifficultyComboBox->addItems(difficulties.mid(0, titleAndMaxDifficulty.second + 1));
    //ui.currentDifficultyComboBox->setCurrentIndex(charInfo.basicInfo.currentDifficulty);
    //ui.currentActSpinBox->setValue(charInfo.basicInfo.currentAct);

    _oldClvl = charInfo.basicInfo.level;
    ui.levelSpinBox->setMaximum(_oldClvl);
    ui.levelSpinBox->setValue(_oldClvl);

    updateCharacterExperienceProgressbar(charInfo.valueOfStatistic(Enums::CharacterStats::Experience));

    setStats();
    recalculateStatPoints();

    int stats = charInfo.basicInfo.totalStatPoints, skills = charInfo.basicInfo.totalSkillPoints;
    updateStatusTips(stats, stats - charInfo.valueOfStatistic(Enums::CharacterStats::FreeStatPoints), skills, skills - charInfo.valueOfStatistic(Enums::CharacterStats::FreeSkillPoints));
    ui.signetsOfLearningEatenLineEdit->setStatusTip(maxValueFormat.arg(Enums::CharacterStats::SignetsOfLearningMax));
    ui.signetsOfSkillEatenLineEdit->setStatusTip(maxValueFormat.arg(Enums::CharacterStats::SignetsOfSkillMax));
    ui.stashGoldLineEdit->setStatusTip(maxValueFormat.arg(QLocale().toString(Enums::CharacterStats::StashGoldMax)));
    if (_sharedGold)
        ui.stashGoldLineEdit->setStatusTip(ui.stashGoldLineEdit->statusTip() + ", " + tr("Shared: %1", "amount of gold in shared stash").arg(QLocale().toString(_sharedGold)));

    if (charInfo.mercenary.exists)
    {
        int mercTypeIndex;
        QPair<int, int> mercArraySlice = Enums::Mercenary::allowedTypesForMercCode(charInfo.mercenary.code, &mercTypeIndex);
        ui.mercTypeComboBox->addItems(Enums::Mercenary::types().mid(mercArraySlice.first, mercArraySlice.second));
        ui.mercTypeComboBox->setCurrentIndex(mercTypeIndex);

        ui.mercNameComboBox->addItems(mercNames.at(Enums::Mercenary::mercNamesIndexFromCode(charInfo.mercenary.code)));
        ui.mercNameComboBox->setCurrentIndex(charInfo.mercenary.nameIndex);

        ui.mercLevelLineEdit->setText(QString::number(charInfo.mercenary.level));

        if ((charInfo.mercenary.level == Enums::CharacterStats::MaxNonHardenedLevel - 1 && charInfo.mercenary.experience < mercExperienceForLevel(Enums::CharacterStats::MaxNonHardenedLevel - 1) + 5) ||
             charInfo.mercenary.level == Enums::CharacterStats::MaxLevel - 1)
        {
            // display levels 119 and 125 as 100% of progressbar
            _mercExpGroupBox->setPreviousLevelExperience(mercExperienceForLevel(charInfo.mercenary.level - 1));
            _mercExpGroupBox->setNextLevelExperience(charInfo.mercenary.experience);
        }
        else
        {
            _mercExpGroupBox->setPreviousLevelExperience(mercExperienceForLevel(charInfo.mercenary.level));
            _mercExpGroupBox->setNextLevelExperience(mercExperienceForLevel(charInfo.mercenary.level + 1));
        }
        _mercExpGroupBox->setCurrentExperience(charInfo.mercenary.experience);

        ui.mercGroupBox->setEnabled(true);
    }

    for (int i = 0; i < kDifficultiesNumber; ++i)
    {
        _checkboxesQuestsHash[Enums::Quests::DenOfEvil][i]->setChecked(charInfo.questsInfo.denOfEvil.at(i));
        _checkboxesQuestsHash[Enums::Quests::Radament][i]->setChecked(charInfo.questsInfo.radament.at(i));
        _checkboxesQuestsHash[Enums::Quests::Izual][i]->setChecked(charInfo.questsInfo.izual.at(i));
        _checkboxesQuestsHash[Enums::Quests::LamEsensTome][i]->setChecked(charInfo.questsInfo.lamEsensTome.at(i));
    }

    bool hasItems = !charInfo.items.character.isEmpty();
    ui.actionShowItems->setEnabled(hasItems);
    ui.actionFind->setEnabled(hasItems);
    ui.actionGiveCube->setDisabled(CharacterInfo::instance().items.hasCube());

    updateWindowTitle();

    _isLoaded = true;
}

void MedianXLOfflineTools::updateHardcoreUIElements()
{
    bool hadDied = CharacterInfo::instance().basicInfo.hadDied;
    ui.convertToSoftcoreCheckBox->setDisabled(hadDied);
    ui.actionConvertToSoftcore->setDisabled(hadDied);
    ui.resurrectButton->setEnabled(hadDied);
    ui.actionResurrect->setEnabled(hadDied);
}

void MedianXLOfflineTools::updateCharacterTitle(bool isHardcore)
{
    const CharacterInfo::CharacterInfoBasic &basicInfo = CharacterInfo::instance().basicInfo;
    bool isMale = basicInfo.classCode >= Enums::ClassName::Necromancer && basicInfo.classCode <= Enums::ClassName::Druid;
    quint8 progression = basicInfo.titleCode;
    QPair<QString, quint8> titleAndMaxDifficulty = Enums::Progression::titleNameAndMaxDifficultyFromValue(progression, isMale, isHardcore);
    QString newTitle = titleAndMaxDifficulty.first;
    if (basicInfo.isHardcore && basicInfo.hadDied)
        newTitle += QString(" (%1)").arg(tr("DEAD", "HC character is dead"));
    ui.titleLineEdit->setText(newTitle);
    ui.titleLineEdit->setStyleSheet(QString("%1; color: %2;").arg(kReadonlyCss, isHardcore ? "red" : "black"));
}

void MedianXLOfflineTools::setStats()
{
    QMetaEnum statisticMetaEnum = Enums::CharacterStats::statisticMetaEnum();
    for (int i = 0; i < statisticMetaEnum.keyCount(); ++i)
    {
        const char *name = statisticMetaEnum.key(i);
        qulonglong value = CharacterInfo::instance().basicInfo.statsDynamicData.property(name).toULongLong();
        Enums::CharacterStats::StatisticEnum statCode = static_cast<Enums::CharacterStats::StatisticEnum>(statisticMetaEnum.value(i));

        if (statCode >= Enums::CharacterStats::Strength && statCode <= Enums::CharacterStats::Vitality)
        {
            _oldStatValues[statCode] = value;

            quint8 baseValue = _baseStatsMap[CharacterInfo::instance().basicInfo.classCode].statsAtStart.statFromCode(statCode);
            QSpinBox *spinBox = _spinBoxesStatsMap[statCode];
            spinBox->setMinimum(baseValue);
            spinBox->setMaximum(value);
            spinBox->setValue(value);
        }
        else if (statCode >= Enums::CharacterStats::Life && statCode <= Enums::CharacterStats::BaseStamina)
        {
            int j = statCode - Enums::CharacterStats::Life, row = j / 2, col = j % 2;
            QTableWidgetItem *item = ui.statsTableWidget->item(row, col);
            if (!item)
            {
                item = new QTableWidgetItem;
                item->setTextAlignment(Qt::AlignCenter);
                ui.statsTableWidget->setItem(row, col, item);
            }
            item->setText(QString::number(value));
        }
        else if (statCode != Enums::CharacterStats::End && statCode != Enums::CharacterStats::Level && statCode != Enums::CharacterStats::Experience)
        {
            _lineEditsStatsMap[statCode]->setText(QString::number(value));
        }
    }

    int freeStats = ui.freeStatPointsLineEdit->text().toInt();
    if (freeStats)
        foreach (QSpinBox *spinBox, _spinBoxesStatsMap)
            spinBox->setMaximum(spinBox->maximum() + freeStats);
}

void MedianXLOfflineTools::updateWindowTitle()
{
    _charPathLabel->setText(QDir::toNativeSeparators(_charPath));
    QString title = QString("%1 (%2)").arg(qApp->applicationName(), SkillplanDialog::modVersionReadable());
    // making setWindowFilePath() work correctly
#ifdef Q_WS_MACX
    setWindowTitle(_charPath.isEmpty() ? SkillplanDialog::modVersionReadable() : QString("%1 (%2)").arg(QFileInfo(_charPath).fileName(), SkillplanDialog::modVersionReadable()));
#else
    setWindowTitle(_charPath.isEmpty() ? title : QString("%1[*] %2 %3").arg(QFileInfo(_charPath).fileName(), QChar(0x2014), title));
#endif
}

void MedianXLOfflineTools::updateTableStats(const BaseStats::StatsStep &statsPerStep, int diff, QSpinBox *senderSpinBox /*= 0*/)
{
    if (!senderSpinBox || (senderSpinBox && senderSpinBox == ui.vitalitySpinBox))
    {
        updateTableItemStat(ui.statsTableWidget->item(0, 0), diff, statsPerStep.life); // current life
        updateTableItemStat(ui.statsTableWidget->item(0, 1), diff, statsPerStep.life); // base life
        updateTableItemStat(ui.statsTableWidget->item(2, 0), diff, statsPerStep.stamina); // current stamina
        updateTableItemStat(ui.statsTableWidget->item(2, 1), diff, statsPerStep.stamina); // base stamina
    }
    if (!senderSpinBox || (senderSpinBox && senderSpinBox == ui.energySpinBox))
    {
        updateTableItemStat(ui.statsTableWidget->item(1, 0), diff, statsPerStep.mana); // current mana
        updateTableItemStat(ui.statsTableWidget->item(1, 1), diff, statsPerStep.mana); // base mana
    }
}

void MedianXLOfflineTools::updateTableItemStat(QTableWidgetItem *item, int diff, int statPerPoint)
{
    double newValue = item->text().toDouble() + diff * statPerPoint / 4.0;
    if (newValue > 8191)
        newValue = 8191;

    if (newValue > 0)
    {
        double foo;
        item->setText(modf(newValue, &foo) > 0.000001 ? QString::number(newValue, 'f', 1) : QString::number(newValue));
    }
    else
        item->setText("0");
}

void MedianXLOfflineTools::updateStatusTips(int newStatPoints, int investedStatPoints, int newSkillPoints, int investedSkillPoints)
{
    updateMaxCompoundStatusTip(ui.freeStatPointsLineEdit, newStatPoints, investedStatPoints);
    updateMaxCompoundStatusTip(ui.freeSkillPointsLineEdit, newSkillPoints, investedSkillPoints);
    ui.inventoryGoldLineEdit->setStatusTip(maxValueFormat.arg(QLocale().toString(_oldClvl * Enums::CharacterStats::InventoryGoldFactor)));
}

void MedianXLOfflineTools::updateCompoundStatusTip(QWidget *widget, const QString &firstString, const QString &secondString)
{
    widget->setStatusTip(kCompoundFormat.arg(firstString, secondString));
}

void MedianXLOfflineTools::updateMinCompoundStatusTip(QWidget *widget, int minValue, int investedValue)
{
    updateCompoundStatusTip(widget, minValueFormat.arg(minValue), investedValueFormat.arg(investedValue));
}

void MedianXLOfflineTools::updateMaxCompoundStatusTip(QWidget *widget, int maxValue, int investedValue)
{
    updateCompoundStatusTip(widget, maxValueFormat.arg(maxValue), investedValueFormat.arg(investedValue));
}

void MedianXLOfflineTools::updateAssociateAction(bool disable)
{
    ui.actionAssociate->setDisabled(disable);
    ui.actionAssociate->setStatusTip(disable ? tr("Application is default already") : QString());
}

void MedianXLOfflineTools::updateCharacterExperienceProgressbar(quint32 newExperience)
{
    if ((_oldClvl == Enums::CharacterStats::MaxNonHardenedLevel && newExperience < experienceTable.at(Enums::CharacterStats::MaxNonHardenedLevel - 1) + 5) || _oldClvl == Enums::CharacterStats::MaxLevel)
    {
        // display levels 120 and 126 as 100% of progressbar
        _expGroupBox->setPreviousLevelExperience(experienceTable.at(_oldClvl - 2));
        _expGroupBox->setNextLevelExperience(newExperience);
    }
    else
    {
        _expGroupBox->setPreviousLevelExperience(experienceTable.at(_oldClvl - 1));
        _expGroupBox->setNextLevelExperience(experienceTable.at(_oldClvl));
    }
    _expGroupBox->setCurrentExperience(newExperience);
}

QByteArray MedianXLOfflineTools::statisticBytes()
{
    QString result;
    QMetaEnum statisticMetaEnum = Enums::CharacterStats::statisticMetaEnum();
    bool isExpAndLevelNotSet = true;
    for (int i = 0; i < statisticMetaEnum.keyCount(); ++i)
    {
        const char *enumKey = statisticMetaEnum.key(i);
        Enums::CharacterStats::StatisticEnum statCode = static_cast<Enums::CharacterStats::StatisticEnum>(statisticMetaEnum.value(i));
        qulonglong value = 0;
        if (statCode >= Enums::CharacterStats::Strength && statCode <= Enums::CharacterStats::Vitality)
        {
            value = _spinBoxesStatsMap[statCode]->value();
        }
        else if (statCode >= Enums::CharacterStats::Life && statCode <= Enums::CharacterStats::BaseStamina)
        {
            int j = statCode - Enums::CharacterStats::Life, row = j / 2, col = j % 2;
            value = static_cast<qulonglong>(ui.statsTableWidget->item(row, col)->text().toDouble()) << 8;
        }
        else if (statCode != Enums::CharacterStats::End && statCode != Enums::CharacterStats::Level && statCode != Enums::CharacterStats::Experience)
        {
            value = _lineEditsStatsMap[statCode]->text().toULongLong();
            // signets should be set to (max + 1)
            if ((statCode == Enums::CharacterStats::SignetsOfLearningEaten && value == Enums::CharacterStats::SignetsOfLearningMax) ||
                (statCode == Enums::CharacterStats::SignetsOfSkillEaten    && value == Enums::CharacterStats::SignetsOfSkillMax))
            {
                ++value;
            }
#ifndef MAKE_FINISHED_CHARACTER
            else if (statCode == Enums::CharacterStats::FreeStatPoints)
            {
                int totalPossibleFreeStats = totalPossibleStatPoints(ui.levelSpinBox->value()) - investedStatPoints();
                if (value > static_cast<quint32>(totalPossibleFreeStats)) // prevent hacks and shut the compiler up
                {
                    value = totalPossibleFreeStats;
                    WARNING_BOX(hackerDetected);
                }
            }
#endif
        }
        else if (statCode != Enums::CharacterStats::End && isExpAndLevelNotSet) // level or exp
        {
            QObject &statsDynamicData = CharacterInfo::instance().basicInfo.statsDynamicData;
            quint8 clvl = CharacterInfo::instance().basicInfo.level, newClvl = ui.levelSpinBox->value();
            if (clvl != newClvl) // set new level and experience explicitly
            {
                addStatisticBits(result, Enums::CharacterStats::Level, Enums::CharacterStats::StatCodeLength);
                addStatisticBits(result, newClvl, ItemDataBase::Properties()->value(Enums::CharacterStats::Level)->saveBits);
                statsDynamicData.setProperty("Level", newClvl);

                quint32 newExp = experienceTable.at(newClvl - 1);
                if (newExp) // must not be present for level 1 character
                {
                    addStatisticBits(result, Enums::CharacterStats::Experience, Enums::CharacterStats::StatCodeLength);
                    addStatisticBits(result, newExp, ItemDataBase::Properties()->value(Enums::CharacterStats::Experience)->saveBits);
                }
                statsDynamicData.setProperty("Experience", newExp);

                isExpAndLevelNotSet = false;
                continue;
            }
            else
                value = statsDynamicData.property(enumKey).toULongLong();
        }
        else if (statCode == Enums::CharacterStats::End) // byte align
        {
            addStatisticBits(result, statCode, 16 - result.length() % 8);
            break; // not necessary actually
        }
        else
            continue;

        if (value)
        {
            addStatisticBits(result, statCode, Enums::CharacterStats::StatCodeLength);
            addStatisticBits(result, value, ItemDataBase::Properties()->value(statCode)->saveBits);
        }

        CharacterInfo::instance().basicInfo.statsDynamicData.setProperty(enumKey, value);
    }

    int bitsCount = result.length();
    if (bitsCount % 8)
    {
        ERROR_BOX(tr("Stats string is not byte aligned!"));
        return QByteArray();
    }

    QByteArray resultBytes;
    resultBytes.reserve(bitsCount / 8);
    for (int startPos = bitsCount - 8; startPos >= 0; startPos -= 8)
    {
        quint8 aByte = result.mid(startPos, 8).toUShort(0, 2);
        resultBytes += aByte;
    }
    return resultBytes;
}

void MedianXLOfflineTools::addStatisticBits(QString &bitsString, quint64 number, int fieldWidth)
{
    bitsString.prepend(binaryStringFromNumber(number, false, fieldWidth));
}

void MedianXLOfflineTools::clearItems(bool sharedStashPathChanged /*= true*/, bool hcStashPathChanged /*= true*/)
{
    QMutableListIterator<ItemInfo *> itemIterator(CharacterInfo::instance().items.character);
    while (itemIterator.hasNext())
    {
        ItemInfo *item = itemIterator.next();
        switch (item->storage)
        {
        case Enums::ItemStorage::SharedStash:
            if ((sharedStashPathChanged || ui.actionAutoOpenSharedStash->isChecked()) && !sharedStashPathChanged)
                continue;
            break;
        case Enums::ItemStorage::HCStash:
            if ((hcStashPathChanged || ui.actionAutoOpenHCShared->isChecked()) && !hcStashPathChanged)
                continue;
            break;
        default:
            break;
        }

        delete item;
        itemIterator.remove();
    }
}

void MedianXLOfflineTools::backupFile(QFile &file)
{
    if (ui.actionBackup->isChecked())
    {
        if (int backupsLimit = _backupLimitsGroup->checkedAction()->data().toInt())
        {
            QFileInfo fi(file.fileName());
            QDir sourceFileDir(fi.canonicalPath(), QString("%1_*.%2").arg(fi.fileName()).arg(kBackupExtension), QDir::Name | QDir::IgnoreCase, QDir::Files);
            QStringList previousBackups = sourceFileDir.entryList();
            while (previousBackups.size() >= backupsLimit)
                sourceFileDir.remove(previousBackups.takeFirst());
        }

        QFile backupFile(QString("%1_%2.%3").arg(file.fileName()).arg(ui.actionBackupFormatReadable->isChecked() ? QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss") : QString::number(QDateTime::currentMSecsSinceEpoch())).arg(kBackupExtension));
        if (backupFile.exists() && !backupFile.remove()) // it shouldn't actually exist, but let's be safe
            showErrorMessageBoxForFile(tr("Error removing old backup '%1'"), backupFile);
        else
        {
            if (!file.copy(backupFile.fileName()))
                showErrorMessageBoxForFile(tr("Error creating backup of '%1'"), file);
        }
    }
}

QHash<int, bool> MedianXLOfflineTools::getPlugyStashesExistenceHash() const
{
    QHash<int, bool> plugyStashesExistenceHash;
    for (QHash<Enums::ItemStorage::ItemStorageEnum, PlugyStashInfo>::const_iterator iter = _plugyStashesHash.constBegin(); iter != _plugyStashesHash.constEnd(); ++iter)
        plugyStashesExistenceHash[iter.key()] = iter.value().exists;
    return plugyStashesExistenceHash;
}

void MedianXLOfflineTools::showErrorMessageBoxForFile(const QString &message, const QFile &file)
{
    CUSTOM_BOX_OK(critical, message.arg(QDir::toNativeSeparators(file.fileName())) + "\n" + tr("Reason: %1", "error with file").arg(file.errorString()));
}

bool MedianXLOfflineTools::maybeSave()
{
    if (isWindowModified())
    {
        QMessageBox msgBox(QMessageBox::Warning, qApp->applicationName(), tr("Character has been modified."), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        msgBox.setInformativeText(tr("Do you want to save your changes?"));
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.setWindowModality(Qt::WindowModal);
        switch (msgBox.exec())
        {
        case QMessageBox::Save:
            saveCharacter();
            break;
        case QMessageBox::Cancel:
            return false;
        case QMessageBox::Discard:
            setModified(false);
            break;
        default:
            break;
        }
    }
    return true;
}

void MedianXLOfflineTools::checkForUpdateFromUrl(const QUrl &url)
{
    _qnam = new QNetworkAccessManager;
    connect(_qnam, SIGNAL(finished(QNetworkReply *)), SLOT(networkReplyFinished(QNetworkReply *)));
    qApp->processEvents(); // prevents from freezing UI
    _qnam->get(QNetworkRequest(url));
}

void MedianXLOfflineTools::networkReplyFinished(QNetworkReply *reply)
{
    QByteArray webpage = reply->readAll();
    reply->deleteLater();
    _qnam->deleteLater();

    QRegExp rx(QString("%1 v(.+)</a>").arg(qApp->applicationName()));
    rx.setMinimal(true);
    if (rx.indexIn(webpage) != -1)
    {
        QString version = rx.cap(1);
        if (qApp->applicationVersion() < version)
            INFO_BOX(tr("New version <b>%1</b> is available!").arg(version) + kHtmlLineBreak + kHtmlLineBreak + kForumThreadHtmlLinks);
        else if (_isManuallyCheckingForUpdate)
            INFO_BOX(tr("You have the latest version"));
    }
    else
    {
        QUrl newUrl("http://forum.worldofplayers.ru/forumdisplay.php?f=935");
        if (newUrl != reply->url())
            checkForUpdateFromUrl(newUrl);
        else if (_isManuallyCheckingForUpdate)
            ERROR_BOX(tr("Error contacting update server"));
    }
}
