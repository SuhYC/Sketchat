#pragma once;

#include <string>
#include <chrono>
#include <set>
#include <mutex>
#include <map>
#include <functional>

#include "NetworkMessage.hpp"
#include "Serialization.hpp"
#include "Texture2D.hpp"
#include "User.hpp"

const unsigned short MAX_SIZE_OF_ROOM = 8;

class Room
{
public:
	Room(uint16_t idx_) : m_MaxUsers(0), m_ManagerUserIndex(0), m_idx(idx_), m_rwlock(0)
	{

	}

	bool Init()
	{
		m_Password.clear();
		m_MaxUsers = 0; // 방이 비어있다는 의미
		m_rwlock.store(0); // idle.
		m_CommandStack.Init();

		return true;
	}

	void SetManager(unsigned short connectionIndex_)
	{
		if (m_MaxUsers == 0) // 빈방을 받아서 새로운 방을 만드는 과정에서 필요
		{
			m_MaxUsers = 1; // 이제 빈방 아님.
		}

		m_ManagerUserIndex = connectionIndex_;
		return;
	}

	bool Init(std::string& RoomName_, std::string& password_, unsigned short maxUsers_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (maxUsers_ < m_UserIndexes.size()) // 현재 인원보다는 적으면 안되지
		{
			return false;
		}

		m_RoomName = RoomName_;
		m_Password = password_;
		m_MaxUsers = maxUsers_;

		return true;
	}

	bool Enter(unsigned short connectionIndex_, User* pUser_, const std::string& nickname_, std::string password_ = std::string())
	{
		if (m_UserIndexes.size() >= m_MaxUsers)
		{
			return false;
		}

		if (m_Password.empty() || m_Password == password_) // 일단 짧은 문자열이라 이렇게 비교함. 해시비교까지는 필요하지 않을거 같음.
		{
			NotifyUserEnter(connectionIndex_, nickname_);

			{
				std::lock_guard<std::mutex> guard(m_mutex);

				m_UserIndexes.emplace(connectionIndex_, pUser_);
			}

			return true;
		}

		return false;
	}

	void Exit(unsigned short connectionIndex_)
	{
		{
			std::lock_guard<std::mutex> guard(m_mutex);
			m_UserIndexes.erase(connectionIndex_);

			if (m_UserIndexes.empty())
			{
				Init();
				return;
			}
		}

		NotifyUserExit(connectionIndex_);

		{
			std::lock_guard<std::mutex> guard(m_mutex);
			if (m_ManagerUserIndex == connectionIndex_)
			{
				auto itr = m_UserIndexes.begin();
				SetManager(itr->first);

				// 매니저 된거 알려줘야할듯
			}
		}
		return;
	}

	bool Empty()
	{
		return m_MaxUsers == 0;
	}

	const std::string& GetName() const noexcept { return m_RoomName; }
	uint16_t GetNowUsers() const noexcept { return m_UserIndexes.size(); }
	uint16_t GetMaxUsers() const noexcept { return m_MaxUsers; }

	/// <summary>
	/// 읽기락을 건다.
	/// </summary>
	/// <returns></returns>
	bool DrawStart()
	{
		return Try_Read_Lock();
	}

	/// <summary>
	/// 읽기락이 걸린 상태에서
	/// 캔버스에 그리는 동작을 수행한다.
	/// </summary>
	/// <returns></returns>
	bool Draw(uint16_t userindex_, uint16_t drawNum_, int16_t drawPosX_, int16_t drawPosY, float width_, float r_, float g_, float b_)
	{
		uint32_t drawkey = ((uint32_t)userindex_ << 16) | drawNum_;
		Vector2Int vertex(drawPosX_, drawPosY);
		Color col(r_, g_, b_);

		m_CommandStack.AddVertexToCommand(drawkey, vertex, col, width_);

		NotifyDrawToAll(userindex_, drawNum_, drawPosX_, drawPosY, width_, r_, g_, b_);

		return true;
	}

	/// <summary>
	/// 읽기락을 해제한다.
	/// 하나의 드래그를 단위로 저장하고 반영한다.
	/// </summary>
	/// <returns></returns>
	bool DrawEnd(uint16_t userindex_, uint16_t drawNum_)
	{
		uint32_t drawkey = ((uint32_t)userindex_ << 16) | drawNum_;

		m_CommandStack.Push(drawkey);

		NotifyDrawEndToAll(userindex_, drawNum_);
		return Read_Unlock();
	}

	void CutTheLine(uint16_t userindex_, uint16_t drawNum_)
	{
		uint32_t drawkey = ((uint32_t)userindex_ << 16) | drawNum_;

		m_CommandStack.Push(drawkey);

		NotifyDrawEndToAll(userindex_, drawNum_);

	}

