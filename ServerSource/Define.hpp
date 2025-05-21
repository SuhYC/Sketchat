#pragma once

#include <WinSock2.h>
#include <MSWSock.h> // AcceptEx()
#include <WS2tcpip.h>
#include <iostream>
#include <exception>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib") // acceptEx()

const unsigned int MAX_SOCKBUF = 4096;
const unsigned short MAX_WORKTHREAD = 4;
const unsigned short MAX_JOBTHREAD = 4;

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