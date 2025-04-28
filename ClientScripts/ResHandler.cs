using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Threading.Tasks;
using System.Net.Http;
using System.Security.Cryptography;
using System;
using static Serializer;
using UnityEngine.SceneManagement;
using System.Text;
public class ResHandler
{
    private Serializer _serializer;

    private delegate Task ResHandleFunc(ReqMessage reqmsg_, ResMessage resmsg_);
    private delegate void InfoHandleFunc(ResMessage resmsg_);

    private SortedDictionary<uint, ReqMessage> _reqmsgs;

    private Hashtable _resHandleFuncs;
    private Hashtable _infoHandleFuncs;


    public void Init()
    {
        _serializer = new Serializer(ActivationMode.DLIB_ONLY);

        _reqmsgs = new SortedDictionary<uint, ReqMessage> ();
        _resHandleFuncs = new Hashtable();
        _infoHandleFuncs = new Hashtable();

        // ----- res -----
        ResHandleFunc CreateRoom = HandleCreateRoomResponse;
        _resHandleFuncs.Add(ReqType.CREATE_ROOM, CreateRoom);

        ResHandleFunc SetNickName = HandleSetNicknameResponse;
        _resHandleFuncs.Add(ReqType.SET_NICKNAME, SetNickName);

        ResHandleFunc DrawStart = HandleDrawStartResponse;
        _resHandleFuncs.Add(ReqType.DRAW_START, DrawStart);

        ResHandleFunc Draw = HandleDrawResponse;
        _resHandleFuncs.Add(ReqType.DRAW, Draw);

        ResHandleFunc DrawEnd = HandleDrawEndResponse;
        _resHandleFuncs.Add(ReqType.DRAW_END, DrawEnd);

        ResHandleFunc Undo = HandleUndoResponse;
        _resHandleFuncs.Add(ReqType.UNDO, Undo);

        ResHandleFunc ReqRoomInfo = HandleReqRoomInfoResponse;
        _resHandleFuncs.Add(ReqType.REQ_ROOM_INFO, ReqRoomInfo);

        ResHandleFunc EnterRoom = HandleEnterRoomResponse;
        _resHandleFuncs.Add(ReqType.ENTER_ROOM, EnterRoom);

        // ----- info -----
        InfoHandleFunc Chat = HandleChat;
        _infoHandleFuncs.Add(InfoType.CHAT, Chat);

        InfoHandleFunc EnterInfo = HandleUserEnterInfo;
        _infoHandleFuncs.Add(InfoType.ROOM_USER_ENTERED, EnterInfo);

        InfoHandleFunc ExitInfo = HandleUserExitInfo;
        _infoHandleFuncs.Add(InfoType.ROOM_USER_EXITED, ExitInfo);

        InfoHandleFunc DrawInfo = HandleDrawInfo;
        _infoHandleFuncs.Add(InfoType.DRAW, DrawInfo);

        InfoHandleFunc DrawEndInfo = HandleDrawEndInfo;
        _infoHandleFuncs.Add(InfoType.DRAW_END, DrawEndInfo);

        InfoHandleFunc UndoInfo = HandleUndoInfo;
        _infoHandleFuncs.Add(InfoType.UNDO, UndoInfo);

        InfoHandleFunc RoomCanvasInfo = HandleRoomCanvasInfo;
        _infoHandleFuncs.Add(InfoType.ROOM_CANVAS_INFO, RoomCanvasInfo);
    }

    public void PushReqMessage(ReqMessage req_)
    {
        if(!_reqmsgs.TryAdd(req_.reqNo, req_))
        {
            Debug.Log($"ResHandler::PushReqMessage : Already Used ReqNo.");
        }
        return;
    }

