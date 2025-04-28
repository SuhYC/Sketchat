#pragma once

#include "Connection.hpp"
#include <sqlext.h>
#include <mutex>

class IOCPServer
{
public:
	IOCPServer()
	{
	}

	virtual ~IOCPServer()
	{
		WSACleanup();
	}

	bool InitSocket(const int nBindPort_)
	{
		auto result = CreateListenSocket(nBindPort_);

		if (result == false)
		{
			std::cerr << "socket 할당 실패\n";
			return false;
		}

		result = CreateIOCPHandle();

		if (result == false)
		{
			std::cerr << "IOCP Handle 할당 실패\n";
			closesocket(m_ListenSocket);
			return false;
		}

		return true;
	}

	void StartServer(const unsigned short maxClientCount_)
	{
		m_IsRunWorkThread = true;

		CreateThreads();
		CreateConnections(maxClientCount_);

		return;
	}

	bool SendMsg(unsigned short connectionIndex_, PacketData* pPacket_)
	{
		if (pPacket_ == nullptr)
		{
			return false;
		}

		Connection* pConnection = GetConnection(connectionIndex_);

		if (pConnection == nullptr)
		{
			return false;
		}

		pConnection->SendMsg(pPacket_);

		return true;
	}

	void EndServer()
	{
		CloseHandle(m_IOCPHandle);

		DestroyThreads();
		DestroyConnections();

		closesocket(m_ListenSocket);

		return;
	}

	bool StoreMsg(const int connectionIndex_, char* msg_, int size_)
	{
		Connection* conn = GetConnection(connectionIndex_);
		return conn->StorePartialMessage(msg_, size_);
	}

