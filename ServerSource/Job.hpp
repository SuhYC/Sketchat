#pragma once

#include <vector>

#include "Serialization.hpp"
#include "NetworkMessage.hpp"

#include "PacketPool.hpp"
#include "PacketData.hpp"

#include "UserManager.hpp"
#include "RoomManager.hpp"
#include "User.hpp"
#include "Room.hpp"
#include <string_view>
#include <optional>


class Job
{
public:
	Job(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : m_userindex(userindex_), m_reqNo(reqNo_), m_userManager(um), m_roomManager(rm) {}
	virtual ~Job() = default;

	virtual InfoType Execute() = 0;
	virtual bool Parse(const std::string& param_) = 0;

	unsigned short m_userindex;
	unsigned int m_reqNo;

	static std::function<InfoType(uint16_t, uint32_t, InfoType, const std::optional<std::string_view>)> SendMsgFunc;
protected:
	UserManager* m_userManager = nullptr;
	RoomManager* m_roomManager = nullptr;

	InfoType SendResultMsg(const unsigned short userIndex_, const unsigned int ReqNo_,
		InfoType resCode_, const std::optional<std::string_view> optionalMsg_ = std::nullopt)
	{
		return SendMsgFunc(userIndex_, ReqNo_, resCode_, optionalMsg_);
	}
};

std::function<InfoType(uint16_t, uint32_t, InfoType, const std::optional<std::string_view>)> Job::SendMsgFunc;

class RoomCreationJob : public Job
{
public:
	RoomCreationJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}
	~RoomCreationJob() override {}

	bool Parse(const std::string& param_) override
	{
		Serializer serializer;

		if (!serializer.Deserialize(param_, m_param))
		{
			std::cerr << "RoomCreationJob::Parse : Failed to Parse\n";
			return false;
		}

		return true;
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr || pUser->GetRoomIdx() == -1)
		{
			std::cout << "RoomCreationJob::Execute : invalid req\n";

			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_roomManager->GetEmptyRoom(m_userindex, pUser->GetName());

		// 빈 방 없음
		if (pRoom == nullptr)
		{
			std::cerr << "RoomCreationJob::Execute : Failed to Create Room.\n";
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		std::string roomName(m_param.roomName, m_param.nameLen);
		std::string pw(m_param.password, m_param.pwLen);

		pRoom->Init(roomName, pw, m_param.maxUsers);

		CreateRoomRes stRes{ pRoom->GetIdx() };
		stRes.roomNameLen = m_param.nameLen;
		stRes.pwLen = m_param.pwLen;
		CopyMemory(stRes.roomName, m_param.roomName, m_param.nameLen);
		CopyMemory(stRes.password, m_param.password, m_param.pwLen);

		Serializer serializer;
		std::string msg;

		if (!serializer.Serialize(stRes, msg))
		{
			std::cerr << "RoomCreationJob::Execute : Failed to Serialize.\n";
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		pUser->SetRoomIdx(pRoom->GetIdx());
		bool bRet = pRoom->Enter(m_userindex, pUser, pUser->GetName(), pw);

		if (!bRet)
		{
			std::cerr << "RoomCreationJob::Execute : Failed to Enter Room\n";
		}

		InfoType eRet = SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS, msg);

		// 방에 있는 모든 유저의 정보 전달
		pRoom->NotifyAllExistUser(m_userindex);

		return eRet;
	}

private:
	CreateRoomParameter m_param;
};

class EnterRoomJob : public Job
{
public:
	EnterRoomJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

	bool Parse(const std::string& param_) override
	{
		Serializer serializer;

		if (!serializer.Deserialize(param_, m_param))
		{
			std::cerr << "EnterRoomJob::Parse : Failed to Parse\n";
			return false;
		}

		return true;
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr || pUser->GetRoomIdx() != -1)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(m_param.roomNumber);