    public async Task HandleServerResponse(byte[] bytes_, uint size_)
    {
        ResMessage resMsg = new ResMessage();

        if(!_serializer.Deserialize(bytes_, size_, ref resMsg))
        {
            Debug.Log($"ResHandler::HandleServerResponse : Failed to Deserialize.");
            return;
        }
        
        // res msg
        if(resMsg.reqNo != 0)
        {
            ReqMessage reqMsg;
            if(!_reqmsgs.TryGetValue(resMsg.reqNo, out reqMsg))
            {
                Debug.Log($"ResHandler::HandleServerResponse : Invalid ReqNo.");
                return;
            }

            try
            {
                if(_resHandleFuncs.ContainsKey(reqMsg.type) && _resHandleFuncs[reqMsg.type] != null)
                {
                    await ((ResHandleFunc)_resHandleFuncs[reqMsg.type]).Invoke(reqMsg, resMsg);
                }
                else
                {
                    Debug.Log($"");
                }
            }
            catch(InvalidCastException e)
            {
                Debug.Log($"ResHandler::HandleServerResponse : Not A Delegate In _resHandleFuncs, {e.Message}");
            }
            catch (Exception e)
            {
                Debug.Log($"ResHandler::HandleServerResponse : {e.Message}");
                Debug.Log($"ResHandler::HandleServerResponse : stack trace : {e.StackTrace}");
            }

            _reqmsgs.Remove(resMsg.reqNo);
        }
        // info msg
        else
        {
            try
            {
                //Debug.Log($"ResHandler::HandleServerResponse : rescode : {(int)resMsg.resCode}");
                if (_infoHandleFuncs[resMsg.resCode] != null)
                {
                    ((InfoHandleFunc)_infoHandleFuncs[resMsg.resCode]).Invoke(resMsg);
                }
            }
            catch (InvalidCastException e)
            {
                Debug.Log($"ResHandler::HandleServerResponse : Not A Delegate In _infoHandleFuncs, {e.Message}");
            }
            catch (Exception e)
            {
                Debug.Log($"ResHandler::HandleServerResponse : {e.Message}");
                Debug.Log($"ResHandler::HandleServerResponse : stack trace : {e.StackTrace}");
            }
        }

        return;
    }

    private async Task HandleCreateRoomResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask;

