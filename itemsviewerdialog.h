#ifndef ITEMSVIEWERWIDGET_H
#define ITEMSVIEWERWIDGET_H

#include "structs.h"
#include "enums.h"

#include <QWidget>


class ItemsPropertiesSplitter;
class QTabWidget;
class QCloseEvent;
class QModelIndex;

class ItemsViewerDialog : public QDialog
{
    Q_OBJECT

public:
    enum TabIndex
    {
        GearIndex = 0,
        InventoryIndex,
        CubeIndex,
        StashIndex,
        PersonalStashIndex,
        SharedStashIndex,
        HCStashIndex,
        LastIndex = HCStashIndex
    };

    static const int cellSize;
    static const QStringList tabNames;
    static const QList<int> rows;

    static int indexFromItemStorage(int storage);

    explicit ItemsViewerDialog(QWidget *parent);

    void updateItems();
    void showItem(ItemInfo *item);

    void enableCubeTab();

    void saveSettings();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void tabChanged(int newIndex);

private:
    QTabWidget *_tabWidget;

    void loadSettings();

    ItemsPropertiesSplitter *splitterAtIndex(int tabIndex);
};

#endif // ITEMSVIEWERWIDGET_H
