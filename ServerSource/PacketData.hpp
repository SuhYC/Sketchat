#pragma once

#include "Define.hpp"
#include <sstream>
#include <atomic>

const unsigned int PACKET_SIZE = MAX_SOCKBUF * 2;

// Network IO Packet
class PacketData
{
private:
	char* m_pData;
	DWORD ioSize;

public:
	PacketData()
	{
		ioSize = 0;

		m_pData = new char[PACKET_SIZE];
	}

	virtual ~PacketData()
	{
		if (m_pData != nullptr)
		{
			Free();
		}
	}

	void Clear()
	{
		if (m_pData == nullptr)
		{
			return;
		}
		ZeroMemory(m_pData, PACKET_SIZE);
		ioSize = 0;
	}

	void Free()
	{
		if (m_pData == nullptr)
		{
			return;
		}

		delete m_pData;

		m_pData = nullptr;
	}

	// √ ±‚»≠
	bool Init(std::string& strData_)
	{
		if (m_pData == nullptr)
		{
			std::cerr << "PacketData::Init : Buffer nullptr.\n";
			return false;
		}

		if (strData_.length() + sizeof(DWORD) > PACKET_SIZE)
		{
			std::cerr << "PacketData::Init : Not Enough Buffer.\n";
			return false;
		}

		ioSize = strData_.size();
		CopyMemory(m_pData, &ioSize, sizeof(DWORD));
		CopyMemory(m_pData + sizeof(DWORD), &strData_[0], ioSize);

		ioSize += sizeof(DWORD);

		return true;
	}

	char* GetData() { return m_pData; }
	DWORD GetSize() const { return ioSize; }
};