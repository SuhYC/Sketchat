#pragma once

#include <exception>
#include <iostream>
#include <Windows.h>
#include <string_view>

#include "Define.hpp"
#include "NetworkMessage.hpp"

const unsigned long MAX_STRING_SIZE = 1000;

/// <summary>
/// char*형 버퍼.
/// push한 데이터를 순서대로 저장한 후 char*형 데이터로 내보낼 수 있다.
/// 일단 리틀엔디안으로 통일하는 것으로 생각한다. << 당연히 클라이언트 배포 시에는 머신의 엔디안을 고려할 수 있도록 해야한다.
/// 문자열 데이터는 4바이트 길이 + 문자열 로 구성한다. << 일단 인코딩은 euc-kr
/// </summary>
class SerializeLib
{
public:
	SerializeLib() : data(nullptr), size(0), capacity(0) {}
	~SerializeLib()
	{
		if (data != nullptr)
		{
			delete[] data;
		}
	}
	/// <summary>
	/// 기존의 데이터를 정리하고
	/// 새로운 버퍼를 만드는 함수
	/// </summary>
	/// <param name="cap_"></param>
	/// <returns></returns>
	bool Init(uint32_t cap_ = MAX_PAYLOAD_SIZE)
	{
		if (data != nullptr)
		{
			delete[] data;
		}

		data = new(std::nothrow) char[cap_];
		size = 0;

		if (data == nullptr)
		{
			std::cerr << "SerializeLib::Init : Memory Alloc Failed.\n";
			capacity = 0;
			return false;
		}

		capacity = cap_;
		return true;
	}

	/// <summary>
	/// 버퍼는 유지하고 기존의 데이터만 정리하는 함수
	/// </summary>
	/// <returns></returns>
	bool Flush()
	{
		size = 0;
	}

	/// <summary>
	/// 기존의 데이터를 유지한 채
	/// 버퍼의 크기를 조정하는 함수.
	/// 기존 데이터가 조정하려는 크기보다 큰 경우 실패한다.
	/// </summary>
	/// <param name="cap_"></param>
	/// <returns></returns>
	bool Resize(uint32_t cap_)
	{
		if (size > cap_)
		{
			return false;
		}

		if (capacity == cap_)
		{
			return true;
		}

		char* newData = new(std::nothrow) char[cap_];

		if (newData == nullptr)
		{
			std::cerr << "SerializeLib::Resize : Memory Alloc Failed.\n";
			return false;
		}

		if (data != nullptr)
		{
			CopyMemory(newData, data, size);
			delete[] data;
		}

		data = newData;
		capacity = cap_;

		return true;
	}


	template <typename T>
	typename std::enable_if<
		std::is_same<T, uint16_t>::value ||
		std::is_same<T, uint32_t>::value ||
		std::is_same<T, uint64_t>::value ||
		std::is_same<T, int16_t>::value ||
		std::is_same<T, int32_t>::value ||
		std::is_same<T, int64_t>::value ||
		std::is_same<T, float>::value ||
		std::is_same<T, double>::value ||
		std::is_same<T, std::string>::value,
		bool>::type
		Push(const T& rhs_)
	{
		if (size + sizeof(rhs_) > capacity)
		{
			return false;
		}

		CopyMemory(data + size, &rhs_, sizeof(rhs_));
		size += sizeof(rhs_);
		return true;
	}

	/// <summary>
	/// 문자열은 문자열의 바이트크기를 먼저 삽입한 후
	/// 문자열을 삽입한다.
	/// 
	/// 빈 문자열이 오더라도 헤더 4바이트는 삽입된다.
	/// optional msg를 넣는 경우를 생각해서 이렇게 만듬.
	/// </summary>
	/// <param name="rhs_"></param>
	/// <returns></returns>
	template <>
	bool Push<std::string>(const std::string& rhs_)
	{
		uint32_t len = rhs_.length();

		if (size + len + sizeof(uint32_t) > capacity)
		{
			return false;
		}

		CopyMemory(data + size, &len, sizeof(uint32_t));
		size += sizeof(uint32_t);

		CopyMemory(data + size, rhs_.c_str(), len);
		size += len;
		return true;
	}

	bool Push(const char* pData_, uint32_t size_)
	{
		if (pData_ == nullptr)
		{
			return false;
		}

		if (size_ > MAX_PAYLOAD_SIZE)
		{
			return false;
		}

		if (size + size_ + sizeof(uint32_t) > capacity)
		{
			return false;
		}

		CopyMemory(data + size, &size_, sizeof(uint32_t));
		size += sizeof(uint32_t);
		CopyMemory(data + size, pData_, size_);
		size += size_;
		return true;
	}

