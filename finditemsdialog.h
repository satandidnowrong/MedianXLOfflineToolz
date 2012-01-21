#ifndef FINDITEMSWIDGET_H
#define FINDITEMSWIDGET_H

#include "ui_finditemsdialog.h"

#include "structs.h"

#include <QDialog>


class FindItemsDialog : public QDialog
{
	Q_OBJECT

public:
	Ui::FindItemsDialog ui;

	FindItemsDialog(QWidget *parent = 0);

	void saveSettings();

public slots:
	void resetSearchStatus();
	void show();

signals:
	void itemFound(ItemInfo *item);

private slots:
	void findNext();
	void findPrevious();
	void showResults();

	void searchTextChanged();

private:
	ItemsList _searchResult;
	bool _searchPerformed;
	int _currentIndex;

	void performSearch();
	void nothingFound();

	void loadSettings();
	void updateWindowTitle();
	void changeItem();
	void setButtonsDisabled(bool disabled, bool updateResultButton = true);
};

#endif // FINDITEMSWIDGET_H
