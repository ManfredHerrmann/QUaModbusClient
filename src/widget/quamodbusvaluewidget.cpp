#include "quamodbusvaluewidget.h"
#include "ui_quamodbusvaluewidget.h"

#include <QMessageBox>

#include <QUaModbusDataBlock>
#include <QUaModbusValue>

#include <QUaModbusClientDialog>

#ifdef QUA_ACCESS_CONTROL
#include <QUaDockWidgetPerms>
#endif // QUA_ACCESS_CONTROL

QUaModbusValueWidget::QUaModbusValueWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QUaModbusValueWidget)
{
    ui->setupUi(this);
#ifndef QUA_ACCESS_CONTROL
	ui->pushButtonPerms->setVisible(false);
#endif // !QUA_ACCESS_CONTROL
}

QUaModbusValueWidget::~QUaModbusValueWidget()
{
    delete ui;
}

void QUaModbusValueWidget::bindValue(QUaModbusValue * value)
{
	// disable old connections
	while (m_connections.count() > 0)
	{
		QObject::disconnect(m_connections.takeFirst());
	}
	// check if valid
	if (!value)
	{
		this->setEnabled(false);
		return;
	}
	// bind common
	m_connections <<
	QObject::connect(value, &QObject::destroyed, this,
	[this]() {
		this->bindValue(nullptr);
	});
	// enable
	this->setEnabled(true);
	// bind edit widget
	this->bindValueWidgetEdit(value);
	// bind status widget
	this->bindValueWidgetStatus(value);
	// bind buttons
#ifdef QUA_ACCESS_CONTROL
	m_connections <<
	QObject::connect(ui->pushButtonPerms, &QPushButton::clicked, value,
	[this, value]() {
		// NOTE : call QUaModbusClientWidget::setupPermissionsModel first to set m_proxyPerms
		Q_CHECK_PTR(m_proxyPerms);
		// create permissions widget
		auto permsWidget = new QUaDockWidgetPerms;
		// configure perms widget combo
		permsWidget->setComboModel(m_proxyPerms);
		permsWidget->setPermissions(value->permissionsObject());
		// dialog
		QUaModbusClientDialog dialog(this);
		dialog.setWindowTitle(tr("Modbus Value Permissions"));
		dialog.setWidget(permsWidget);
		// exec dialog
		int res = dialog.exec();
		if (res != QDialog::Accepted)
		{
			return;
		}
		// read permissions and set them for layout list
		value->setPermissionsObject(permsWidget->permissions());
	});
#endif // QUA_ACCESS_CONTROL
	m_connections <<
	QObject::connect(ui->pushButtonDelete, &QPushButton::clicked, value,
	[this, value]() {
		Q_CHECK_PTR(value);
		// are you sure?
		auto res = QMessageBox::question(
			this,
			tr("Delete Value Confirmation"),
			tr("Would you like to delete value %1?").arg(value->browseName()),
			QMessageBox::StandardButton::Ok,
			QMessageBox::StandardButton::Cancel
		);
		if (res != QMessageBox::StandardButton::Ok)
		{
			return;
		}
		// delete
		value->remove();
		// NOTE : removed from tree on &QObject::destroyed callback
	});
	// NOTE : apply button bound in bindValueWidgetEdit
}

void QUaModbusValueWidget::clear()
{
	// disable old connections
	while (m_connections.count() > 0)
	{
		QObject::disconnect(m_connections.takeFirst());
	}
	// clear edit widget
	ui->widgetValueEdit->setId("");
	// clear status widget
}

#ifdef QUA_ACCESS_CONTROL
void QUaModbusValueWidget::setupPermissionsModel(QSortFilterProxyModel * proxyPerms)
{
	m_proxyPerms = proxyPerms;
	Q_CHECK_PTR(m_proxyPerms);
}

void QUaModbusValueWidget::setCanWrite(const bool & canWrite)
{
	ui->widgetValueEdit->setTypeEditable(canWrite);
	ui->widgetValueEdit->setOffsetEditable(canWrite);
	ui->pushButtonApply->setEnabled(canWrite);
	ui->pushButtonDelete->setVisible(canWrite);
}

