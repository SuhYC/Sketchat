#pragma once
#include <string>
#include <unordered_map>
#include "UserManager.hpp"
#include "PacketPool.hpp"
#include "Serialization.hpp"
#include "RoomManager.hpp"

/*
* Json -> param -> Operation
*
* �� �������� �����ϴ� �� hashmap�� �̿��� �����Ѵ�.
*
* �� ������ �ڵ��ϴ� �Լ����� ��� �۽ű��� �Բ��ϱ�� ����.
*
* BuyItem�� �ϴ� �ʸ��� NPC�� ��ġ�ϰ�
* 1. �÷��̾ ��ġ�� �ʿ� NPC�� �ִ��� Ȯ���ϰ� -> �ش� ���� DB�� �߰� �ʿ� [mapcode] [npccode]
* 2. NPC�� �Ĵ� ��ǰ�� �ش� �������� �ִ��� Ȯ���ϰ� -> �ش� ���� DB�� �߰� �ʿ� [npccode] [itemcode]
* 3. �ش� �������� ���ݰ� ���Ͽ� ������ �� �ִ��� (�ϴ� DB�� ���� ������ Ȯ���ϰ� Redis�� ������û�ϴ� �κ��� �����ξ���.)
* Ȯ���ϴ� ������� �ϸ� ���� �� ����.
* ���� �� ����غ��� ����.
*
* MoveMap�� DB�� �ʰ� ���������� ����ϰ� -> DB�� �߰� �ʿ� [mapcode] [mapcode]
* �̸� ������ �����Ҷ� �޾ƿ� DB�� ������ �ִ� ������ �Ѵ�. (MapEdge)
* �̵����ɿ��θ� DB�κ��� Ȯ�ιް� �����Ѵ�.
*
* todo. DB�� npc-item ������ -> npc�� �ش� �������� �Ǹ��Ѵٴ� ����
* map-npc ������ -> �ش� �ʿ� npc�� �����Ѵٴ� ����
* item-price������ -> �ش� �������� �⺻ ���� ���Ű� ����
* map-map������ ����Ͽ�����. -> �ش� �ʿ��� ���� ������ �̵��� �� �ִٴ� ����
*/


class ReqHandler
{
public:
	ReqHandler()
	{
		Actions = std::unordered_map<ReqType, REQ_HANDLE_FUNC>();
		m_PacketPool = std::make_unique<NetworkPacket::PacketPool>();

		Actions[ReqType::CREATE_ROOM] = &ReqHandler::HandleCreateRoom;
		Actions[ReqType::ENTER_ROOM] = &ReqHandler::HandleEnterRoom;
		Actions[ReqType::SET_NICKNAME] = &ReqHandler::HandleSetNickname;

		Actions[ReqType::DRAW_START] = &ReqHandler::HandleDrawStart;
		Actions[ReqType::DRAW] = &ReqHandler::HandleDraw;
		Actions[ReqType::DRAW_END] = &ReqHandler::HandleDrawEnd;
		Actions[ReqType::UNDO] = &ReqHandler::HandleUndo;
		Actions[ReqType::CHAT] = &ReqHandler::HandleChat;
		Actions[ReqType::REQ_ROOM_INFO] = &ReqHandler::HandleReqRoomInfo;
	}

	void Init(const unsigned short MaxClient_)
	{
		m_UserManager.Init(MaxClient_);
		
		auto SendInfo =
			[this](const unsigned short userindex_,
				InfoType rescode_,
				const std::string& msg_)
			{SendInfoMsg(userindex_, rescode_, msg_); };

		auto SendInfoToUsers =
			[this](const std::map<unsigned short, User*>& users,
				InfoType rescode_,
				const std::string& msg_)
			{SendInfoMsgToUsers(users, rescode_, msg_); };

		m_RoomManager.SendInfoFunc = SendInfo;
		m_RoomManager.SendInfoToUsersFunc = SendInfoToUsers;

		m_RoomManager.Init();
	}

	~ReqHandler()
	{
		m_PacketPool.reset();
		m_UserManager.Clear();
	}

	// ���� ����� �ǽɽ����� ������ ����ȴٸ� ���� ����ϴ°͵� ���ڴ�.
	bool HandleReq(const unsigned short userindex_, std::string& req_)
	{
		ReqMessage msg;

		if (!m_Serializer.Deserialize(req_, msg))
		{
			std::cout << "ReqHandler::HandleReq : Deserialize(ReqMessage) ����\n";
			// ReqMessage�������� �ȸ���ٰ�..?
			return false;
		}

		auto itr = Actions.find(msg.reqType);

		if (itr == Actions.end())
		{
			std::cerr << "ReqHandler::HandleReq : Not Defined ReqType.\n";
			return false;
		}

		std::string strPayLoad(msg.payLoad, msg.payLoadSize);

		// �Լ������͸� ���� ���
		InfoType eRet = (this->*(itr->second))(userindex_, msg.reqNo, strPayLoad);

		return true;
	}