	/// <summary>
	/// 직렬화버퍼 반환
	/// </summary>
	/// <returns></returns>
	const char* GetData() const { return data; }
	/// <summary>
	/// 실제 데이터의 크기
	/// </summary>
	/// <returns></returns>
	uint32_t GetSize() const { return size; }
	/// <summary>
	/// 직렬화버퍼의 크기
	/// </summary>
	/// <returns></returns>
	uint32_t GetCap() const { return capacity; }

private:
	char* data;
	uint32_t size;
	uint32_t capacity;
};

class DeserializeLib
{
public:
	/// <summary>
	/// data_의 크기가 len_과 일치하는지 반드시 확인할 것.
	/// 버퍼오버플로우가 발생할 수 있다.
	/// </summary>
	/// <param name="data_"></param>
	/// <param name="len_"></param>
	DeserializeLib(const char* data_, uint32_t len_) : data(nullptr), size(0), idx(0)
	{
		if (data_ != nullptr)
		{
			data = new(std::nothrow) char[len_];

			if (data != nullptr)
			{
				CopyMemory(data, data_, len_);
				size = len_;
			}
		}
	}

	~DeserializeLib()
	{
		if (data != nullptr)
		{
			delete[] data;
		}
	}

	template <typename T>
	typename std::enable_if<
		std::is_same<T, uint16_t>::value ||
		std::is_same<T, uint32_t>::value ||
		std::is_same<T, uint64_t>::value ||
		std::is_same<T, int16_t>::value ||
		std::is_same<T, int32_t>::value ||
		std::is_same<T, int64_t>::value ||
		std::is_same<T, float>::value ||
		std::is_same<T, double>::value ||
		std::is_same<T, std::string>::value ||
		std::is_same<T, std::string_view>::value,
		bool>::type
		Get(T& lhs_)
	{
		if (GetRemainSize() < sizeof(lhs_))
		{
			return false;
		}

		CopyMemory(&lhs_, data + idx, sizeof(lhs_));
		idx += sizeof(lhs_);

		//std::cout << "Serialization::DeserializeLib::Get : remainlen = " << GetRemainSize() << '\n';

		return true;
	}

	/// <summary>
	/// 길이 부분을 미리 체크하고 역직렬화를 시도한다.
	/// </summary>
	/// <param name="lhs_"></param>
	/// <returns></returns>
	template<>
	bool Get<std::string>(std::string& lhs_)
	{
		if (GetRemainSize() < sizeof(uint32_t) + lhs_.length())
		{
			return false;
		}

		uint32_t len{ 0 };

		CopyMemory(&len, data + idx, sizeof(uint32_t));

		if (len == 0 || len > MAX_STRING_SIZE || len + sizeof(uint32_t) > GetRemainSize())
		{
			return false;
		}

		idx += sizeof(uint32_t);

		lhs_.resize(len);
		CopyMemory(&(lhs_[0]), data + idx, len);
		idx += len;

		//std::cout << "Serialization::DeserializeLib::Get : remainlen = " << GetRemainSize() << '\n';

		return true;
	}

	/// <summary>
	/// 힙할당 비용을 감소시키기 위해 도입.
	/// </summary>
	/// <param name="lhs_"></param>
	/// <returns></returns>
	template<>
	bool Get<std::string_view>(std::string_view& lhs_)
	{
		if (GetRemainSize() < sizeof(uint32_t))
		{
			return false;
		}

		uint32_t len{ 0 };

		CopyMemory(&len, data + idx, sizeof(uint32_t));

		if (len == 0)
		{
			idx += sizeof(uint32_t);
			lhs_ = "";
			return true;
		}

		if (len > MAX_STRING_SIZE || len + sizeof(uint32_t) > GetRemainSize())
		{
			return false;
		}

		idx += sizeof(uint32_t);

		lhs_ = std::string_view(data + idx, len);

		idx += len;

		//std::cout << "Serialization::DeserializeLib::Get : remainlen = " << GetRemainSize() << '\n';

		return true;
	}

	uint32_t GetRemainSize() const { return size - idx; }

private:
	char* data;
	uint32_t size;
	uint32_t idx;
};

