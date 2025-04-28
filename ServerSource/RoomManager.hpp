#pragma once

#include "Room.hpp"
#include <vector>
#include <mutex>
#include "Serialization.hpp"

const int MAX_ROOMS = 100;

class RoomManager
{
public:
	~RoomManager()
	{
		for (int i = 0; i < m_MaxRoom; i++)
		{
			if (m_Rooms[i] != nullptr)
			{
				delete m_Rooms[i];
			}
		}
	}

	void Init(int roomCount_ = MAX_ROOMS)
	{
		m_Rooms.reserve(roomCount_);
		m_MaxRoom = roomCount_;

		for (int i = 0; i < roomCount_; i++)
		{
			Room* pRoom = new Room(i);

			pRoom->SendInfoFunc = this->SendInfoFunc;
			pRoom->SendInfoToUsersFunc = this->SendInfoToUsersFunc;

			m_Rooms.push_back(pRoom);
		}
	}

	Room* GetEmptyRoom(unsigned short connectionIdx_, const std::string& nickname_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);

		for (int i = 0; i < m_MaxRoom; i++)
		{
			if (m_Rooms[i]->Empty())
			{
				m_Rooms[i]->SetManager(connectionIdx_);
				//m_Rooms[i]->Enter(connectionIdx_, nickname_); 서순문제때문에 ReqHandler에서 요청하는 걸로..

				return m_Rooms[i];
			}
		}

		// 빈 방 없음
		return nullptr;
	}

	/// <summary>
	/// 활성화된 방 중에 인덱스로 호출함.
	/// 해당 방이 빈방인경우 nullptr 반환.
	/// </summary>
	/// <param name="roomidx_"></param>
	/// <returns></returns>
	Room* GetRoomByIndex(int16_t roomidx_)
	{
		if (roomidx_ > m_MaxRoom || roomidx_ < 0)
		{
			return nullptr;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		if (m_Rooms[roomidx_]->Empty())
		{
			return nullptr;
		}

		return m_Rooms[roomidx_];
	}

	bool GetRoomInfo(std::string& out_)
	{
		std::vector<RoomInfo> vec;
		vec.reserve(m_MaxRoom);

		std::lock_guard<std::mutex> guard(m_mutex);

		for (int i = 0; i < m_MaxRoom; i++)
		{
			if (m_Rooms[i]->Empty())
			{
				continue;
			}

			std::string str(m_Rooms[i]->GetName());


			vec.push_back(RoomInfo{});
			vec.back().roomNum = i;
			vec.back().nowUsers = m_Rooms[i]->GetNowUsers();
			vec.back().maxUsers = m_Rooms[i]->GetMaxUsers();
			vec.back().roomNameLen = str.length();
			
			CopyMemory(vec.back().roomName, str.c_str(), str.length());
		}

		SerializeLib slib;

		uint32_t size = sizeof(uint16_t) + sizeof(RoomInfo) * vec.size();

		bool bRet = slib.Init(size) &&
			slib.Push(static_cast<uint16_t>(vec.size()));

		if (!bRet)
		{
			return false;
		}

		for (auto itr = vec.begin(); itr != vec.end(); itr++)
		{
			bRet = slib.Push(itr->roomNum) &&
				slib.Push(itr->roomName, itr->roomNameLen) &&
				slib.Push(itr->nowUsers) && slib.Push(itr->maxUsers);

			if (!bRet)
			{
				return false;
			}
		}
		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	//----- func pointer
	std::function<void(std::map<unsigned short, User*>&, InfoType, const std::string&)> SendInfoToUsersFunc;
	std::function<void(const unsigned short, InfoType, const std::string&)> SendInfoFunc;

private:
	int m_MaxRoom;
	std::vector<Room*> m_Rooms;

	std::mutex m_mutex;
};