void QUaModbusValueWidget::setCanWriteValueList(const bool & canWrite)
{
	ui->pushButtonPerms->setVisible(canWrite);
}
#endif // QUA_ACCESS_CONTROL

void QUaModbusValueWidget::bindValueWidgetEdit(QUaModbusValue * value)
{
	// id
	ui->widgetValueEdit->setIdEditable(false);
	ui->widgetValueEdit->setId(value->browseName());
	// type
	ui->widgetValueEdit->setType(value->getType());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::typeChanged, ui->widgetValueEdit,
	[this](const QModbusValueType &type) {
		ui->widgetValueEdit->setType(type);
	});
	// offset
	ui->widgetValueEdit->setOffset(value->getAddressOffset());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::addressOffsetChanged, ui->widgetValueEdit,
	[this](const int &addressOffset) {
		ui->widgetValueEdit->setOffset(addressOffset);
	});
	// on apply
	m_connections <<
	QObject::connect(ui->pushButtonApply, &QPushButton::clicked, ui->widgetValueEdit,
	[value, this]() {
		value->setType(ui->widgetValueEdit->type());
		value->setAddressOffset(ui->widgetValueEdit->offset());
		// NOTE : do not change value on the fly, but only when user click apply
		value->setValue(ui->widgetValueStatus->value());
	});
}

void QUaModbusValueWidget::bindValueWidgetStatus(QUaModbusValue * value)
{
	// unfreeze status widget
	ui->widgetValueStatus->setIsFrozen(false);
	// type
	ui->widgetValueStatus->setType(value->getType());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::typeChanged, ui->widgetValueStatus,
	[this, value](const QModbusValueType & type) {
		ui->widgetValueStatus->setType(type);
		ui->widgetValueStatus->setValue(value->getValue());
	});
	// status
	ui->widgetValueStatus->setStatus(value->getLastError());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::lastErrorChanged, ui->widgetValueStatus,
	[this](const QModbusError & error) {
		ui->widgetValueStatus->setStatus(error);
	});
	// registers used
	ui->widgetValueStatus->setRegistersUsed(value->getRegistersUsed());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::registersUsedChanged, ui->widgetValueStatus,
	[this](const quint16 & registersUsed) {
		ui->widgetValueStatus->setRegistersUsed(registersUsed);
	});
	// data & value
	auto data   = value->block()->getData();
	auto offset = value->getAddressOffset();
	auto size   = QUaModbusValue::typeBlockSize(value->getType());
	ui->widgetValueStatus->setData(data.mid(offset, size));
	ui->widgetValueStatus->setValue(value->getValue());
	m_connections <<
	QObject::connect(value, &QUaModbusValue::valueChanged, ui->widgetValueStatus,
	[this, value](const QVariant & varVal) {
		auto block  = value->block()->getData();
		auto offset = value->getAddressOffset();
		auto size   = QUaModbusValue::typeBlockSize(value->getType());
		ui->widgetValueStatus->setData(block.mid(offset, size));
		ui->widgetValueStatus->setValue(varVal);
	});
	// writable
	auto blkType = value->block()->getType();
	ui->widgetValueStatus->setWritable(blkType == QModbusDataBlockType::Coils || blkType == QModbusDataBlockType::HoldingRegisters);
	m_connections <<
	QObject::connect(value->block(), &QUaModbusDataBlock::typeChanged, ui->widgetValueStatus,
	[this](const QModbusDataBlockType &type) {
		ui->widgetValueStatus->setWritable(type == QModbusDataBlockType::Coils || type == QModbusDataBlockType::HoldingRegisters);
	});
	/*
	// NOTE : changed to change only when apply clicked
	// on value change (on the fly, as user types it)
	m_connections <<
	QObject::connect(ui->widgetValueStatus, &QUaModbusValueWidgetStatus::valueUpdated, value,
	[value](const QVariant &varVal) {
		value->setValue(varVal);
	});
	*/
}