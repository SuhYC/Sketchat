using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public class PacketMaker
{
    private static PacketMaker _instance;

    private float _drawLastSendTime = 0f;
    private float _sendInterval = 0.0f;

    public static PacketMaker Instance
    {
        get {
            if (_instance == null)
            {
                _instance = new PacketMaker();
            }
            return _instance;
        }
    }

    private uint _reqNo = 0;

    public async Task<bool> ReqCreateRoom(string roomName_, string pw_, ushort maxuser_)
    {
        if (maxuser_ < 1)
        {
            Debug.Log($"PacketMaker::ReqCreateRoom : maxUser Cannot Be Under Zero");
            return false;
        }

        Serializer.CreateRoomParameter stParam = new Serializer.CreateRoomParameter();

        stParam.name = Encoding.GetEncoding("euc-kr").GetBytes(roomName_);

        if (stParam.name.Length < 1 || stParam.name.Length > Serializer.MAX_ROOM_NAME_LEN)
        {
            Debug.Log($"PacketMaker::ReqCreateRoom : Invalid name len : {stParam.name.Length}");
            return false;
        }

        stParam.pw = Encoding.GetEncoding("euc-kr").GetBytes(pw_);

        if (stParam.pw.Length > Serializer.MAX_ROOM_NAME_LEN)
        {
            Debug.Log($"PacketMaker::ReqCreateRoom : Invalid pw len : {stParam.pw.Length}");
            return false;
        }


        stParam.nameLen = (ushort)stParam.name.Length;
        stParam.pwLen = (ushort)stParam.pw.Length;
        stParam.maxUsers = maxuser_;

        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage stReq = new Serializer.ReqMessage();

        stReq.payLoad = null;

        bool bRet = serializer.Serialize(stParam, ref stReq.payLoad);
        if (!bRet || stReq.payLoad.Length > Serializer.MAX_PACKET_SIZE)
        {
            Debug.Log($"PacketMaker::ReqCreateRoom : Failed to Serialize.");
            return false;
        }
        stReq.payLoadSize = (uint)stReq.payLoad.Length;
        stReq.type = Serializer.ReqType.CREATE_ROOM;
        stReq.reqNo = ++_reqNo;

        byte[] req = null;

        bRet = serializer.Serialize(stReq, ref req);
        if(!bRet)
        {
            Debug.Log($"PacketMaker::ReqCreateRoom : Failed to Make Req.");
            return false;
        }

        NetworkManager.Instance.PushReq(stReq);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqSetNickname(string nickname)
    {
        if(nickname == null || nickname.Length < 1)
        {
            return false;
        }

        Serializer.SetNicknameParameter stParam = new Serializer.SetNicknameParameter();

        stParam.nickName = Encoding.GetEncoding("euc-kr").GetBytes(nickname);

        if(nickname.Length > Serializer.MAX_ROOM_NAME_LEN)
        {
            return false;
        }

        stParam.nickNameLen = (ushort)stParam.nickName.Length;

        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);

        Serializer.ReqMessage reqMsg = new Serializer.ReqMessage();

        bool bRet = serializer.Serialize(stParam, ref reqMsg.payLoad);

        if(!bRet || reqMsg.payLoad.Length > Serializer.MAX_PACKET_SIZE)
        {
            return false;
        }
        reqMsg.payLoadSize = (uint)reqMsg.payLoad.Length;
        reqMsg.type = Serializer.ReqType.SET_NICKNAME;
        reqMsg.reqNo = ++_reqNo;

        byte[] req = null;

        bRet = serializer.Serialize(reqMsg, ref req);
        if (!bRet)
        {
            Debug.Log($"PacketMaker::ReqSetNickname : Failed to Make Req.");
            return false;
        }

        NetworkManager.Instance.PushReq(reqMsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqDrawStart()
    {
        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        reqmsg.payLoadSize = 0;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.DRAW_START;

        byte[] req = null;

        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqDraw(ushort drawNum, Vector2Int pos, float width, Color color)
    {
        if(pos == null || color == null)
        {
            return false;
        }

        if(_drawLastSendTime + _sendInterval > Time.time)
        {
            return false;
        }

        _drawLastSendTime = Time.time;

        Serializer.DrawParameter stParam = new Serializer.DrawParameter();

        stParam.drawNum = drawNum;
        stParam.drawPosX = (short)pos.x; stParam.drawPosY = (short)pos.y;
        stParam.r = (byte)(color.r * 255); stParam.g = (byte)(color.g * 255); stParam.b = (byte)(color.b * 255);
        stParam.width = width;

        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        if(!serializer.Serialize(stParam, ref reqmsg.payLoad) || reqmsg.payLoad.Length > Serializer.MAX_PACKET_SIZE)
        {
            return false;
        }

        reqmsg.payLoadSize = (uint)reqmsg.payLoad.Length;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.DRAW;

        byte[] req = null;

        if(!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqDrawEnd(ushort drawNum)
    {
        Serializer.DrawEndParameter stParam = new Serializer.DrawEndParameter();

        stParam.drawNum = drawNum;

        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        if(!serializer.Serialize(stParam, ref reqmsg.payLoad) || reqmsg.payLoad.Length > Serializer.MAX_PACKET_SIZE)
        {
            return false;
        }

        reqmsg.payLoadSize = (uint)reqmsg.payLoad.Length;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.DRAW_END;

        byte[] req = null;
        if(!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqCutTheLine(ushort drawNum)
    {
        Serializer.DrawEndParameter stParam = new Serializer.DrawEndParameter();

        stParam.drawNum = drawNum;

        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        if (!serializer.Serialize(stParam, ref reqmsg.payLoad) || reqmsg.payLoad.Length > Serializer.MAX_PACKET_SIZE)
        {
            return false;
        }

        reqmsg.payLoadSize = (uint)reqmsg.payLoad.Length;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.CUT_THE_LINE;

        byte[] req = null;
        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqUndo()
    {
        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        reqmsg.payLoadSize = 0;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.UNDO;

        byte[] req = null;
        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqRoomInfo()
    {
        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);
        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        reqmsg.payLoadSize = 0;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.REQ_ROOM_INFO;

        byte[] req = null;
        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqEnterRoom(ushort roomNum_, string pw_)
    {
        byte[] pwBytes = Encoding.GetEncoding("euc-kr").GetBytes(pw_);

        if(pwBytes.Length > Serializer.MAX_ROOM_NAME_LEN)
        {
            return false;
        }

        Serializer.EnterRoomParameter stParam = new Serializer.EnterRoomParameter();

        stParam.pwLen = (ushort)pwBytes.Length;
        stParam.roomnumber = roomNum_;

        if(stParam.pwLen != 0)
        {
            stParam.password = new byte[stParam.pwLen];
            Array.Copy(pwBytes, stParam.password, pwBytes.Length);
        }
        
        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);

        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        if(!serializer.Serialize(stParam, ref reqmsg.payLoad))
        {
            return false;
        }

        reqmsg.payLoadSize = (ushort)reqmsg.payLoad.Length;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.ENTER_ROOM;

        byte[] req = null;
        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }

    public async Task<bool> ReqRoomCanvasInfo()
    {
        Serializer serializer = new Serializer(Serializer.ActivationMode.SLIB_ONLY);

        Serializer.ReqMessage reqmsg = new Serializer.ReqMessage();

        reqmsg.payLoadSize = 0;
        reqmsg.reqNo = ++_reqNo;
        reqmsg.type = Serializer.ReqType.REQ_CANVAS_INFO;

        byte[] req = null;
        if (!serializer.Serialize(reqmsg, ref req))
        {
            return false;
        }

        NetworkManager.Instance.PushReq(reqmsg);
        await NetworkManager.Instance.SendMsg(req, (uint)req.Length);

        return true;
    }
}
