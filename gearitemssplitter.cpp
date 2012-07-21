#include "gearitemssplitter.h"
#include "itemstoragetableview.h"
#include "itemdatabase.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>


GearItemsSplitter::GearItemsSplitter(ItemStorageTableView *itemsView, QWidget *parent) : ItemsPropertiesSplitter(itemsView, parent), _button1(new QPushButton(this)), _button2(new QPushButton(this)),
    kButtonNames(QStringList() << tr("Character") << tr("Mercenary") << tr("Corpse")), _currentGearButtonNameIndex(CharacterNameIndex)
{
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addWidget(_button1);
    hlayout->addWidget(_button2);

    QVBoxLayout *vlayout = static_cast<QVBoxLayout *>(widget(0)->layout());
    vlayout->removeWidget(_disenchantBox);
    vlayout->removeWidget(_upgradeBox);
    vlayout->addLayout(hlayout);

    delete _disenchantBox;
    delete _upgradeBox;

    connect(_button1, SIGNAL(clicked()), SLOT(changeGear()));
    connect(_button2, SIGNAL(clicked()), SLOT(changeGear()));
}

void GearItemsSplitter::setItems(const ItemsList &newItems)
{
    _allItems = newItems;
    _gearItems[CharacterNameIndex]  = ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Equipped, 0, &_allItems);
    _gearItems[CharacterNameIndex] += ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Belt,     0, &_allItems);
    _gearItems[MercenaryNameIndex]  = ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Merc,     0, &_allItems);
    _gearItems[CorpseNameIndex]     = ItemDataBase::itemsStoredIn(Enums::ItemStorage::NotInStorage, Enums::ItemLocation::Corpse,   0, &_allItems);

    changeButtonText(_button1, MercenaryNameIndex);
    changeButtonText(_button2, CorpseNameIndex);
    _button2->setDisabled(_gearItems[CorpseNameIndex].isEmpty());

    updateItemsForCurrentGear();
}

void GearItemsSplitter::changeGear()
{
    QPushButton *pressedButton = qobject_cast<QPushButton *>(sender());
    int pressedButtonNameIndex = kButtonNames.indexOf(pressedButton->text().left(pressedButton->text().indexOf(' ')));
    changeButtonText(pressedButton, _currentGearButtonNameIndex);
    _currentGearButtonNameIndex = static_cast<GearItemsSplitter::GearNameIndex>(pressedButtonNameIndex);

    updateItemsForCurrentGear();
}

void GearItemsSplitter::updateItemsForCurrentGear()
{
    const ItemsList &items = _gearItems[_currentGearButtonNameIndex];
    updateItems(items);
    emit itemCountChanged(items.size());
}

void GearItemsSplitter::changeButtonText(QPushButton *button, GearItemsSplitter::GearNameIndex nameIndex)
{
    button->setText(QString("%1 (%2)").arg(kButtonNames.at(nameIndex)).arg(_gearItems[nameIndex].size()));
}