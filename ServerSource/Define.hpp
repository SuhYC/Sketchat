#pragma once

#include <WinSock2.h>
#include <MSWSock.h> // AcceptEx()
#include <WS2tcpip.h>
#include <iostream>
#include <exception>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib") // acceptEx()

const unsigned short MAX_SOCKBUF = 1000;
const unsigned short MAX_WORKTHREAD = 4;

enum class eIOOperation
{
	RECV,
	SEND,
	ACCEPT
};

struct stOverlappedEx
{
	WSAOVERLAPPED m_overlapped;
	unsigned short m_userIndex;
	WSABUF m_wsaBuf;
	eIOOperation m_eOperation;
};

class ToJsonException : public std::exception
{
private:
	std::string message;

public:
	ToJsonException(const std::string& msg) : message(msg)
	{

	}

	const char* what() const noexcept override
	{
		return message.c_str();
	}
};