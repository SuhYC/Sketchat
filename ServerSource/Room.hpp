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
		m_MaxUsers = 0; // ���� ����ִٴ� �ǹ�
		m_rwlock.store(0); // idle.
		m_CommandStack.Init();

		return true;
	}

	void SetManager(unsigned short connectionIndex_)
	{
		if (m_MaxUsers == 0) // ����� �޾Ƽ� ���ο� ���� ����� �������� �ʿ�
		{
			m_MaxUsers = 1; // ���� ��� �ƴ�.
		}

		m_ManagerUserIndex = connectionIndex_;
		return;
	}

	bool Init(std::string& RoomName_, std::string& password_, unsigned short maxUsers_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (maxUsers_ < m_UserIndexes.size()) // ���� �ο����ٴ� ������ �ȵ���
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

		if (m_Password.empty() || m_Password == password_) // �ϴ� ª�� ���ڿ��̶� �̷��� ����. �ؽú񱳱����� �ʿ����� ������ ����.
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

				// �Ŵ��� �Ȱ� �˷�����ҵ�
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
	/// �б���� �Ǵ�.
	/// </summary>
	/// <returns></returns>
	bool DrawStart()
	{
		return Try_Read_Lock();
	}

	/// <summary>
	/// �б���� �ɸ� ���¿���
	/// ĵ������ �׸��� ������ �����Ѵ�.
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
	/// �б���� �����Ѵ�.
	/// �ϳ��� �巡�׸� ������ �����ϰ� �ݿ��Ѵ�.
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
	/// �ֱ� �Ϸ�� �巡�׸� ��������Ѵ�. 
	/// ������� �ɰ� �����Ѵ�.
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
	/// ��뿡 ����.
	/// try_lock�� ���¸� ���������ٺ��� RAII���·� �������� ���ߴ�.
	/// ������ Ŭ���̾�Ʈ�� Ư�� ���ۿ� ���� lock�� �ɰ�,
	/// �ٸ� Ư�� ���ۿ� ���� unlock�ϴ� ����̴� ����
	/// �������� ��װų�, �ᱸ�� ���� ���¿��� �����Ϸ��� �� ���� �ִ�.
	/// 
	/// ��� ���� �ѹ��� ������ ��.
	/// (std::set�� std::mutex�� ����ؼ� �����ϰ� �� ���� ������ ���ɻ��� ������ ���� �����ϰ� ����.)
	/// 
	/// �ش� �Լ��� public���� ������ ������ ReqHandler���� HandleDisconnect �Լ��� �����ϸ鼭
	/// User* �� �����͸� ��ȸ�ϰ� �׿� �°� Read_Unlock�� �������ֱ� ����.
	/// �̿ܿ� ��ҿ��� ȣ�� �� ����.
	/// </summary>
	/// <returns></returns>
	bool Read_Unlock()
	{
		int32_t expected = m_rwlock.load();

		while (expected > 0)
		{
			if (m_rwlock.compare_exchange_weak(expected, expected - 1))
			{
				//std::cout << "Room::Read_Unlock : �� ǰ!\n";
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
	/// ĵ������ ���¸� �����Ϸ� �ϴµ� ĵ������ ��ü ũ�Ⱑ �ʹ� ũ��.
	/// 512 * 512 * pixeldatasize + Command * 50 ������ ũ���ε�,
	/// 512 * 512 �� �ϴ��� 25������Ʈ �̻�.
	/// �ȼ� �ϳ��� ������ 3����Ʈ�� �ٿ��� 78�� ����Ʈ ������ �۽��ؾ��Ѵ�.
	///
	/// ĵ������ 128 ûũ�� �з��Ͽ� �����͸� 2048�ȼ����� ��û�Ѵ�.
	/// ���� 0�� ûũ�� ��û�ϱ� ��, �б����� ���¸� ����Ѵ�.
	/// (�д� �߿��� DrawCommand�� ���ܵ� ĵ������ �����ϰų� �������� �ʰ� ���� ť�� ��´�.)
	/// 127�� ûũ���� ��û�� �Ϸ�Ǹ� 128������ DrawCommand �����͸� ��û�ϰ� �д��� ���¸� �����Ѵ�.
	/// (�������� ��û�ϰ� �־��� ���� �����ϱ�.)
	/// 
	/// 138���� ûũ�� �۽��Ϸ��ٺ��� �����۰� �������� �ѹ��� ��û�� �� ���� ���� �ִ�.
	/// �׷��Ƿ� JobQueue�� ����� �۽��� ������ ���� �۾��� ���� ������ ����Ѵ�.
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
		while (expected >= 0) // ���⵿�۰� ����
		{
			if (m_rwlock.compare_exchange_weak(expected, expected + 1))
			{
				//std::cout << "Room::Try_Read_Lock : �� ��!\n";
				return true;
			}
		}

		return false;
	}

	bool Try_Write_Lock()
	{
		int32_t expected = 0; // �ٸ� ��� ���۰� ����
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
	/// �濡 �����ϴ� ��Ȳ�� ���� ��ȣ����
	/// </summary>
	std::mutex m_mutex;

	/// <summary>
	/// ������ �б⾲����� ���� ������ ����.
	/// value > 0 : reading
	/// value == -1 : writing
	/// 
	/// value == -1 : cant read
	/// value != 0 : cant write
	/// 
	/// try_lock�� �������� �����غ���.
	/// </summary>
	std::atomic<int32_t> m_rwlock;
};