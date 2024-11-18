#include "pch.h"
#include "NetworkDiscovery.h"
#include "NetworkConstants.h"

NetworkDiscovery::NetworkDiscovery(const std::string& localServerName)
	: _localServerName(localServerName)
{

}

NetworkDiscovery::~NetworkDiscovery()
{
	Term();
}

bool NetworkDiscovery::Init()
{
	// � compl�ter :
	// 1. Essayer de lier le socket au port NetworkPort
	sf::Socket::Status status;
	status = _socket.bind(NetworkPort);
	uint16_t port = NetworkPort+1;
	// 2. Si �chec, essayer les ports suivants jusqu'� succ�s

	while(status != sf::Socket::Done || port >= 60000)
	{	
		status = _socket.bind(port);
		port++;
	}

	// 3. Ajouter le socket au s�lecteur
	if (status == sf::Socket::Done) 
	{
		return true;
	}

	return false;
}

void NetworkDiscovery::Term()
{
	_socketSelector.remove(_socket);
	_socket.unbind();
}

void NetworkDiscovery::StartBroadcast()
{
	_isBroadcastEnabled = true;
	_lastDeclareGameServerTimeMs = 0;
}

void NetworkDiscovery::StopBroadcast()
{
	_isBroadcastEnabled = false;
}

void NetworkDiscovery::Update()
{
	uint64_t nowMs = GetTimeMs();

	if(_isBroadcastEnabled)
	{
		// � compl�ter :
		// 1. V�rifier si l'�cart de temps entre maintenant et la derni�re d�claration de temps est sup�rieure ou �gale � DeclareGameServerDelayMs
		uint64_t Ecart;
		if (_lastDeclareGameServerTimeMs != nowMs )
		{
			Ecart = nowMs - _lastDeclareGameServerTimeMs;
		}

		if (Ecart >= DeclareGameServerDelayMs )
		{
			
		// 2. Cr�er un paquet avec MagicPacket et _localServerName
			sf::Packet packet;
    		packet << MagicPacket << _localServerName;
			_socket.send(packet, sf::IpAddress::Broadcast, NetworkPort);
		}

		// 3. Envoyer le paquet en broadcast
	}

	while(_socketSelector.wait(sf::microseconds(1)) && _socketSelector.isReady(_socket))
	{
		sf::Packet packet;
		sf::IpAddress senderIp;
		uint16_t senderPort;

		if(_socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done)
		{
			uint32_t magicValue = 0;
			std::string serverName;

			if((packet >> magicValue >> serverName) &&
				magicValue == MagicPacket &&
				serverName.size() > 0)
			{
				AddServerOrUpdate(senderIp, serverName);
			}
		}
	}

	RemoveExpiredServers();
}

const std::vector<GameInfo>& NetworkDiscovery::GetDiscoveredServers() const
{
	return _discoveredServers;
}

NetworkDiscovery::EventDiscoverServer& NetworkDiscovery::GetEventDiscoverServer()
{
	return _eventDiscoverServer;
}

NetworkDiscovery::EventUndiscoverServer& NetworkDiscovery::GetEventUndiscoverServer()
{
	return _eventUndiscoverServer;
}

void NetworkDiscovery::AddServerOrUpdate(const sf::IpAddress& ipAddress, const std::string& serverName)
{
	bool isNew = false;
	GameInfo* pGameInfo = nullptr;

	for(GameInfo& gameInfo : _discoveredServers)
	{
		if(gameInfo.serverIP == ipAddress)
		{
			pGameInfo = &gameInfo;
			break;
		}
	}

	if(pGameInfo == nullptr)
	{
		pGameInfo = &_discoveredServers.emplace_back();
		pGameInfo->serverIP = ipAddress;
		isNew = true;
	}

	pGameInfo->lastTimeReceivedMs = GetTimeMs();
	pGameInfo->serverName = serverName;

	if(isNew)
	{
		std::cout << "A new server discovered (" << ipAddress.toString() << ", " << serverName << ")" << std::endl;

		_eventDiscoverServer.Invoke(*pGameInfo);
	}
}

void NetworkDiscovery::RemoveExpiredServers()
{
	uint64_t nowMs = GetTimeMs();

	for(std::vector<GameInfo>::iterator it = _discoveredServers.begin(); it != _discoveredServers.end();)
	{
		GameInfo& gameInfo = *it;

		if(nowMs - gameInfo.lastTimeReceivedMs >= DiscoveredGameServerTimeoutMs)
		{
			std::cout << "A discovered server timed out (" << gameInfo.serverIP.toString() << ", " << gameInfo.serverName << ")" << std::endl;

			_eventUndiscoverServer.Invoke(gameInfo);
			it = _discoveredServers.erase(it);
		}
		else
		{
			++it;
		}
	}
}
