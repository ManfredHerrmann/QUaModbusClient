#include "quamodbusqmlcontext.h"

#ifdef QUA_ACCESS_CONTROL
#include <QUaUser>
#include <QUaPermissions>
#endif // QUA_ACCESS_CONTROL



/******************************************************************************************************
*/

QUaModbusClientQmlContext::QUaModbusClientQmlContext(QObject* parent) : QObject(parent)
{
#ifdef QUA_ACCESS_CONTROL
	m_canWrite = true;
	auto context = qobject_cast<QUaModbusQmlContext*>(parent);
	Q_ASSERT(context);
	m_loggedUser = context->loggedUser();
#endif // QUA_ACCESS_CONTROL
	if (QMetaType::type("QModbusClientType") == QMetaType::UnknownType)
	{
		qRegisterMetaType<QModbusClientType>("QModbusClientType");
	}
	if (QMetaType::type("QModbusState") == QMetaType::UnknownType)
	{
		qRegisterMetaType<QModbusState>("QModbusState");
	}
	if (QMetaType::type("QModbusError") == QMetaType::UnknownType)
	{
		qRegisterMetaType<QModbusError>("QModbusError");
	}
}

QString QUaModbusClientQmlContext::clientId() const
{
	Q_ASSERT(m_client);
	return m_client->browseName().name();
}

QModbusClientType QUaModbusClientQmlContext::type() const
{
	Q_ASSERT(m_client);
	return m_client->getType();
}

quint8 QUaModbusClientQmlContext::serverAddress() const
{
	Q_ASSERT(m_client);
	return m_client->getServerAddress();
}

void QUaModbusClientQmlContext::setServerAddress(const quint8& serverAddress)
{
	Q_ASSERT(m_client);
#ifdef QUA_ACCESS_CONTROL
	if (!m_canWrite)
	{
		return;
	}
#endif // QUA_ACCESS_CONTROL
	m_client->setServerAddress(serverAddress);
}

bool QUaModbusClientQmlContext::keepConnecting() const
{
	Q_ASSERT(m_client);
	return m_client->getKeepConnecting();
}

void QUaModbusClientQmlContext::setKeepConnecting(const bool& keepConnecting)
{
	Q_ASSERT(m_client);
#ifdef QUA_ACCESS_CONTROL
	if (!m_canWrite)
	{
		return;
	}
#endif // QUA_ACCESS_CONTROL
	m_client->setKeepConnecting(keepConnecting);
}

QModbusState QUaModbusClientQmlContext::state() const
{
	Q_ASSERT(m_client);
	return m_client->getState();
}

QModbusError QUaModbusClientQmlContext::lastError() const
{
	Q_ASSERT(m_client);
	return m_client->getLastError();
}

#ifdef QUA_ACCESS_CONTROL
bool QUaModbusClientQmlContext::canWrite() const
{
	return m_canWrite;
}
#endif // QUA_ACCESS_CONTROL

void QUaModbusClientQmlContext::bindClient(QUaModbusClient* client)
{
	// check valid arg
	Q_ASSERT(client);
	if (!client) { return; }
	// copy reference
	m_client = client;
#ifdef QUA_ACCESS_CONTROL
	auto perms = m_client->permissionsObject();
	m_canWrite = !perms ? true : perms->canUserWrite(m_loggedUser);
	QObject::connect(m_client, &QUaModbusClient::permissionsObjectChanged, this,
		[this]() {
			auto perms = m_client->permissionsObject();
			m_canWrite = !perms ? true : perms->canUserWrite(m_loggedUser);			
			emit this->canWriteChanged();
		});
#endif // QUA_ACCESS_CONTROL
	// susbcribe to changes
	// QUaModbusClient
	QObject::connect(m_client, &QUaModbusClient::serverAddressChanged , this, &QUaModbusClientQmlContext::serverAddressChanged );
	QObject::connect(m_client, &QUaModbusClient::keepConnectingChanged, this, &QUaModbusClientQmlContext::keepConnectingChanged);
	QObject::connect(m_client, &QUaModbusClient::stateChanged         , this, &QUaModbusClientQmlContext::stateChanged         );
	QObject::connect(m_client, &QUaModbusClient::lastErrorChanged     , this, &QUaModbusClientQmlContext::lastErrorChanged     );
	// TODO : tcp, serial

}

void QUaModbusClientQmlContext::clear()
{
	// unsubscribe
	while (!m_connections.isEmpty())
	{
		QObject::disconnect(m_connections.takeFirst());
	}
	//// delete children contexts

	// TODO

	//while (!m_alarms.isEmpty())
	//{
	//	auto context = m_alarms.take(m_alarms.firstKey()).value<VrAlarmQmlContext*>();
	//	delete context;
	//}
}



