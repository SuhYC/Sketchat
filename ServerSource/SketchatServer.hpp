#pragma once

/*
* ������ �� �� �ִ� ����
* 0. ���θ޴�
* �κ� ���� -> �� ��� �� ���� ��������
* 
* 1. �κ�
* �� �����
* �� ����
*
* 2. ��
* �׸���
*
* ������ ó���ؾ��ϴ� ����
* �� ��� �� ���� ��ȭ ���� �ϱ�
* 
*/

#include "IOCP.hpp"
#include "UserManager.hpp"
#include "ReqHandler.hpp"
#include <Windows.h>
#include <functional>
#include <stdexcept>

class SketchatServer : IOCPServer
{
public:
	SketchatServer(const int nBindPort_, const unsigned short MaxClient_) : m_BindPort(nBindPort_), m_MaxClient(MaxClient_)
	{
		InitSocket(nBindPort_);

	}

	~SketchatServer()
	{

	}

	void Start()
	{
		auto SendMsgFunc = [this](unsigned short connectionIndex, PacketData* pPacket) -> bool {return SendMsg(connectionIndex, pPacket); };

		m_ReqHandler.SendMsgFunc = SendMsgFunc;

		m_ReqHandler.Init(m_MaxClient);

		StartServer(m_MaxClient);
	}

	void End()
	{
		EndServer();
	}

private:
	void OnConnect(const unsigned short index_, const uint32_t ip_) override
	{
		std::cout << "RPGServer::OnConnect : [" << index_ << "] Connected.\n";
		return;
	}

	void OnReceive(const unsigned short index_, char* pData_, const DWORD ioSize_) override
	{
		std::cout << "RPGServer::OnReceive : RecvMsg : size:" << ioSize_ << "\n";

		StoreMsg(index_, pData_, ioSize_);

		char buf[MAX_SOCKBUF];

		while (true)
		{

			//std::string req = GetMsg(index_);

			ZeroMemory(buf, MAX_SOCKBUF);
			unsigned int len = GetMsg(index_, buf);

			if (len == 0)
			{
				break;
			}

			std::string req(buf, len);

			std::cout << "SketchatServer::OnReceive : req size : " << len << ".\n";

			if (!m_ReqHandler.HandleReq(index_, req))
			{
				std::cerr << "RPGServer::OnReceive : Failed to HandleReq\n";
				return;
			}
			else
			{
				//std::cout << "RPGServer::OnReceive : " << req << '\n';
			}
		}

		return;
	}

	void OnDisconnect(const unsigned short index_) override
	{
		std::cout << "RPGServer::OnDisconnect : [" << index_ << "] disconnected.\n";

		m_ReqHandler.HandleDisconnect(index_);

		return;
	}

	ReqHandler m_ReqHandler;
	const int m_BindPort;
	const unsigned short m_MaxClient;
};