class Serializer
{
public:
	bool Serialize(const ResMessage& res_, std::string& out_)
	{
		SerializeLib slib;

		uint32_t size = sizeof(res_.reqNo) + sizeof(res_.resCode) + sizeof(uint32_t) + res_.payLoadSize;

		bool bRet = slib.Init(size) && 
				slib.Push(res_.reqNo) &&
				slib.Push(static_cast<int32_t>(res_.resCode)) &&
				slib.Push(res_.payLoad, res_.payLoadSize);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(ResMessage) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const CreateRoomRes& res_, std::string& out_)
	{
		SerializeLib slib;

		uint32_t size = sizeof(res_);

		bool bRet = slib.Init(size) &&
			slib.Push(res_.roomNum) &&
			slib.Push(res_.roomName, res_.roomNameLen) &&
			slib.Push(res_.password, res_.pwLen);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(CreateRoomRes) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const EnterRoomRes& res_, std::string& out_)
	{
		SerializeLib slib;

		uint32_t size = sizeof(res_);

		bool bRet = slib.Init(size) &&
			slib.Push(res_.roomName, res_.roomNameLen) &&
			slib.Push(res_.password, res_.pwLen);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(CreateRoomRes) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const ChatInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.userNum) &&
			slib.Push(info_.msg, info_.chatlen);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(ChatInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const RoomEnterInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.userNum) &&
			slib.Push(info_.nickname, info_.nicknameLen);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(RoomEnterInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const RoomExitInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.userNum);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(RoomExitInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const DrawInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.drawNum) &&
			slib.Push(info_.userNum) &&
			slib.Push(info_.drawPosX) && slib.Push(info_.drawPosY) &&
			slib.Push(info_.width) &&
			slib.Push(info_.r) && slib.Push(info_.g) && slib.Push(info_.b);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(DrawInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const DrawEndInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.drawNum) &&
			slib.Push(info_.userNum);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(DrawEndInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Serialize(const UndoInfo& info_, std::string& out_)
	{
		SerializeLib slib;
		uint32_t size = sizeof(info_);

		bool bRet = slib.Init(size) &&
			slib.Push(info_.drawNum) &&
			slib.Push(info_.userNum);

		if (!bRet)
		{
			std::cerr << "Serializer::Serialize(DrawEndInfo) : Failed.\n";
			return false;
		}

		out_.resize(slib.GetSize());
		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}

	bool Deserialize(const std::string& str_, ReqMessage& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		int32_t type{ 0 };
		std::string_view strview;

		bool bRet = dlib.Get(type) &&
				dlib.Get(out_.reqNo) &&
				dlib.Get(strview);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(ReqMessage) : Failed.\n";
			return false;
		}

		out_.reqType = static_cast<ReqType>(type);
		CopyMemory(out_.payLoad, strview.data(), strview.size());
		out_.payLoadSize = strview.size();

		return true;
	}

	bool Deserialize(const std::string& str_, CreateRoomParameter& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		std::string_view sv;

		bool bRet = dlib.Get(sv);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(CreateRoomParameter) : Failed.\n";
			return false;
		}

		out_.nameLen = sv.length();
		if (!sv.empty())
		{
			CopyMemory(out_.roomName, sv.data(), sv.length());
		}

		bRet = dlib.Get(sv) &&
			dlib.Get(out_.maxUsers);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(CreateRoomParameter) : Failed.\n";
			return false;
		}

		out_.pwLen = sv.length();
		if (!sv.empty())
		{
			CopyMemory(out_.password, sv.data(), sv.length());
		}

		return true;
	}

	bool Deserialize(const std::string& str_, SetNicknameParameter& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		std::string_view sv;

		bool bRet = dlib.Get(sv);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(CreateRoomParameter) : Failed.\n";
			return false;
		}

		out_.nameLen = sv.length();
		if (!sv.empty())
		{
			CopyMemory(out_.nickname, sv.data(), sv.length());
		}

		return true;
	}

	bool Deserialize(const std::string& str_, DrawParameter& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		bool bRet = dlib.Get(out_.drawNum) &&
			dlib.Get(out_.drawPosX) && dlib.Get(out_.drawPosY) &&
			dlib.Get(out_.width) &&
			dlib.Get(out_.r) && dlib.Get(out_.g) && dlib.Get(out_.b);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(DrawParameter) : Failed.\n";
			return false;
		}

		return true;
	}

	bool Deserialize(const std::string& str_, DrawEndParameter& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		bool bRet = dlib.Get(out_.drawNum);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(DrawEndParameter) : Failed.\n";
			return false;
		}


		return true;
	}

	bool Deserialize(const std::string& str_, EnterRoomParameter& out_)
	{
		DeserializeLib dlib(&(str_[0]), str_.length());

		std::string_view sv;

		bool bRet = dlib.Get(out_.roomNumber) &&
			dlib.Get(sv);

		if (!bRet)
		{
			std::cerr << "Serializer::Deserialize(DrawEndParameter) : Failed.\n";
			return false;
		}

		CopyMemory(out_.password, sv.data(), sv.length());
		out_.pwLen = sv.length();

		return true;
	}
};