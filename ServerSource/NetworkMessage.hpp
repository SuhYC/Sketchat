#pragma once

#include <cstdint>
#include <string>

/*
* 클라이언트 -> 서버 메시지 구성
* [Size : uint32_t][ReqType : int32_t][ReqNo : uint32_t][PayLoad]
* 
* 서버 -> 클라이언트 메시지 구성
* [PayLoadSize : uint32_t][ReqNo : uint32_t][ResCode : int32_t][PayLoad]
*/

const uint32_t MAX_PAYLOAD_SIZE = 2036;
const uint32_t HEADER_SIZE = 12;
const uint32_t MAX_ROOM_NAME_LEN = 12;
const uint32_t MAX_CHATTING_LEN = 80;

/// <summary>
/// 클라이언트 -> 서버로 요청하는 경우
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

	LAST = CHAT // 기능 추가시 다시 입력할 것. 가장 마지막 enum을 지정해야한다.
};

/// <summary>
/// 서버 -> 클라이언트로 전파하는 정보
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
	NOT_FINISHED // 사실상 서버에서만 수행. Job이 끝나지 않아서 파라미터의 상태를 그대로 다시 큐잉한다.
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
* 방 설정값
* 1. 이름
* 2. 비밀번호
* 3. 최대인원
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

// ExitRoomParameter << 별도의 매개변수 필요없음.

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

// DrawStartParameter << 매개변수 딱히 안필요함.

struct DrawParameter
{
	uint16_t drawNum; // 해당 유저의 몇번째 그림요청인지 (드래그 중에는 같은 요청으로 구분한다.)
	int16_t drawPosX;
	int16_t drawPosY;
	float width;
	char r;
	char g;
	char b;
	//float a; // a값은 안쓸거같음.
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
/// 클라이언트에서는 해당 값을 가지고 있되,
/// drawEnd가 발생하지 않고 연속적으로 오는 데이터는 알아서 잘 이어서 직선으로 표현해준다.
/// 모든 점을 다 기록하기에는 데이터가 너무 많기 때문에 클라이언트에서 알아서 프레임단위로 좌표를 전송하게 된다.
/// 프레임 사이에 마우스좌표가 스킵되는 경우가 발생하게 되는데,
/// 이 스킵된 좌표를 직선으로 이어서 표현해주면 된다.
/// </summary>
struct DrawInfo
{
	uint16_t drawNum;
	uint16_t userNum; // 클라이언트는 자신이 몇번 유저인지 알 필요가 없다. 서버가 알아서 처리해서 가공한 정보를 가지고만 있어도 된다.
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

// ReqUndoParameter << 별도 매개변수 필요없음.

/// <summary>
/// 해당 인포가 클라이언트에 도달하면,
/// 수신한 DrawInfo를 저장한 컨테이너에서 해당 번호의 DrawInfo를 제거하고,
/// 초기상태로부터 제거되지 않은 DrawInfo를 차례대로 실행하여 캔버스를 재구성한다.
/// </summary>
struct UndoInfo
{
	uint16_t drawNum;
	uint16_t userNum;
};

// ChatParameter << PayLoad 자체가 채팅내용

struct ChatInfo
{
	uint16_t userNum;
	uint16_t chatlen;
	char msg[MAX_CHATTING_LEN];
};