        switch (resmsg_.resCode)
        {
            case InfoType.REQ_SUCCESS:
                
                CreateRoomRes res = new CreateRoomRes();
                if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref res))
                {
                    Debug.Log($"ResHandler::HandleCreateRoomRes : Failed to Deserialize.");
                    return;
                }

                string roomName = Encoding.GetEncoding("euc-kr").GetString(res.roomName, 0, res.roomNameLen);

                if(res.pwLen != 0)
                {
                    string pw = Encoding.GetEncoding("euc-kr").GetString(res.password, 0, res.pwLen);
                    UserData.Instance.SetRoomPW(pw);
                }
                else
                {
                    UserData.Instance.SetRoomPW("");
                }
                UserData.Instance.SetRoomName(roomName);



                SceneManager.LoadScene("DrawScene");

                break;

            default:

                break;
        }
    }

    private async Task HandleReqRoomInfoResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask;

        switch(resmsg_.resCode)
        {
            case InfoType.REQ_SUCCESS:
                Serializer.RoomInfos stParam = new Serializer.RoomInfos();

                if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
                {
                    Debug.Log($"ResHandler::HandleReqRoomInfoResponse : failed to Serialize.");
                    return;
                }

                RoomPanel.Instance.Init();

                for(int i = 0; i < stParam.RoomCount; i++)
                {
                    string roomName = Encoding.GetEncoding("euc-kr").GetString(stParam.infos[i].roomName, 0, stParam.infos[i].roomNameLen);

                    RoomPanel.Instance.Add(stParam.infos[i].roomNum, roomName, stParam.infos[i].nowUsers, stParam.infos[i].maxUsers);
                }
                break;
            case InfoType.REQ_FAILED:

                break;

            default:

                break;
        }

    }

    private async Task HandleEnterRoomResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask;

        switch(resmsg_.resCode)
        {
            case InfoType.REQ_SUCCESS:

                EnterRoomRes res = new EnterRoomRes();

                if (!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref res))
                {
                    Debug.Log($"ResHandler::HandleCreateRoomRes : Failed to Deserialize.");
                    return;
                }

                string roomName = Encoding.GetEncoding("euc-kr").GetString(res.roomName, 0, res.roomNameLen);

                if (res.pwLen != 0)
                {
                    string pw = Encoding.GetEncoding("euc-kr").GetString(res.password, 0, res.pwLen);
                    UserData.Instance.SetRoomPW(pw);
                }
                else
                {
                    UserData.Instance.SetRoomPW("");
                }
                UserData.Instance.SetRoomName(roomName);

                SceneManager.LoadScene("DrawScene");

                break;

            case InfoType.REQ_FAILED:
                break;
            default:
                break;
        }
    }

    private async Task HandleSetNicknameResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask;

        await PacketMaker.Instance.ReqRoomInfo();

        return;
    }

    private async Task HandleDrawStartResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask; return;
    }
    private async Task HandleDrawResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask;

        return;
    }

    private async Task HandleDrawEndResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    { 
        await Task.CompletedTask;

        return;
    }

    private async Task HandleUndoResponse(ReqMessage reqmsg_, ResMessage resmsg_)
    {
        await Task.CompletedTask; 

        switch(resmsg_.resCode)
        {
            case InfoType.REQ_FAILED:
                // -- 텍스트 메시지를 띄울까?
                break;
        }

        return;
    }

    private void HandleChat(ResMessage resmsg_)
    {
        ChatInfo stParam = new ChatInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            return;
        }

        string msg = Encoding.GetEncoding("euc-kr").GetString(stParam.chat, 0, stParam.chatLen);

        UserPanel.Instance.UserChat(stParam.userNum, msg);
    }

    private void HandleUserEnterInfo(ResMessage resmsg_)
    {
        RoomEnterInfo stParam = new RoomEnterInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            return;
        }

        string username = Encoding.GetEncoding("euc-kr").GetString(stParam.nickname, 0, stParam.nicknameLen);

        UserPanel.Instance.UserEnter(stParam.userNum, username);
    }

    private void HandleUserExitInfo(ResMessage resmsg_)
    {
        RoomExitInfo stParam = new RoomExitInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            return;
        }

        UserPanel.Instance.UserExit(stParam.userNum);
    }

    private void HandleDrawInfo(ResMessage resmsg_)
    {
        DrawInfo stParam = new DrawInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            Debug.Log($"ResHandler::HandleDrawInfo : Failed to Serialize.");
            return;
        }

        uint key = ((uint)stParam.userNum << 16) | stParam.drawNum;
        Vector2Int vertex = new Vector2Int(stParam.drawPosX, stParam.drawPosY);
        Color color = new Color(stParam.r, stParam.g, stParam.b, 1.0f);

        CommandStack.Instance.AddVertexToCommand(key, vertex, color, stParam.width);

        return;
    }

    private void HandleDrawEndInfo(ResMessage resmsg_)
    {
        DrawEndInfo stParam = new DrawEndInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad,resmsg_.payLoadSize,ref stParam))
        {
            Debug.Log($"ResHandler::HandleDrawEndInfo : Failed to Serialize.");
            return;
        }

        uint key = ((uint)stParam.userNum << 16) | stParam.drawNum;

        CommandStack.Instance.Push(key);

        return;
    }

    private void HandleUndoInfo(ResMessage resmsg_)
    {
        UndoInfo stParam = new UndoInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            Debug.Log($"ResHandler::HandleUndoInfo : Failed to Serialize.");
            return;
        }

        uint key = ((uint)stParam.userNum << 16) | stParam.drawNum;

        CommandStack.Instance.Undo(key);
    }

    private void HandleRoomCanvasInfo(ResMessage resmsg_)
    {
        RoomCanvasInfo stParam = new RoomCanvasInfo();

        if(!_serializer.Deserialize(resmsg_.payLoad, resmsg_.payLoadSize, ref stParam))
        {
            Debug.Log($"ResHandler::HandleRoomCanvasInfo : Failed to Serialize.");
            return;
        }

        CommandStack.Instance.Init();
        CommandStack.Instance.SetInitTexture(stParam.pixels);

        for(int commandidx = 0; commandidx < stParam.CountOfCommands; commandidx++)
        {
            uint drawkey = stParam.drawkeys[commandidx];
            for(int vertexidx = 0; vertexidx < stParam.CommandInfos[commandidx].vertices.Length; vertexidx++)
            {
                CommandStack.Instance.AddVertexToCommand(drawkey, stParam.CommandInfos[commandidx].vertices[vertexidx], stParam.CommandInfos[commandidx].DrawColor, stParam.CommandInfos[commandidx].DrawWidth);
            }

            CommandStack.Instance.Push(drawkey);
        }
    }
}
