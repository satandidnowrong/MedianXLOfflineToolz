#ifndef ITEMPARSER_H
#define ITEMPARSER_H

#include "structs.h"

#include <QCoreApplication>


class QDataStream;
class QString;
class ReverseBitReader;

class ItemParser
{
	Q_DECLARE_TR_FUNCTIONS(ItemParser)

public:
	static ItemInfo *parseItem(QDataStream &inputDataStream, const QByteArray &bytes);
	static QMultiMap<int, ItemProperty> parseItemProperties(ReverseBitReader &bitReader, bool *ok);
	static void writeItems(const ItemsList &items, QDataStream &ds);

	static ItemInfo *loadItemFromFile(const QString &filePath);
	static ItemsList itemsLocatedAt(int storage, bool location = Enums::ItemLocation::Stored);
	static bool storeItemIn(ItemInfo *item, Enums::ItemStorage::ItemStorageEnum storage, quint8 rows, quint8 cols, int plugyPage = 0);
    static bool canStoreItemAt(quint8 row, quint8 col, const QByteArray &storeItemType, const ItemsList &items, int rowsTotal, int colsTotal, int plugyPage = 0);

	static const QByteArray itemHeader, plugyPageHeader;

private:
    static bool isCorrectItemTypeForRW(const QList<QByteArray> &itemTypes, const QList<QByteArray> &allowedItemTypes);
};

#endif // ITEMPARSER_H