	unsigned int GetMsg(const int connectionIndex_, char* buf)
	{
		Connection* conn = GetConnection(connectionIndex_);
		return conn->GetReqMessage(buf);
	}

private:
	bool CreateListenSocket(int nBindPort_)
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0)
		{
			std::cerr << "WSAStartup 실패\n";
			return false;
		}

		m_ListenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_ListenSocket == INVALID_SOCKET)
		{
			std::cerr << "Socket() 실패\n" << WSAGetLastError() << '\n';
			return false;
		}

		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;

		stServerAddr.sin_port = htons(nBindPort_);
		stServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

		nRet = bind(m_ListenSocket, reinterpret_cast<SOCKADDR*>(&stServerAddr), sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			std::cerr << "Socket Bind 실패\n";
			return false;
		}

		nRet = listen(m_ListenSocket, 5);
		if (nRet != 0)
		{
			std::cerr << "Listen 실패\n";
			return false;
		}

		return true;
	}

	bool CreateIOCPHandle()
	{
		m_IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKTHREAD);

		if (m_IOCPHandle == INVALID_HANDLE_VALUE)
		{
			std::cerr << "IOCP 핸들 발급 실패\n";
			return false;
		}

		auto hIOCPHandle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_ListenSocket), m_IOCPHandle, static_cast<UINT32>(0), 0);

		if (hIOCPHandle == INVALID_HANDLE_VALUE)
		{
			std::cerr << "IOCP 핸들 링크 실패\n";
			return false;
		}

		return true;
	}

	void CreateThreads()
	{
		for (int i = 0; i < MAX_WORKTHREAD; i++)
		{
			WorkThreads.emplace_back([this]() { WorkThread(); });
		}

		return;
	}

	void DestroyThreads()
	{
		m_IsRunWorkThread = false;

		for (int i = 0; i < MAX_WORKTHREAD; i++)
		{
			WorkThreads[i].join();
		}

		return;
	}

	void CreateConnections(const unsigned short maxClientCount_)
	{
		for (int i = 0; i < maxClientCount_; i++)
		{

			Connections.push_back(new Connection(m_ListenSocket, i));
		}

		return;
	}

	void DestroyConnections()
	{
		for (int i = 0; i < Connections.size(); i++)
		{
			delete Connections[i];
		}

		return;
	}

	Connection* GetConnection(const int connectionIndex_) { return Connections[connectionIndex_]; }

	void WorkThread()
	{
		DWORD ioSize = 0;
		stOverlappedEx* pOverlapped = nullptr;
		Connection* pConnection = nullptr;

		while (m_IsRunWorkThread)
		{
			auto result = GetQueuedCompletionStatus(
				m_IOCPHandle,
				&ioSize,
				reinterpret_cast<PULONG_PTR>(&pConnection),
				reinterpret_cast<LPOVERLAPPED*>(&pOverlapped),
				INFINITE
			);

			if (pOverlapped == nullptr)
			{
				if (WSAGetLastError() != 0 && WSAGetLastError() != WSA_IO_PENDING)
				{
					//std::cerr << "WSAError : " << WSAGetLastError() << "\n";
				}
				continue;
			}

			if (!result || (ioSize == 0 && pOverlapped->m_eOperation != eIOOperation::ACCEPT))
			{
				HandleException(pConnection, pOverlapped);
				OnDisconnect(pConnection->GetIndex());
				continue;
			}

			switch (pOverlapped->m_eOperation)
			{
			case eIOOperation::ACCEPT:
				DoAccept(pOverlapped);
				break;
			case eIOOperation::RECV:
				DoRecv(pOverlapped, ioSize);
				break;
			case eIOOperation::SEND:
				DoSend(pOverlapped, ioSize);
			}
		}

		return;
	}

	void HandleException(Connection* pConnection_, const stOverlappedEx* pOverlapped_)
	{
		if (pOverlapped_ == nullptr)
		{
			return;
		}

		pConnection_->Close();

		return;
	}

	void DoAccept(const stOverlappedEx* pOverlapped_)
	{
		Connection* pConnection = GetConnection(pOverlapped_->m_userIndex);
		auto result = pConnection->BindIOCP(m_IOCPHandle);

		if (result == false)
		{
			pConnection->Close();
			pConnection->ResetConnection();
			return;
		}

		result = pConnection->BindRecv();

		if (result == false)
		{
			pConnection->Close();
			pConnection->ResetConnection();
			return;
		}

		uint32_t ip;
		result = pConnection->GetIP(ip);

		if (result == false)
		{
			std::cerr << "IOCP::DoAccept : GetIP Failed\n";
		}

		OnConnect(pOverlapped_->m_userIndex, ip);

		return;
	}

	void DoRecv(const stOverlappedEx* pOverlapped_, const DWORD ioSize_)
	{
		Connection* pConnection = GetConnection(pOverlapped_->m_userIndex);
		if (pConnection == nullptr)
		{
			return;
		}

		pConnection->RecvBuffer()[ioSize_] = NULL;

		OnReceive(pOverlapped_->m_userIndex, pConnection->RecvBuffer(), ioSize_);

		auto result = pConnection->BindRecv();
		if (result == false)
		{
			pConnection->Close();
			pConnection->ResetConnection();
			OnDisconnect(pOverlapped_->m_userIndex);
			return;
		}

		return;
	}

	void DoSend(const stOverlappedEx* pOverlapped_, const DWORD ioSize_)
	{
		Connection* pConnection = GetConnection(pOverlapped_->m_userIndex);
		if (pConnection == nullptr)
		{
			return;
		}

		pConnection->SendCompleted();
		return;
	}

	virtual void OnConnect(const unsigned short clientIndex_, const uint32_t ip_) = 0;
	virtual void OnReceive(const unsigned short clientIndex_, char* pData_, const DWORD dataSize_) = 0;
	virtual void OnDisconnect(const unsigned short clientIndex) = 0;

	bool m_IsRunWorkThread;

	SOCKET m_ListenSocket;
	HANDLE m_IOCPHandle;
	std::vector<Connection*> Connections;
	std::vector<std::thread> WorkThreads;
};