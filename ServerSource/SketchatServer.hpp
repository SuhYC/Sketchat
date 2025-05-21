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
#include <Windows.h>
#include <functional>
#include <stdexcept>
#include "Job.hpp"
#include "concurrent_queue.h"
#include <thread>
#include <vector>
#include <optional>
#include <string_view>

class SketchatServer : IOCPServer
{
public:
	SketchatServer(const int nBindPort_, const unsigned short MaxClient_) : m_BindPort(nBindPort_), m_MaxClient(MaxClient_), m_IsRunJobThread(false)
	{
		InitSocket(nBindPort_);

	}

	~SketchatServer()
	{

	}

	void Start()
	{
		m_PacketPool = std::make_unique<NetworkPacket::PacketPool>();

		m_IsRunJobThread = true;

		CreateThreads();

		auto SendMsgFunc = [this](unsigned short connectionIndex, uint32_t reqNo,
			InfoType resCode, const std::optional<std::string_view> optionalMsg)
			-> InfoType {return SendResultMsg(connectionIndex, reqNo, resCode, optionalMsg); };

		Job::SendMsgFunc = SendMsgFunc;

		m_UserManager.Init(m_MaxClient);

		auto SendInfo =
			[this](const uint16_t userindex_,
				InfoType rescode_,
				const std::string& msg_)
			-> bool { return SendInfoMsg(userindex_, rescode_, msg_); };

		auto SendInfoToUsers =
			[this](const std::map<uint16_t, User*> users_,
				InfoType rescode_,
				const std::string& msg_)
			{
				SendInfoMsgToUsers(users_, rescode_, msg_);
			};

		m_RoomManager.SendInfoFunc = SendInfo;
		m_RoomManager.SendInfoToUsersFunc = SendInfoToUsers;
		m_RoomManager.Init();

		m_JobFactory.Init(&m_UserManager, &m_RoomManager);

		//m_ReqHandler.Init(m_MaxClient);

		StartServer(m_MaxClient);
	}

