#pragma once

#include <functional>
#include <iostream>
#include <chrono>
#include <mutex>
#include <atomic>

class User
{
public:
	User(const int idx_) : m_idx(idx_), m_roomIdx(-1), m_state(0)
	{
	}

	~User()
	{
	}

	void SetIP(const uint32_t ip_)
	{
		m_ip.store(ip_);
		return;
	}

	void Init()
	{
		m_roomIdx.store(-1);
		m_state.store(0);
	}

	/// <summary>
	/// 현재 방에 입장해있는 상태가 아니면 -1
	/// </summary>
	/// <param name="roomidx_"></param>
	void SetRoomIdx(int16_t roomidx_) noexcept
	{
		m_roomIdx.store(roomidx_);
		return;
	}

	void SetDrawing() noexcept { m_state.store(1); }
	void SetIdle() noexcept { m_state.store(0); }

	int32_t GetState() const noexcept { return m_state.load(); }

	/// <summary>
	/// 현재 방에 입장해있는 상태가 아니면 -1반환
	/// </summary>
	/// <returns></returns>
	int16_t GetRoomIdx() const noexcept { return m_roomIdx; }

	void SetName(std::string& str) noexcept
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		name = str;

		return;
	}

	const std::string& GetName() const noexcept { return name; }

	// 바이트형식의 ip를 std::string으로 변환
	const std::string GetIP() const noexcept
	{
		char ipStr[INET_ADDRSTRLEN];

		if (inet_ntop(AF_INET, &m_ip, ipStr, sizeof(ipStr)) == nullptr)
		{
			return std::string();
		}

		return std::string(ipStr);
	}

private:
	std::mutex m_mutex;
	int m_idx;
	std::atomic<int16_t> m_roomIdx;
	std::atomic<uint32_t> m_ip;
	
	std::atomic<int32_t> m_state; // draw : 1, idle : 0.  disconnect -> handledisconnect

	std::string name;
};