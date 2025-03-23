#include "AddressInputDialog.h"
#include <QtGlobal>

#if QT_VERSION >= 0x050000
	#include <QtWidgets>
#else
	#include <QtGui>
#endif
//-----------

AddressInputDialog::AddressInputDialog(Executable *exe, bool isRaw, ColorSettings &_addrColors, QWidget *parent)
	: HexInputDialog(tr("Go to address"), tr("Address (hex):"), parent),
	myExe(exe), isRawAddr(isRaw),
	addrColors(_addrColors),
	cbox_isRva(NULL), otherCaptionLabel(NULL), otherEdit(NULL)
{
	this->setMinimumSize( QSize(250, 150) );
	cbox_isRva = new QCheckBox(tr("+ ImageBase (RVA -> VA)"));
	if (!isRawAddr) {
		layout_middleBox->addWidget(cbox_isRva);
	}
	
	const QString otherLabel = (isRawAddr) ? tr("= Virtual:") : tr("= Raw:");
	this->otherCaptionLabel = new QLabel(otherLabel);
	layout_middleBox->addWidget(this->otherCaptionLabel);

	otherEdit = new QLineEdit(this);
	layout_middleBox->addWidget(otherEdit);
	otherEdit->setReadOnly(true);
	if (isRawAddr) {
		layout_middleBox->addWidget(cbox_isRva);
	}

	setDescriptions();

	connect(le, SIGNAL(textChanged(const QString &)), this, SLOT(onAddrChanged()) );
	if (cbox_isRva) {
		connect(cbox_isRva, SIGNAL(stateChanged(int)), this, SLOT(onAddrTypeChanged()));
	}
}

Executable::addr_type AddressInputDialog::getAddrType()
{
	if (this->isRawAddr) {
		return Executable::RAW;
	}
	if (cbox_isRva) {
		if (cbox_isRva->isChecked()) return Executable::VA;
	}
	return Executable::RVA;
}

void AddressInputDialog::onAddrChanged()
{
	validateAddr();
}

offset_t AddressInputDialog::convertToOther(offset_t val, Executable::addr_type aT)
{
	if (!myExe) return INVALID_ADDR;
	
	if (!this->isRawAddr) {
		return myExe->toRaw(val, aT);
	}
	Executable::addr_type otherType = (cbox_isRva->isChecked()) ? Executable::VA : Executable::RVA;
	offset_t number = INVALID_ADDR;
	try {
		number = myExe->rawToRva(val);
		if (otherType == Executable::VA) {
			number = myExe->rvaToVa(number);
		}
	} catch (CustomException e) {
		number = INVALID_ADDR;
	}
	return number;
}

void AddressInputDialog::validateAddr()
{
	bool isAccepted = false;

	if (myExe) {
		Executable::addr_type aT = getAddrType();

		bool isValid = false;
		qulonglong val = getNumValue(&isValid);
		const offset_t otherFmt = convertToOther(val, aT);
		
		if (isValid && myExe->toRaw(val, aT) != INVALID_ADDR) {
			isAccepted = true;
		}
		if (otherFmt != INVALID_ADDR) {
			otherEdit->setText(QString::number(otherFmt, 16).toUpper());
		} else {
			otherEdit->setText(tr("<invalid>"));
		}
	}

	if (isAccepted == false) {
		this->le->setStyleSheet(tr("border: 2px solid red;"));
	} else {
		this->le->setStyleSheet("");
	}

	setTextColor();
}

void AddressInputDialog::setTextColor()
{
	Executable::addr_type aT = getAddrType();
	Executable::addr_type otherType = Executable::RAW;
	
	if (aT == Executable::RAW) {
		otherType = (cbox_isRva->isChecked()) ? Executable::VA : Executable::RVA;
	}

	//set primary color:
	QColor addrColor = addrColors.addrTypeToColor(aT);

	//set secondary color:
	QColor otherColor = addrColors.addrTypeToColor(otherType);

	QPalette palette;
	palette.setColor(QPalette::Text, addrColor);
	le->setPalette(palette);
	le->setStyleSheet(le->styleSheet() + "color : "+ addrColor.name() + ";");

	QPalette otherPalette;
	otherPalette.setColor(QPalette::Text, otherColor);
	this->otherEdit->setPalette(otherPalette);
	otherEdit->setStyleSheet(otherEdit->styleSheet() + "color : " + otherColor.name() + ";");
}

void AddressInputDialog::setDescriptions()
{
	Executable::addr_type aT = getAddrType();
	QString desc = tr("Raw");
	QString winDesc = (aT == Executable::RAW) ? tr("Raw Address") : tr("Virtual Address");
	if (aT == Executable::RVA) desc = "RVA";
	if (aT == Executable::VA) desc = "VA";
	
	this->setWindowTitle(tr("Go to ") + winDesc);
	this->captionLabel->setText(tr("Go to ") + desc + " (hex):");
}

void AddressInputDialog::onAddrTypeChanged()
{
	setDescriptions();

	if (!myExe) return;
	
	const Executable::addr_type aT = getAddrType();
	const offset_t val = getNumValue();
	if (isRawAddr) {
		//change the secondary field
		const offset_t otherAddr = this->convertToOther(val, aT);
		otherEdit->setText(QString::number(otherAddr, 16).toUpper());
	} else {
		//change the primary field:
		const offset_t newVal = (aT == Executable::RVA) ? this->myExe->VaToRva(val, false) : myExe->rvaToVa(val);
		this->setDefaultValue(newVal);
	}
	validateAddr();
}