	void End()
	{
		EndServer();
		DestroyThreads();
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
			unsigned int len = GetMsg(index_, buf);

			if (len == 0)
			{
				break;
			}

			std::string req(buf, len);

			std::cout << "SketchatServer::OnReceive : req size : " << len << ".\n";

			Job* pJob = m_JobFactory.CreateJob(index_, req);

			if (pJob == nullptr)
			{
				std::cerr << "RPGServer::OnReceive : Failed to Make Job\n";
				return;
			}

			JobQueue.push(pJob);

			//if (!m_ReqHandler.HandleReq(index_, req))
			//{
			//	std::cerr << "RPGServer::OnReceive : Failed to HandleReq\n";
			//	return;
			//}
			//else
			//{
			//	//std::cout << "RPGServer::OnReceive : " << req << '\n';
			//}
		}
		return;
	}

	void OnDisconnect(const unsigned short index_) override
	{
		std::cout << "RPGServer::OnDisconnect : [" << index_ << "] disconnected.\n";

		// Disconnect �� �ϳ��� Job���� �����ϱ�.
		
		Job* pJob = m_JobFactory.CreateExitRoomJob(index_, 0);

		JobQueue.push(pJob);

		//m_ReqHandler.HandleDisconnect(index_);

		return;
	}

	void CreateThreads()
	{
		for (int i = 0; i < MAX_JOBTHREAD; i++)
		{
			JobThreads.emplace_back([this]() {JobThread(); });
		}
	}

	void DestroyThreads()
	{
		m_IsRunJobThread = false;

		for (int i = 0; i < MAX_JOBTHREAD; i++)
		{
			JobThreads[i].join();
		}

		Job* pJob = nullptr;

		while (!JobQueue.empty())
		{
			if (JobQueue.try_pop(pJob) && pJob != nullptr)
			{
				delete pJob;
			}
		}
	}

	void JobThread()
	{
		while (m_IsRunJobThread)
		{
			Job* pJob = nullptr;
			if (JobQueue.try_pop(pJob))
			{
				InfoType eRet = pJob->Execute();

				// ó�� �Ϸ�. ���е� �����̵� Job�� ������.
				if (eRet == InfoType::REQ_SUCCESS || eRet == InfoType::REQ_FAILED)
				{
					delete pJob;
				}
				// ������ �뷮 ������ ��� �� �̾ �ؾ��ϴ� ��� ��.
				else if (eRet == InfoType::NOT_FINISHED)
				{
					JobQueue.push(pJob); // Re Queueing.
				}
				// REQ_FAILED, REQ_SUCCESS, NOT_FINISHED �̿��� InfoType�� ��ȯ�Ǹ� �ȵȴ�.
				// ������ InfoType�� ��û�� ���� ������ �ƴ� ���������̱� ����.
				else
				{
					std::cerr << "JobThread : Undefined Behavior[" << static_cast<int>(eRet) << "] occurred\n";
					delete pJob;
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
			}
		}

	}

	InfoType SendResultMsg(const unsigned short userIndex_, const unsigned int ReqNo_,
		InfoType resCode_, std::optional<std::string_view> optionalMsg_ = std::nullopt)
	{
		ResMessage stResultMsg;

		stResultMsg.reqNo = ReqNo_;
		stResultMsg.resCode = static_cast<int32_t>(resCode_);

		if (optionalMsg_.has_value())
		{
			stResultMsg.payLoadSize = optionalMsg_.value().size();
			CopyMemory(stResultMsg.payLoad, optionalMsg_.value().data(), stResultMsg.payLoadSize);
		}
		else
		{
			stResultMsg.payLoadSize = 0;
		}

		std::string msg;
		Serializer serializer;

		if (!serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "ReqHandler::SendResultMsg : Failed to Make MsgStream\n";
			return InfoType::REQ_FAILED;
		}

		PacketData* packet = m_PacketPool->Allocate();

		packet->Init(msg);

		if (!SendMsg(userIndex_, packet))
		{
			// �۽� ����
			m_PacketPool->Deallocate(packet);
			return InfoType::REQ_FAILED;
		}

		// �۽� ����
		m_PacketPool->Deallocate(packet);
		return resCode_;
	}

	bool SendInfoMsg(const unsigned short userindex_, InfoType rescode_, const std::string& msg_)
	{
		ResMessage stResultMsg;

		stResultMsg.reqNo = 0;
		stResultMsg.resCode = static_cast<int32_t>(rescode_);
		stResultMsg.payLoadSize = msg_.length();

		if (stResultMsg.payLoadSize > MAX_PAYLOAD_SIZE)
		{
			std::cerr << "SketchatServer::SendInfoMsg : Too Long Msg.\n";
			std::cerr << "Msg Size : " << stResultMsg.payLoadSize << "bytes.\n";
			return false;
		}

		if (!msg_.empty())
		{
			CopyMemory(stResultMsg.payLoad, msg_.c_str(), msg_.length());
		}

		std::string msg;
		Serializer serializer;
		if (!serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "SketchatServer::SendInfoMsg : Failed to Make Jsonstr\n";
			return false;
		}

		PacketData* packet = m_PacketPool->Allocate();
		if (packet == nullptr)
		{
			std::cerr << "SketchatServer::SendInfoMsg : Failed to Allocate Packet\n";
			return false;
		}

		packet->Init(msg);

		if (!SendMsg(userindex_, packet))
		{
			// �۽� ����
			m_PacketPool->Deallocate(packet);
			std::cout << "SketchatServer::SendInfoMsg : Need To Requeue.\n";
			return false;
		}

		m_PacketPool->Deallocate(packet);
		return true;
	}

	void SendInfoMsgToUsers(const std::map<unsigned short, User*>& users, InfoType rescode_, const std::string& msg_)
	{
		if (users.size() == 0)
		{
			return;
		}

		ResMessage stResultMsg;

		stResultMsg.reqNo = 0;
		stResultMsg.resCode = static_cast<int32_t>(rescode_);
		stResultMsg.payLoadSize = msg_.length();

		if (!msg_.empty())
		{
			CopyMemory(stResultMsg.payLoad, msg_.c_str(), msg_.length());
		}

		std::string msg;
		Serializer serializer;
		if (!serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "ReqHandler::SendInfoMsgToUsers : Failed to Make Jsonstr\n";
			return;
		}

		PacketData* packet = m_PacketPool->Allocate();
		if (packet == nullptr)
		{
			std::cerr << "ReqHandler::SendInfoMsgToUsers : Failed to Allocate Packet\n";
			return;
		}

		packet->Init(msg);

		for (auto itr = users.begin(); itr != users.end(); itr++)
		{
			if (!SendMsg(itr->first, packet))
			{
				std::cerr << "ReqHandler::SendInfoMsgToUsers : Failed to Send Msg\n";
				m_PacketPool->Deallocate(packet);
				return;
			}
		}

		m_PacketPool->Deallocate(packet);

		return;
	}

	UserManager m_UserManager;
	RoomManager m_RoomManager;

	std::unique_ptr<NetworkPacket::PacketPool> m_PacketPool;

	Concurrency::concurrent_queue<Job*> JobQueue;
	
	JobFactory m_JobFactory;

	const int m_BindPort;
	const unsigned short m_MaxClient;

	bool m_IsRunJobThread;
	std::vector<std::thread> JobThreads;
};