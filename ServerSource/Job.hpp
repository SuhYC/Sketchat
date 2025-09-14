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

#include "concurrent_queue.h"

#include <typeinfo>
#include <variant>

struct DIStruct
{
	UserManager* um;
	RoomManager* rm;
};

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

		if (pUser == nullptr || pUser->GetRoomIdx() != -1)
		{
			std::cerr << "RoomCreationJob::Execute : invalid req\n";

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
		
		if (pUser == nullptr)
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
			std::cerr << "DrawStartJob::Execute : User[" << m_userindex << "] Cant lock r-lock\n";
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

/// <summary>
/// 읽기락의 상태를 변화시키지 않고 drawnum을 변화시킬 동작.
/// 획이 너무 길어지면 송신하기 어렵기 때문에 획을 일정길이마다 자르기 위함이다.
/// </summary>
class CutTheLineJob : public Job
{
public:
	CutTheLineJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm) : Job(userindex_, reqNo_, um, rm), m_param() {}

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

		if (pUser->GetState() != 1) // 그래도 그리는 중이어야한다.
		{
			return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
		}

		pRoom->CutTheLine(m_userindex, m_param.drawNum);

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

class ReqCanvasInfoJob : public Job
{
public:
	ReqCanvasInfoJob(uint16_t userindex_, uint32_t reqNo_, UserManager* um, RoomManager* rm)
		: Job(userindex_, reqNo_, um, rm), chunkidx(0) {}

	bool Parse(const std::string& param_) override
	{
		return true; // 파라미터 따로 없음
	}

	InfoType Execute() override
	{
		User* pUser = m_userManager->GetUserByConnIndex(m_userindex);

		if (pUser == nullptr)
		{
			if (chunkidx == 0)
			{
				return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
			}
			return InfoType::REQ_FAILED;
		}

		Room* pRoom = m_roomManager->GetRoomByIndex(pUser->GetRoomIdx());

		if (pRoom == nullptr)
		{
			if (chunkidx == 0)
			{
				return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_FAILED);
			}
			return InfoType::REQ_FAILED;
		}

		while (chunkidx < MAX_CHUNKS_ON_CANVAS_INFO + MAX_CHUNKS_ON_DRAWCOMMAND)
		{
			if (!pRoom->NotifyCanvasInfo(m_userindex, chunkidx))
			{
				return InfoType::NOT_FINISHED; // 버퍼 오버플로 등의 문제로 중단됨.
			}
			chunkidx++;
		}

		return SendResultMsg(m_userindex, m_reqNo, InfoType::REQ_SUCCESS);
	}

	uint16_t chunkidx;
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
/// 선언한 모든 타입의 Job 파생클래스를 담을 것.
/// 해당 variant를 바탕으로 가장 크기가 큰 Job을 확인하는 로직.
/// </summary>
using Jobs = std::variant<
	// ----- 작업이 추가될 때마다 추가해주어야함
	RoomCreationJob,
	EnterRoomJob,
	ExitRoomJob,
	SetNicknameJob,
	ReqRoomInfoJob,
	DrawStartJob,
	DrawJob,
	DrawEndJob,
	CutTheLineJob,
	UndoJob,
	ReqCanvasInfoJob,
	ChatJob
>;

const uint32_t MAX_JOB_SIZE = sizeof(Jobs);
const uint32_t MEMORY_POOL_SIZE = 10;

class JobMemoryPool final
{
public:
	JobMemoryPool()
	{
		try
		{
			void* block = nullptr;

			for (int i = 0; i < MEMORY_POOL_SIZE; i++)
			{
				block = ::operator new(MAX_JOB_SIZE, std::nothrow);
				if (block != nullptr)
				{
					FreeList.push(block);
				}
			}
		}
		catch (std::bad_alloc& e)
		{
			std::cerr << "JobMemoryPool::Constructor : Failed to Allocate Memory Block.\n";
			return;
		}
	}
	~JobMemoryPool()
	{
		void* block = nullptr;
		while (!FreeList.empty())
		{
			if (FreeList.try_pop(block))
			{
				if (block != nullptr)
				{
					::operator delete(block);
				}
			}
		}
	}

	template<typename T>
	typename std::enable_if<std::is_base_of<Job, T>::value, Job*>::type
		Allocate(uint16_t userindex_, uint32_t reqNo_, DIStruct& stDI_)
	{
		if (sizeof(T) > MAX_JOB_SIZE)
		{
			std::cerr << "JobMemoryPool::Allocate : Check union Jobs. JobType : " << typeid(T).name() << "\n";
			return nullptr;
		}

		void* memory = nullptr;

		if (!FreeList.try_pop(memory))
		{
			memory = ::operator new(MAX_JOB_SIZE, std::nothrow);
			if (memory == nullptr)
			{
				std::cerr << "JobMemoryPool::Allocate : Not Enough Mem.\n";
				return nullptr;
			}
		}

		Job* pRet = new (memory) T(userindex_, reqNo_, stDI_.um, stDI_.rm);

		return pRet;
	}

	void Deallocate(Job* job_)
	{
		if (job_ == nullptr)
		{
			return;
		}

		job_->~Job();

		FreeList.push(job_);
		return;
	}

private:
	Concurrency::concurrent_queue<void*> FreeList;

};

/// <summary>
/// DI + Parsing
/// </summary>
class JobFactory
{
public:
	void Init(UserManager* um_, RoomManager* rm_)
	{
		m_DIStruct.um = um_;
		m_DIStruct.rm = rm_;

		createFuncs.resize(static_cast<size_t>(ReqType::LAST) + 1);

		Register<RoomCreationJob>(ReqType::CREATE_ROOM);
		Register<EnterRoomJob>(ReqType::ENTER_ROOM);
		Register<ReqRoomInfoJob>(ReqType::REQ_ROOM_INFO);
		Register<ReqCanvasInfoJob>(ReqType::REQ_CANVAS_INFO);
		Register<ExitRoomJob>(ReqType::EXIT_ROOM);
		//Register<EditRoomSettingJob>(ReqType::EDIT_ROOM_SETTING);
		Register<SetNicknameJob>(ReqType::SET_NICKNAME);

		Register<DrawStartJob>(ReqType::DRAW_START);
		Register<DrawJob>(ReqType::DRAW);
		Register<DrawEndJob>(ReqType::DRAW_END);
		Register<CutTheLineJob>(ReqType::CUT_THE_LINE);
		Register<UndoJob>(ReqType::UNDO);
		Register<ChatJob>(ReqType::CHAT);

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

		if (!func)
		{
			std::cerr << "JobFactory::CreateJob : Cant Find CreateJob Func\n";
			return nullptr;
		}

		std::string payLoad(msg.payLoad, msg.payLoadSize);

		Job* pRet = func(userindex_, msg.reqNo);

		if (pRet == nullptr || !pRet->Parse(payLoad))
		{
			std::cerr << "JobFactory::CreateJob(" << static_cast<size_t>(msg.reqType) << ") : Failed to CreateJob\n";
			return nullptr;
		}

		return pRet;
	}

	Job* CreateExitRoomJob(uint16_t userindex_, uint32_t reqNo_)
	{
		Job* pRet = m_pool.Allocate<ExitRoomJob>(userindex_, reqNo_, m_DIStruct);

		if (pRet == nullptr)
		{
			std::cerr << "JobFactory::CreateExitRoomJob : Failed to Create Job\n";
		}

		return pRet;
	}

	void DeallocateJob(Job* pJob)
	{
		m_pool.Deallocate(pJob);

		return;
	}

private:

	/// <summary>
	/// 추가한 작업객체와 요청코드를 연결하는 함수.
	/// 요청코드에 맞는 작업객체를 생성하는 람다식을 넘긴다.
	/// </summary>
	/// <typeparam name="T">Job의 파생클래스 타입</typeparam>
	/// <param name="eReqType_">요청코드</param>
	/// <returns></returns>
	template<typename T>
	typename std::enable_if<std::is_base_of<Job, T>::value, void>::type
		Register(ReqType eReqType_)
	{
		createFuncs[static_cast<int32_t>(eReqType_)] =
			[this](uint16_t userindex_, uint32_t reqNo_) -> Job* {
			return m_pool.Allocate<T>(userindex_, reqNo_, m_DIStruct);
			};
	}


	DIStruct m_DIStruct;

	UserManager* m_userManager = nullptr;
	RoomManager* m_roomManager = nullptr;

	JobMemoryPool m_pool;

	std::vector<std::function<Job* (uint16_t, uint32_t)>> createFuncs;
};