		if (pRoom == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		bool bRet = pRoom->Enter(m_userindex, pUser, pUser->GetName(), std::string(m_param.password, m_param.pwLen));

		if (!bRet)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		pUser->SetRoomIdx(m_param.roomNumber);

		EnterRoomRes stRes;

		CopyMemory(stRes.password, m_param.password, m_param.pwLen);
		CopyMemory(stRes.roomName, pRoom->GetName().c_str(), pRoom->GetName().length());
		stRes.pwLen = m_param.pwLen;
		stRes.roomNameLen = pRoom->GetName().length();

		Serializer serializer;
		std::string msg;

		if (!serializer.Serialize(stRes, msg))
		{
			return InfoType::REQ_FAILED;
		}

		InfoType eRet = SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS, msg);

		// 방에 있는 모든 유저 정보 전달
		pRoom->NotifyAllExistUser(m_userindex);
		// 캔버스 정보 전달
		pRoom->NotifyCanvasInfo(m_userindex);

		return eRet;
	}

private:
	EnterRoomParameter m_param;
};

class ExitRoomJob : public Job
{
public:
	ExitRoomJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm) {}

	bool Parse(const std::string& param_) override
	{
		return true;
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr)
		{
			return InfoType::REQ_FAILED; // 응답패킷은 보내지 않아도 된다.
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom != nullptr)
		{
			if (pUser->GetState() == 1) // locked read lock. need to unlock
			{
				pRoom->Read_Unlock();
			}

			pRoom->Exit(m_userindex);
		}

		pUser->Init();

		return InfoType::REQ_SUCCESS; // 응답패킷은 보내지 않아도 된다.
	}
};

class SetNicknameJob : public Job
{
public:
	SetNicknameJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

	bool Parse(const std::string& param_) override
	{
		Serializer serializer;

		if (!serializer.Deserialize(param_, m_param))
		{
			std::cerr << "SetNicknameJob::Parse : Failed to Parse\n";
			return false;
		}

		return true;
	}

	InfoType Execute() override
	{
		std::string name(m_param.nickname, m_param.nameLen);

		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		pUser->SetName(name);

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}

private:
	SetNicknameParameter m_param;
};

class ReqRoomInfoJob : public Job
{
public:
	ReqRoomInfoJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm) {}

	bool Parse(const std::string& param_) override
	{
		return true; // 파라미터 따로 없음
	}

	InfoType Execute() override
	{
		std::string msg;
		if (!m_roomManager->GetRoomInfo(msg))
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS, msg);
	}
};

class DrawStartJob : public Job
{
public:
	DrawStartJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm) {}

	bool Parse(const std::string& param_) override
	{
		return true; // 파라미터 따로 없음
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);
		
		if (pUser == nullptr || pUser->GetState() == 1)
		{
			std::cerr << "DrawstartJob::Execute : invalid req\n";
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			std::cerr << "DrawstartJob::Execute : User[" << m_userindex << "] Has No Room.\n";
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		bool bRet = pRoom->DrawStart();

		if (!bRet)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		pUser->SetDrawing();

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}
};

class DrawJob : public Job
{
public:
	DrawJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

	bool Parse(const std::string& param_) override
	{
		Serializer serializer;

		if (!serializer.Deserialize(param_, m_param))
		{
			std::cerr << "DrawJob::Parse : Failed to Parse\n";
			return false;
		}

		return true;
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);
		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		if (pUser->GetState() != 1)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		// todo. 좌표 확인같은거 안했다!!
		pRoom->Draw(m_userindex, m_param.drawNum, m_param.drawPosX, m_param.drawPosY, m_param.width, m_param.r, m_param.g, m_param.b);

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}

private:
	DrawParameter m_param;
};

class DrawEndJob : public Job
{
public:
	DrawEndJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

	bool Parse(const std::string& param_) override
	{
		Serializer serializer;

		if (!serializer.Deserialize(param_, m_param))
		{
			std::cerr << "DrawEndJob::Parse : Failed to Parse\n";
			return false;
		}

		return true;
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);
		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		pRoom->DrawEnd(m_userindex, m_param.drawNum);
		pUser->SetIdle();

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}

private:
	DrawEndParameter m_param;
};

class UndoJob : public Job
{
public:
	UndoJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm) {}

	bool Parse(const std::string& param_) override
	{
		return true; // 파라미터 따로 없음
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		uint32_t iRet = pRoom->Undo();

		if (iRet == 0)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}
};

class ChatJob : public Job
{
public:
	ChatJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

