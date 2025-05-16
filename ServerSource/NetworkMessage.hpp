#pragma once

#include <cstdint>
#include <string>

/*
* Ŭ���̾�Ʈ -> ���� �޽��� ����
* [Size : uint32_t][ReqType : int32_t][ReqNo : uint32_t][PayLoad]
* 
* ���� -> Ŭ���̾�Ʈ �޽��� ����
* [PayLoadSize : uint32_t][ReqNo : uint32_t][ResCode : int32_t][PayLoad]
*/

const uint32_t MAX_PAYLOAD_SIZE = 2036;
const uint32_t HEADER_SIZE = 12;
const uint32_t MAX_ROOM_NAME_LEN = 12;
const uint32_t MAX_CHATTING_LEN = 80;

/// <summary>
/// Ŭ���̾�Ʈ -> ������ ��û�ϴ� ���
/// </summary>
enum class ReqType
{
	CREATE_ROOM,
	ENTER_ROOM,
	REQ_ROOM_INFO,
	EXIT_ROOM,
	EDIT_ROOM_SETTING,
	SET_NICKNAME,
	DRAW_START,
	DRAW,
	DRAW_END,
	UNDO,
	CHAT,

	LAST = CHAT // ��� �߰��� �ٽ� �Է��� ��. ���� ������ enum�� �����ؾ��Ѵ�.
};

/// <summary>
/// ���� -> Ŭ���̾�Ʈ�� �����ϴ� ����
/// </summary>
enum class InfoType
{
	REQ_SUCCESS,
	REQ_FAILED,
	ROOM_INFO,
	ROOM_SETTING_EDITED,
	ROOM_USER_ENTERED,
	ROOM_USER_EXITED,
	ROOM_CANVAS_INFO,
	DRAW,
	DRAW_END,
	UNDO,
	CHAT,
	NOT_FINISHED // ��ǻ� ���������� ����. Job�� ������ �ʾƼ� �Ķ������ ���¸� �״�� �ٽ� ť���Ѵ�.
};

/// <summary>
/// 
/// </summary>
struct ReqMessage
{
	uint32_t payLoadSize;
	ReqType reqType;
	uint32_t reqNo;
	char payLoad[MAX_PAYLOAD_SIZE];
};

struct ResMessage
{
	uint32_t payLoadSize;
	uint32_t reqNo;
	int32_t resCode;
	char payLoad[MAX_PAYLOAD_SIZE];
};

/*
* �� ������
* 1. �̸�
* 2. ��й�ȣ
* 3. �ִ��ο�
*/

struct CreateRoomParameter
{
	uint16_t nameLen;
	char roomName[MAX_ROOM_NAME_LEN];
	uint16_t pwLen;
	char password[MAX_ROOM_NAME_LEN];
	uint16_t maxUsers;
};

struct EnterRoomParameter
{
	uint16_t roomNumber;
	uint16_t pwLen;
	char password[MAX_ROOM_NAME_LEN];
};

// ExitRoomParameter << ������ �Ű����� �ʿ����.

struct EditRoomSettingParameter
{
	uint16_t nameLen;
	char roomName[MAX_ROOM_NAME_LEN];
	uint16_t pwLen;
	char password[MAX_ROOM_NAME_LEN];
	uint16_t maxUsers;
};

struct SetNicknameParameter
{
	uint16_t nameLen;
	char nickname[MAX_ROOM_NAME_LEN];
};

// DrawStartParameter << �Ű����� ���� ���ʿ���.

struct DrawParameter
{
	uint16_t drawNum; // �ش� ������ ���° �׸���û���� (�巡�� �߿��� ���� ��û���� �����Ѵ�.)
	int16_t drawPosX;
	int16_t drawPosY;
	float width;
	char r;
	char g;
	char b;
	//float a; // a���� �Ⱦ��Ű���.
};

struct DrawEndParameter
{
	uint16_t drawNum;
};

struct CreateRoomRes
{
	uint16_t roomNum;
	uint16_t roomNameLen;
	char roomName[MAX_ROOM_NAME_LEN];
	uint16_t pwLen;
	char password[MAX_ROOM_NAME_LEN];

};

struct EnterRoomRes
{
	uint16_t roomNameLen;
	char roomName[MAX_ROOM_NAME_LEN];
	uint16_t pwLen;
	char password[MAX_ROOM_NAME_LEN];
};

/// <summary>
/// [RoomCount :ushort][RoomInfos....]
/// </summary>
struct RoomInfo
{
	uint16_t roomNum;
	uint16_t roomNameLen;
	char roomName[MAX_ROOM_NAME_LEN];
	uint16_t nowUsers;
	uint16_t maxUsers;
};

struct RoomDestroyInfo
{
	uint16_t roomNum;
};

struct RoomEnterInfo
{
	uint16_t userNum;
	uint16_t nicknameLen;
	char nickname[MAX_ROOM_NAME_LEN];
};

struct RoomExitInfo
{
	uint16_t userNum;
};

/// <summary>
/// Ŭ���̾�Ʈ������ �ش� ���� ������ �ֵ�,
/// drawEnd�� �߻����� �ʰ� ���������� ���� �����ʹ� �˾Ƽ� �� �̾ �������� ǥ�����ش�.
/// ��� ���� �� ����ϱ⿡�� �����Ͱ� �ʹ� ���� ������ Ŭ���̾�Ʈ���� �˾Ƽ� �����Ӵ����� ��ǥ�� �����ϰ� �ȴ�.
/// ������ ���̿� ���콺��ǥ�� ��ŵ�Ǵ� ��찡 �߻��ϰ� �Ǵµ�,
/// �� ��ŵ�� ��ǥ�� �������� �̾ ǥ�����ָ� �ȴ�.
/// </summary>
struct DrawInfo
{
	uint16_t drawNum;
	uint16_t userNum; // Ŭ���̾�Ʈ�� �ڽ��� ��� �������� �� �ʿ䰡 ����. ������ �˾Ƽ� ó���ؼ� ������ ������ ������ �־ �ȴ�.
	int16_t drawPosX;
	int16_t drawPosY;
	float width;
	char r;
	char g;
	char b;
};

struct DrawEndInfo
{
	uint16_t drawNum;
	uint16_t userNum;
};

// ReqUndoParameter << ���� �Ű����� �ʿ����.

/// <summary>
/// �ش� ������ Ŭ���̾�Ʈ�� �����ϸ�,
/// ������ DrawInfo�� ������ �����̳ʿ��� �ش� ��ȣ�� DrawInfo�� �����ϰ�,
/// �ʱ���·κ��� ���ŵ��� ���� DrawInfo�� ���ʴ�� �����Ͽ� ĵ������ �籸���Ѵ�.
/// </summary>
struct UndoInfo
{
	uint16_t drawNum;
	uint16_t userNum;
};

// ChatParameter << PayLoad ��ü�� ä�ó���

struct ChatInfo
{
	uint16_t userNum;
	uint16_t chatlen;
	char msg[MAX_CHATTING_LEN];
};