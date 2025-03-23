#include "PackersTableModel.h"

enum COLS {
	COL_OFFSET = 0,
	COL_NAME,
	COL_SIG,
	COL_SECTION,
	MAX_COL
};

PackersTableModel::PackersTableModel(PeHandler *peHndl, QObject *parent)
	: PeTableModel(peHndl, parent)
{
	offsetFont.setCapitalization(QFont::AllUppercase);
}

QVariant PackersTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole) return QVariant();
	if (orientation == Qt::Horizontal) {
		switch (section) {
			case COL_OFFSET: return tr("Offset");
			case COL_NAME: return tr("Name");
			case COL_SIG: return tr("Signature");
			case COL_SECTION:  return tr("Section");
		}
	}
	return QVariant();
}

Qt::ItemFlags PackersTableModel::flags(const QModelIndex &index) const
{	
	if (!index.isValid())
		return Qt::NoItemFlags;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int PackersTableModel::columnCount(const QModelIndex &parent) const 
{ 
	return MAX_COL; 
}

int PackersTableModel::rowCount(const QModelIndex &parent) const 
{ 
	return this->myPeHndl->packerAtOffset.size(); 
}

QVariant PackersTableModel::data(const QModelIndex &index, int role) const
{
	int row = index.row();
	int column = index.column();

	if (role == Qt::ForegroundRole) return this->addrColor(index);

	if (column == COL_OFFSET) {
		if (role == Qt::FontRole) return offsetFont;
	}
	if (role == Qt::ToolTipRole) return toolTip(index);
	if (role != Qt::DisplayRole && role != Qt::EditRole) return QVariant();

	int size = this->myPeHndl->packerAtOffset.size();
	if (size <= row) return QVariant();

	FoundPacker &found = this->myPeHndl->packerAtOffset[row];

	uint32_t offset = found.offset;

	SectionHdrWrapper *sec = m_PE->getSecHdrAtOffset(offset, Executable::RAW);
	switch (column) {
		case COL_OFFSET: return QString::number(offset, 16);
		case COL_SIG: return QString::fromStdString(found.packerName);
		case COL_NAME: return QString::fromStdString(found.packerBytes);
		case COL_SECTION: {
			if (sec) return sec->mappedName;
		}
	}
	return QVariant();
}

offset_t PackersTableModel::getFieldOffset(QModelIndex index) const
{
	if (!index.isValid()) return 0;
	int row = index.row();
	int size = this->myPeHndl->packerAtOffset.size();
	if (size <= row) return 0;

	FoundPacker &found = this->myPeHndl->packerAtOffset[row];
	return found.offset;
}

bufsize_t PackersTableModel::getFieldSize(QModelIndex index) const
{
	if (!index.isValid()) return 0;
	int row = index.row();
	int size = this->myPeHndl->packerAtOffset.size();
	if (size <= row) return 0;
	FoundPacker &found = this->myPeHndl->packerAtOffset[row];
	return found.size;
}