	bool Parse(const std::string& param_) override
	{
		m_param.assign(param_);
		return true; // 파라미터 따로 없음
	}

	InfoType Execute() override
	{
		if (m_param.length() > MAX_CHATTING_LEN)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		// 해당 맵에 전파
		pRoom->NotifyChatToAll(m_userindex, m_param);

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}

private:
	std::string m_param;
};

/// <summary>
/// DI + Parsing
/// </summary>
class JobFactory
{
public:
	void Init(UserManager* um, RoomManager* rm)
	{
		m_userManager = um;
		m_roomManager = rm;

		createFuncs.resize(static_cast<size_t>(ReqType::LAST) + 1);

		createFuncs[static_cast<size_t>(ReqType::CREATE_ROOM)] = &JobFactory::CreateRoomCreationJob;
		createFuncs[static_cast<size_t>(ReqType::ENTER_ROOM)] = &JobFactory::CreateEnterRoomJob;
		createFuncs[static_cast<size_t>(ReqType::REQ_ROOM_INFO)] = &JobFactory::CreateReqRoomInfoJob;
		createFuncs[static_cast<size_t>(ReqType::EXIT_ROOM)] = &JobFactory::CreateExitRoomJob;
		//createFuncs[static_cast<size_t>(ReqType::EDIT_ROOM_SETTING)] = &JobFactory::CreateRoomCreationJob;
		createFuncs[static_cast<size_t>(ReqType::SET_NICKNAME)] = &JobFactory::CreateSetNicknameJob;
		createFuncs[static_cast<size_t>(ReqType::DRAW_START)] = &JobFactory::CreateDrawStartJob;
		createFuncs[static_cast<size_t>(ReqType::DRAW)] = &JobFactory::CreateDrawJob;
		createFuncs[static_cast<size_t>(ReqType::DRAW_END)] = &JobFactory::CreateDrawEndJob;
		createFuncs[static_cast<size_t>(ReqType::UNDO)] = &JobFactory::CreateUndoJob;
		createFuncs[static_cast<size_t>(ReqType::CHAT)] = &JobFactory::CreateChatJob;

		return;
	}

	Job* CreateJob(uint16_t userindex_, std::string& req_)
	{
		ReqMessage msg;
		Serializer serializer;

		if (!serializer.Deserialize(req_, msg))
		{
			std::cerr << "JobFactory::CreateJob : Deserialize(ReqMessage) 실패\n";
			return nullptr;
		}

		if (msg.reqType > ReqType::LAST)
		{
			std::cerr << "JobFactory::CreateJob : Wrong ReqType\n";
			return nullptr;
		}

		auto func = createFuncs[static_cast<size_t>(msg.reqType)];

		if (func == nullptr)
		{
			std::cerr << "JobFactory::CreateJob : Cant Find CreateJob Func\n";
			return nullptr;
		}

		std::string payLoad(msg.payLoad, msg.payLoadSize);

		Job* pRet = (this->*(func))(userindex_, msg.reqNo);

		if (pRet == nullptr || !pRet->Parse(payLoad))
		{
			std::cerr << "JobFactory::CreateJob(" << static_cast<size_t>(msg.reqType) << ") : Failed to CreateJob\n";
			return nullptr;
		}

		return pRet;
	}

	Job* CreateExitRoomJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new ExitRoomJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

private:
	Job* CreateRoomCreationJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new RoomCreationJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateEnterRoomJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new EnterRoomJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateSetNicknameJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new SetNicknameJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateReqRoomInfoJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new ReqRoomInfoJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateDrawStartJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new DrawStartJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateDrawJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new DrawJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateDrawEndJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new DrawEndJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateUndoJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new UndoJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	Job* CreateChatJob(uint16_t userindex_, uint32_t reqNo_)
	{
		return new ChatJob(userindex_, reqNo_, m_userManager, m_roomManager);
	}

	UserManager* m_userManager = nullptr;
	RoomManager* m_roomManager = nullptr;

	typedef Job* (JobFactory::* CREATE_JOB_FUNC)(uint16_t, uint32_t);

	std::vector<CREATE_JOB_FUNC> createFuncs;
};