/******************************************************************************************************
*/

QUaModbusQmlContext::QUaModbusQmlContext(QObject *parent) : QObject(parent)
{
#ifdef QUA_ACCESS_CONTROL
	m_loggedUser = nullptr;
#endif // QUA_ACCESS_CONTROL
	// forward signal
	QObject::connect(this, &QUaModbusQmlContext::clientsChanged, this, &QUaModbusQmlContext::clientsModelChanged);
}


QUaModbusQmlContext::~QUaModbusQmlContext()
{

}

QVariantMap QUaModbusQmlContext::clients()
{
	return m_clients;
}

QVariant QUaModbusQmlContext::clientsModel()
{
	QList<QObject*> retList;
	for (auto clientVariant : m_clients)
	{
		retList << clientVariant.value<QUaModbusClientQmlContext*>();
	}
	return QVariant::fromValue(retList);
}

void QUaModbusQmlContext::bindClients(QUaModbusClientList* clients)
{
	// check valid arg
	Q_ASSERT(clients);
	if (!clients) { return; }
	// bind existing
	for (auto client : clients->clients())
	{
		// bind existing client
		this->bindClient(client);
	}
	// bind client added
	m_connections << QObject::connect(clients, &QUaNode::childAdded, this,
		[this](QUaNode* node) {
			// bind new client
			QUaModbusClient* client = qobject_cast<QUaModbusClient*>(node);
			Q_ASSERT(client);
			this->bindClient(client);
		}/*, Qt::QueuedConnection // NOTE : do not queue or clients will not be available on view load */);
#ifdef QUA_ACCESS_CONTROL
	m_connections << QObject::connect(this, &QUaModbusQmlContext::loggedUserChanged, clients,
		[this, clients]() {
			this->clear();
			this->bindClients(clients);
		});
#endif // QUA_ACCESS_CONTROL
}

void QUaModbusQmlContext::clear()
{
	// unsubscribe
	while (!m_connections.isEmpty())
	{
		QObject::disconnect(m_connections.takeFirst());
	}
	// delete client contexts
	while (!m_clients.isEmpty())
	{
		auto context = m_clients.take(m_clients.firstKey()).value<QUaModbusClientQmlContext*>();
		context->clear();
		delete context;
	}
}

#ifdef QUA_ACCESS_CONTROL
QUaUser* QUaModbusQmlContext::loggedUser() const
{
	return m_loggedUser;
}

void QUaModbusQmlContext::on_loggedUserChanged(QUaUser* user)
{
	m_loggedUser = user;
	emit this->loggedUserChanged(QPrivateSignal());
	// TODO : reset models
}
#endif

void QUaModbusQmlContext::bindClient(QUaModbusClient* client)
{
	Q_ASSERT(client);
	// NOTE : access control must be checked before anything due to early exit condition
#ifdef QUA_ACCESS_CONTROL
	m_connections <<
	QObject::connect(client, &QUaModbusClient::permissionsObjectChanged, this,
		[this, client]() {
			auto perms = client->permissionsObject();
			auto canRead = !perms ? true : perms->canUserRead(m_loggedUser);
			// add or remove client to/from exposed list
			if (canRead)
			{
				this->addClient(client);
			}
			else
			{
				this->removeClient(client);
			}
		});
	auto perms = client->permissionsObject();
	auto canRead = !perms ? true : perms->canUserRead(m_loggedUser);
	if (!canRead)
	{
		return;
	}
#endif // QUA_ACCESS_CONTROL
	// add client to exposed list
	this->addClient(client);
}

void QUaModbusQmlContext::addClient(QUaModbusClient* client)
{
	// get client id
	QString strId = client->browseName().name();
	Q_ASSERT(!strId.isEmpty() && !strId.isNull());
	// add client context to map
	auto context = new QUaModbusClientQmlContext(this);
	context->bindClient(client);
	Q_ASSERT(!m_clients.contains(strId));
	m_clients[strId] = QVariant::fromValue(context);
	// subscribe to destroyed
	m_connections <<
		QObject::connect(client, &QObject::destroyed, context,
			[this, client]() {
				this->removeClient(client);
			});
	// notify changes
	emit this->clientsChanged();
}

void QUaModbusQmlContext::removeClient(QUaModbusClient* client)
{
	QString strId = client->browseName().name();
	Q_ASSERT(m_clients.contains(strId));
	delete m_clients.take(strId).value<QUaModbusClientQmlContext*>();
	// notify changes
	emit this->clientsChanged();
}