	void HandleDisconnect(const unsigned short userindex_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);

		if (pUser == nullptr)
		{
			return;
		}

		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom != nullptr)
		{
			if (pUser->GetState() == 1) // locked read lock. need to unlock
			{
				pRoom->Read_Unlock();
			}

			pRoom->Exit(userindex_);
		}

		pUser->Init();

		return;
	}

	void SetIP(const unsigned short connidx_, const uint32_t ip_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(connidx_);
		pUser->SetIP(ip_);

		return;
	}

	std::function<bool(unsigned short, PacketData*)> SendMsgFunc;

private:
	InfoType HandleSetNickname(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		SetNicknameParameter stParam;
		if (!m_Serializer.Deserialize(param_, stParam))
		{
			std::cout << "ReqHandler::HandleSetNickname : invalid req\n";

			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		std::string name(stParam.nickname, stParam.nameLen);

		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		
		pUser->SetName(name);

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType HandleCreateRoom(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		if (pUser->GetRoomIdx() == -1)
		{
			std::cout << "ReqHandler::HandleCreateRoom : invalid req\n";

			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		CreateRoomParameter stParam;
		if (!m_Serializer.Deserialize(param_, stParam))
		{
			std::cerr << "ReqHandler::HandleCreateRoom : Failed to Deserialize.\n";
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_RoomManager.GetEmptyRoom(userindex_, pUser->GetName());
		
		// �� �� ����
		if (pRoom == nullptr)
		{
			std::cerr << "ReqHandler::HandleCreateRoom : Failed to Create Room.\n";
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		std::string roomName(stParam.roomName, stParam.nameLen);
		std::string pw(stParam.password, stParam.pwLen);

		pRoom->Init(roomName, pw, stParam.maxUsers);

		CreateRoomRes stRes{ pRoom->GetIdx() };
		stRes.roomNameLen = stParam.nameLen;
		stRes.pwLen = stParam.pwLen;
		CopyMemory(stRes.roomName, stParam.roomName, stParam.nameLen);
		CopyMemory(stRes.password, stParam.password, stParam.pwLen);

		std::string msg;
		
		if (!m_Serializer.Serialize(stRes, msg))
		{
			std::cerr << "ReqHandler::HandleCreateRoom : Failed to Serialize.\n";
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		pUser->SetRoomIdx(pRoom->GetIdx());
		bool bRet = pRoom->Enter(userindex_, pUser, pUser->GetName(), pw);

		if (!bRet)
		{
			std::cerr << "ReqHandler::HandleCreateRoom : Failed to Enter Room\n";
		}

		InfoType eRet = SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS, msg);

		// �濡 �ִ� ��� ������ ���� ����
		pRoom->NotifyAllExistUser(userindex_);

		return eRet;
	}

	InfoType HandleEnterRoom(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom != nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		EnterRoomParameter stParam;
		if (!m_Serializer.Deserialize(param_, stParam))
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		pRoom = m_RoomManager.GetRoomByIndex(stParam.roomNumber);

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		bool bRet = pRoom->Enter(userindex_, pUser, pUser->GetName(), std::string(stParam.password, stParam.pwLen));

		if (!bRet)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}
		pUser->SetRoomIdx(stParam.roomNumber);
		
		EnterRoomRes stRes;

		CopyMemory(stRes.password, stParam.password, stParam.pwLen);
		CopyMemory(stRes.roomName, pRoom->GetName().c_str(), pRoom->GetName().length());
		stRes.pwLen = stParam.pwLen;
		stRes.roomNameLen = pRoom->GetName().length();

		std::string msg;
		if (!m_Serializer.Serialize(stRes, msg))
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}
		
		InfoType eRet = SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS, msg);

		// �濡 �ִ� ��� ���� ���� ����
		pRoom->NotifyAllExistUser(userindex_);
		// ĵ���� ���� ����
		pRoom->NotifyCanvasInfo(userindex_);

		return eRet;
	}

	InfoType HandleReqRoomInfo(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		std::string msg;
		if (!m_RoomManager.GetRoomInfo(msg))
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS, msg);
	}

	InfoType HandleDrawStart(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		if (pUser->GetState() == 1)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		bool bRet = pRoom->DrawStart();

		if (!bRet)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		pUser->SetDrawing();

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType HandleDraw(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		if (pUser->GetState() != 1)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		DrawParameter stParam;
		if (!m_Serializer.Deserialize(param_, stParam))
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		pRoom->Draw(userindex_, stParam.drawNum, stParam.drawPosX, stParam.drawPosY, stParam.width, stParam.r, stParam.g, stParam.b);

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType HandleDrawEnd(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		DrawEndParameter stParam;
		if (!m_Serializer.Deserialize(param_, stParam))
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		pRoom->DrawEnd(userindex_, stParam.drawNum);
		pUser->SetIdle();

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType HandleUndo(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);
		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		uint32_t iRet = pRoom->Undo();

		if (iRet == 0)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType HandleChat(const unsigned short userindex_, const unsigned int ReqNo, const std::string& param_)
	{
		if (param_.length() > MAX_CHATTING_LEN)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		User* pUser = m_UserManager.GetUserByConnIndex(userindex_);

		Room* pRoom = m_RoomManager.GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(userindex_, ReqNo, InfoType::REQ_FAILED);
		}

		// �ش� �ʿ� ����
		pRoom->NotifyChatToAll(userindex_, param_);

		return SendResultMsg(userindex_, ReqNo, InfoType::REQ_SUCCESS);
	}

	InfoType SendResultMsg(const unsigned short userIndex_, const unsigned int ReqNo_,
		InfoType resCode_, std::string& optionalMsg_)
	{
		ResMessage stResultMsg;

		stResultMsg.reqNo = ReqNo_;
		stResultMsg.resCode = static_cast<int32_t>(resCode_);
		stResultMsg.payLoadSize = optionalMsg_.length();
		
		if (stResultMsg.payLoadSize != 0)
		{
			CopyMemory(stResultMsg.payLoad, optionalMsg_.c_str(), stResultMsg.payLoadSize);
		}

		std::string msg;
		if (!m_Serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "ReqHandler::SendResultMsg : Failed to Make MsgStream\n";
			return InfoType::REQ_FAILED;
		}

		PacketData* packet = m_PacketPool->Allocate();

		packet->Init(msg);

		if (!SendMsgFunc(userIndex_, packet))
		{
			// �۽� ����
			m_PacketPool->Deallocate(packet);
			return InfoType::REQ_FAILED;
		}

		// �۽� ����
		m_PacketPool->Deallocate(packet);
		return resCode_;
	}

	InfoType SendResultMsg(const unsigned short userIndex_, const unsigned int ReqNo_,
		InfoType resCode_, const std::string&& optionalMsg_ = std::string())
	{
		ResMessage stResultMsg;

		stResultMsg.reqNo = ReqNo_;
		stResultMsg.resCode = static_cast<int32_t>(resCode_);
		stResultMsg.payLoadSize = optionalMsg_.length();

		if (!optionalMsg_.empty())
		{
			CopyMemory(stResultMsg.payLoad, optionalMsg_.c_str(), stResultMsg.payLoadSize);
		}

		std::string msg;
		if (!m_Serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "ReqHandler::SendResultMsg : Failed to Make Jsonstr\n";
			return InfoType::REQ_FAILED;
		}

		PacketData* packet = m_PacketPool->Allocate();

		packet->Init(msg);

		if (!SendMsgFunc(userIndex_, packet))
		{
			// �۽� ����
			std::cerr << "ReqHandler::SendResultMsg : Failed to Send\n";
			m_PacketPool->Deallocate(packet);
			return InfoType::REQ_FAILED;
		}

		m_PacketPool->Deallocate(packet);
		// �۽� ����
		return resCode_;
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
		if (!m_Serializer.Serialize(stResultMsg, msg))
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
			if (!SendMsgFunc(itr->first, packet))
			{
				std::cerr << "ReqHandler::SendInfoMsgToUsers : Failed to Send Msg\n";
				m_PacketPool->Deallocate(packet);
				return;
			}
		}

		m_PacketPool->Deallocate(packet);

		return;
	}

	void SendInfoMsg(const unsigned short userindex_, InfoType rescode_, const std::string& msg_)
	{
		ResMessage stResultMsg;

		stResultMsg.reqNo = 0;
		stResultMsg.resCode = static_cast<int32_t>(rescode_);
		stResultMsg.payLoadSize = msg_.length();

		if (stResultMsg.payLoadSize > MAX_PAYLOAD_SIZE)
		{
			return;
		}

		if (!msg_.empty())
		{
			CopyMemory(stResultMsg.payLoad, msg_.c_str(), msg_.length());
		}

		std::string msg;
		if (!m_Serializer.Serialize(stResultMsg, msg))
		{
			std::cerr << "ReqHandler::SendInfoMsg : Failed to Make Jsonstr\n";
			return;
		}

		PacketData* packet = m_PacketPool->Allocate();
		if (packet == nullptr)
		{
			std::cerr << "ReqHandler::SendInfoMsg : Failed to Allocate Packet\n";
			return;
		}

		packet->Init(msg);

		if (!SendMsgFunc(userindex_, packet))
		{
			// �۽� ����
			m_PacketPool->Deallocate(packet);
			std::cerr << "ReqHandler::SendInfoMsg : Failed to Send Msg\n";
		}

		m_PacketPool->Deallocate(packet);
		return;
	}

	Serializer m_Serializer;
	UserManager m_UserManager;
	RoomManager m_RoomManager;

	unsigned short m_MaxClient;

	typedef InfoType(ReqHandler::* REQ_HANDLE_FUNC)(const unsigned short, const unsigned int, const std::string&);

	std::unordered_map<ReqType, REQ_HANDLE_FUNC> Actions;

	std::unique_ptr<NetworkPacket::PacketPool> m_PacketPool;
};