	/// <summary>
	/// 최근 완료된 드래그를 실행취소한다. 
	/// 쓰기락을 걸고 해제한다.
	/// 
	/// if ret == 0 -> failed to try_unique_lock
	/// </summary>
	/// <returns>drawkey</returns>
	uint32_t Undo()
	{
		bool bRet = Try_Write_Lock();

		if (!bRet)
		{
			return 0; // failed to lock
		}

		uint32_t iRet = m_CommandStack.Undo();

		if (iRet != 0)
		{
			uint16_t usernum = iRet >> 16;
			uint16_t drawnum = iRet & 0xFFFF;

			NotifyUndo(usernum, drawnum);
		}

		Write_Unlock();
		return iRet; // drawkey
	}

	void NotifyChatToAll(const unsigned short userindex_, const std::string& msg_)
	{
		ChatInfo stInfo{userindex_, msg_.length()};
		if (!msg_.empty())
		{
			CopyMemory(stInfo.msg, msg_.c_str(), msg_.length());
		}

		std::string msg;
		Serializer serializer;
		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyChatToAll : Failed to Serialize.\n";
			return;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		SendInfoToUsersFunc(m_UserIndexes, InfoType::CHAT, msg);
		return;
	}

	void NotifyDrawToAll(const unsigned short userindex_, const uint16_t drawNum_, int16_t drawPosX_, int16_t drawPosY_, float width_, float r_, float g_, float b_)
	{
		DrawInfo stInfo{ drawNum_, userindex_, drawPosX_, drawPosY_, width_, r_, g_, b_};

		std::string msg;
		Serializer serializer;

		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyDrawToAll : Failed to Serialize.\n";
			return;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		SendInfoToUsersFunc(m_UserIndexes, InfoType::DRAW, msg);
	}

	void NotifyDrawEndToAll(const unsigned short userindex_, const uint16_t drawNum_)
	{
		DrawEndInfo stInfo{ drawNum_, userindex_ };

		std::string msg;
		Serializer serializer;

		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyDrawToAll : Failed to Serialize.\n";
			return;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		SendInfoToUsersFunc(m_UserIndexes, InfoType::DRAW_END, msg);
	}

	uint16_t GetIdx() const noexcept { return m_idx; }

	/// <summary>
	/// 사용에 주의.
	/// try_lock의 형태를 가져가려다보니 RAII형태로 구현하지 못했다.
	/// 더구나 클라이언트의 특정 동작에 의해 lock을 걸고,
	/// 다른 특정 동작에 의해 unlock하는 방식이다 보니
	/// 다중으로 잠그거나, 잠구지 않은 상태에서 해제하려고 할 수도 있다.
	/// 
	/// 사용 전에 한번더 생각할 것.
	/// (std::set과 std::mutex를 사용해서 안전하게 쓸 수도 있지만 성능상의 이유로 조금 위험하게 간다.)
	/// 
	/// 해당 함수를 public으로 선언한 이유는 ReqHandler에서 HandleDisconnect 함수를 수행하면서
	/// User* 의 데이터를 조회하고 그에 맞게 Read_Unlock을 수행해주기 위함.
	/// 이외에 장소에서 호출 시 주의.
	/// </summary>
	/// <returns></returns>
	bool Read_Unlock()
	{
		int32_t expected = m_rwlock.load();

		while (expected > 0)
		{
			if (m_rwlock.compare_exchange_weak(expected, expected - 1))
			{
				//std::cout << "Room::Read_Unlock : 락 품!\n";
				return true;
			}
		}

		return false;
	}

	void NotifyAllExistUser(const unsigned userindex_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		
		for (auto& itr : m_UserIndexes)
		{
			NotifyExistUser(userindex_, itr.first, itr.second->GetName());
		}
	}

	/// <summary>
	/// 캔버스의 상태를 전송하려 하는데 캔버스의 자체 크기가 너무 크다.
	/// 512 * 512 * pixeldatasize + Command * 50 정도의 크기인데,
	/// 512 * 512 만 하더라도 25만바이트 이상.
	/// 픽셀 하나의 정보를 3바이트로 줄여도 78만 바이트 가량을 송신해야한다.
	///
	/// 캔버스를 128 청크로 분류하여 데이터를 2048픽셀마다 요청한다.
	/// 먼저 0번 청크를 요청하기 전, 읽기중인 상태를 기록한다.
	/// (읽는 중에는 DrawCommand가 생겨도 캔버스를 수정하거나 전파하지 않고 따로 큐에 담는다.)
	/// 127번 청크까지 요청이 완료되면 128번으로 DrawCommand 데이터를 요청하고 읽는중 상태를 조정한다.
	/// (여러명이 요청하고 있었을 수도 있으니까.)
	/// 
	/// 138개의 청크를 송신하려다보니 링버퍼가 가득차서 한번에 요청할 수 없을 수도 있다.
	/// 그러므로 JobQueue를 만들어 송신이 끝나지 못한 작업에 대한 정보를 기록한다.
	/// </summary>
	/// <param name="userindex_"></param>
	bool NotifyCanvasInfo(const unsigned short userindex_, const unsigned short chunkidx_)
	{
		std::string msg;
		if (!m_CommandStack.GetData(chunkidx_, msg))
		{
			std::cerr << "Room::NotifyCanvasInfo : Failed to Serialize\n";
			return false;
		}

		// Canvas
		if (chunkidx_ < MAX_CHUNKS_ON_CANVAS_INFO)
		{
			if (!SendInfoFunc(userindex_, InfoType::ROOM_CANVAS_INFO, msg))
			{
				return false;
			}
		}
		// DrawCommand
		else
		{
			if (!SendInfoFunc(userindex_, InfoType::ROOM_DRAWCOMMAND_INFO, msg))
			{
				return false;
			}
		}


		return true;
	}

