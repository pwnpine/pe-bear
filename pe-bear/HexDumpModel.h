#pragma once
#include <QtGlobal>

#include "QtCompat.h"
#include "gui_base/PeTableModel.h"
#include "ViewSettings.h"

class HexDumpModel : public PeTableModel//QAbstractTableModel, public PeViewItem
{
	Q_OBJECT

signals:
	void scrollReset();

public slots:
	void setHexView(bool isSet) { showHex = isSet; reset(); }

	void setShownContent(offset_t start, bufsize_t size);

public:
	HexDumpModel(PeHandler *peHndl, bool isHex, QObject *parent = 0);
	
	Executable::addr_type getAddrType() { return this->addrType; }
	bool isHexView() const { return showHex; }

	bufsize_t getPageSize() { return pageSize; };
	offset_t getStartOff() { return startOff; }

	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex &parent) const;

	QVariant data(const QModelIndex &index, int role) const;
	virtual bool setData(const QModelIndex &, const QVariant &, int role);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	virtual offset_t contentOffsetAt(const QModelIndex &index) const;
	QVariant getRawContentAt(const QModelIndex &index) const;
	QVariant getElement(offset_t offset) const;

	virtual ViewSettings* getSettings()
	{
		return &settings;
	}

	void changeSettings(HexViewSettings &newSettings) 
	{
		settings = newSettings;
	}

protected:
	Executable::addr_type addrType;

private:
	HexViewSettings settings;
	bool showHex;
	offset_t startOff, endOff;
	bufsize_t pageSize;

friend class HexTableView;
friend class OffsetHeader;
};
