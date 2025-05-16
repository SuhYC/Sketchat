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
* 각 동작으로 연결하는 건 hashmap을 이용해 연결한다.
*
* 각 동작을 핸들하는 함수에서 결과 송신까지 함께하기로 결정.
*
* BuyItem은 일단 맵마다 NPC를 배치하고
* 1. 플레이어가 위치한 맵에 NPC가 있는지 확인하고 -> 해당 정보 DB에 추가 필요 [mapcode] [npccode]
* 2. NPC가 파는 물품에 해당 아이템이 있는지 확인하고 -> 해당 정보 DB에 추가 필요 [npccode] [itemcode]
* 3. 해당 아이템의 가격과 비교하여 구매할 수 있는지 (일단 DB를 통해 가격을 확인하고 Redis에 수정요청하는 부분은 만들어두었다.)
* 확인하는 방식으로 하면 좋을 것 같다.
* 아직 더 고민해보고 결정.
*
* MoveMap은 DB에 맵간 연결정보를 기록하고 -> DB에 추가 필요 [mapcode] [mapcode]
* 이를 서버가 시작할때 받아와 DB가 가지고 있는 것으로 한다. (MapEdge)
* 이동가능여부를 DB로부터 확인받고 이행한다.
*
* todo. DB에 npc-item 정보와 -> npc가 해당 아이템을 판매한다는 정보
* map-npc 정보와 -> 해당 맵에 npc가 존재한다는 정보
* item-price정보와 -> 해당 아이템의 기본 상점 구매가 정보
* map-map정보를 기록하여야함. -> 해당 맵에서 다음 맵으로 이동할 수 있다는 정보
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

	// 수행 결과로 의심스러운 동작이 검출된다면 따로 기록하는것도 좋겠다.
	bool HandleReq(const unsigned short userindex_, std::string& req_)
	{
		ReqMessage msg;

		if (!m_Serializer.Deserialize(req_, msg))
		{
			std::cout << "ReqHandler::HandleReq : Deserialize(ReqMessage) 실패\n";
			// ReqMessage포맷조차 안맞췄다고..?
			return false;
		}

		auto itr = Actions.find(msg.reqType);

		if (itr == Actions.end())
		{
			std::cerr << "ReqHandler::HandleReq : Not Defined ReqType.\n";
			return false;
		}

		std::string strPayLoad(msg.payLoad, msg.payLoadSize);

		// 함수포인터를 통한 사용
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
		
		// 빈 방 없음
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

		// 방에 있는 모든 유저의 정보 전달
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

		// 방에 있는 모든 유저 정보 전달
		pRoom->NotifyAllExistUser(userindex_);
		// 캔버스 정보 전달
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

		// 해당 맵에 전파
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
			// 송신 실패
			m_PacketPool->Deallocate(packet);
			return InfoType::REQ_FAILED;
		}

		// 송신 성공
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
			// 송신 실패
			std::cerr << "ReqHandler::SendResultMsg : Failed to Send\n";
			m_PacketPool->Deallocate(packet);
			return InfoType::REQ_FAILED;
		}

		m_PacketPool->Deallocate(packet);
		// 송신 성공
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
			// 송신 실패
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