	//----- func pointer
	std::function<void(std::map<unsigned short, User*>&, InfoType, const std::string&)> SendInfoToUsersFunc;
	std::function<bool(const unsigned short, InfoType, const std::string&)> SendInfoFunc;


private:
	void NotifyExistUser(const unsigned short userindex_, const unsigned short enteredUserIndex_, const std::string& nickname_)
	{
		if (nickname_.length() > MAX_ROOM_NAME_LEN || nickname_.empty())
		{
			return;
		}

		RoomEnterInfo stInfo{ enteredUserIndex_, nickname_.length() };

		CopyMemory(stInfo.nickname, nickname_.c_str(), nickname_.length());

		std::string msg;
		Serializer serializer;

		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyUserEnter : Failed to Serialize.\n";
			return;
		}

		SendInfoFunc(userindex_, InfoType::ROOM_USER_ENTERED, msg);
	}

	void NotifyUserEnter(const unsigned short userindex_, const std::string& nickname_)
	{
		if (nickname_.length() > MAX_ROOM_NAME_LEN || nickname_.empty())
		{
			return;
		}

		RoomEnterInfo stInfo{ userindex_, nickname_.length() };

		CopyMemory(stInfo.nickname, nickname_.c_str(), nickname_.length());

		std::string msg;
		Serializer serializer;

		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyUserEnter : Failed to Serialize.\n";
			return;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		SendInfoToUsersFunc(m_UserIndexes, InfoType::ROOM_USER_ENTERED, msg);
	}

	void NotifyUserExit(const unsigned short userindex_)
	{
		RoomExitInfo stInfo{ userindex_ };

		std::string msg;
		Serializer serializer;
		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyUserExit : Failed to Serialize.\n";
			return;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		SendInfoToUsersFunc(m_UserIndexes, InfoType::ROOM_USER_EXITED, msg);
	}

	

	void NotifyUndo(const unsigned short userindex_, const uint16_t drawNum_)
	{
		UndoInfo stInfo{ drawNum_, userindex_ };

		std::string msg;
		Serializer serializer;
		if (!serializer.Serialize(stInfo, msg))
		{
			std::cerr << "Room::NotifyUndo : Failed to Serialize.\n";
			return;
		}

		SendInfoToUsersFunc(m_UserIndexes, InfoType::UNDO, msg);
	}

	bool Try_Read_Lock()
	{
		int32_t expected = m_rwlock.load();
		while (expected >= 0) // 쓰기동작과 배제
		{
			if (m_rwlock.compare_exchange_weak(expected, expected + 1))
			{
				//std::cout << "Room::Try_Read_Lock : 락 검!\n";
				return true;
			}
		}

		return false;
	}

	bool Try_Write_Lock()
	{
		int32_t expected = 0; // 다른 모든 동작과 배제
		return m_rwlock.compare_exchange_strong(expected, -1);
	}

	bool Write_Unlock()
	{
		int32_t expected = -1;
		return m_rwlock.compare_exchange_strong(expected, 0);
	}

	std::string m_RoomName;
	std::string m_Password;

	unsigned short m_MaxUsers;

	unsigned short m_ManagerUserIndex;

	std::map<unsigned short, User*> m_UserIndexes;

	uint16_t m_idx;

	CommandStack m_CommandStack;

	/// <summary>
	/// 방에 출입하는 상황에 대한 상호배제
	/// </summary>
	std::mutex m_mutex;

	/// <summary>
	/// 간단한 읽기쓰기락을 위해 선언한 변수.
	/// value > 0 : reading
	/// value == -1 : writing
	/// 
	/// value == -1 : cant read
	/// value != 0 : cant write
	/// 
	/// try_lock의 느낌으로 구현해보자.
	/// </summary>
	std::atomic<int32_t> m_rwlock;
};