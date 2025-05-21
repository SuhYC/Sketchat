using System;
using System.Collections;
using System.Collections.Generic;
using System.Data.SqlTypes;
using System.Text;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.UIElements;
using UnityEngine.XR;

public class Serializer
{
    public const int MAX_STRING_SIZE = 1000;
    public const int MAX_PACKET_SIZE = NetworkManager.IO_BUF_SIZE;
    public const int MAX_ROOM_NAME_LEN = 12;
    public const int MAX_CHATTING_LEN = 80;

    public const int CANVAS_INFO_CHUNK_COUNT_OF_PIXELS = 2048;

    public enum ActivationMode
    {
        DLIB_ONLY,
        SLIB_ONLY,
        BOTH_TYPE
    }

    private DeserializeLib dlib;
    private SerializeLib slib;

    public enum ReqType
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
        REQ_CANVAS_INFO,
        CHAT,

        LAST = CHAT // 기능 추가시 다시 입력할 것. 가장 마지막 enum을 지정해야한다.
    };

    public enum InfoType
    {
        REQ_SUCCESS,
        REQ_FAILED,
        ROOM_INFO,
        ROOM_SETTING_EDITED,
        ROOM_USER_ENTERED,
        ROOM_USER_EXITED,
        ROOM_CANVAS_INFO,
        ROOM_DRAWCOMMAND_INFO,
        DRAW,
        DRAW_END,
        UNDO,
        CHAT,
        NOT_FINISHED // 사실상 서버에서만 수행. Job이 끝나지 않아서 파라미터의 상태를 그대로 다시 큐잉한다.
    };

    public struct ReqMessage
    {
        public uint payLoadSize;
        public ReqType type;
        public uint reqNo;
        public byte[] payLoad;
    }

    public struct ResMessage
    {
        public uint payLoadSize;
        public uint reqNo;
        public InfoType resCode;
        public byte[] payLoad;

        public ResMessage(bool init = true)
        {
            payLoadSize = 0;
            reqNo = 0;
            resCode = 0;
            payLoad = new byte[MAX_PACKET_SIZE - 12];
        }
    }

    public struct CreateRoomParameter
    {
        public ushort nameLen;
        public byte[] name;
        public ushort pwLen;
        public byte[] pw;
        public ushort maxUsers;
    }

    public struct EnterRoomParameter
    {
        public ushort roomnumber;
        public ushort pwLen;
        public byte[] password;
    }

    public struct CreateRoomRes
    {
        public ushort roomNum;
        public ushort roomNameLen;
        public byte[] roomName;
        public ushort pwLen;
        public byte[] password;
    }

    public struct EnterRoomRes
    {
        public ushort roomNameLen;
        public byte[] roomName;
        public ushort pwLen;
        public byte[] password;
    }


    public struct SetNicknameParameter
    {
        public ushort nickNameLen;
        public byte[] nickName;
    }

    public struct DrawParameter
    {
        public ushort drawNum;
        public short drawPosX;
        public short drawPosY;
        public float width;
        public byte r;
        public byte g;
        public byte b;
    }

    public struct DrawEndParameter
    {
        public ushort drawNum;
    }

    public struct RoomEnterInfo
    {
        public ushort userNum;
        public ushort nicknameLen;
        public byte[] nickname;
    }

    public struct RoomExitInfo
    {
        public ushort userNum;
    }

    public struct ChatInfo
    {
        public ushort userNum;
        public ushort chatLen;
        public byte[] chat;
    }

    public struct DrawInfo
    {
        public ushort drawNum;
        public ushort userNum;
        public short drawPosX;
        public short drawPosY;
        public float width;
        public float r;
        public float g;
        public float b;
    }

    public struct DrawEndInfo
    {
        public ushort drawNum;
        public ushort userNum;
    }

    public struct UndoInfo
    {
        public ushort drawNum;
        public ushort userNum;
    }

    public struct RoomInfos
    {
        public ushort RoomCount;
        public RoomInfo[] infos;
    }

    public struct RoomInfo
    {
        public ushort roomNum;
        public ushort roomNameLen;
        public byte[] roomName;
        public ushort nowUsers;
        public ushort maxUsers;
    }

    /// <summary>
    /// [count of pixels:uint][color * count : float * 3 * count][count of Commands:ushort][Command infos]
	/// [Command info] : [Color : float * 3][width : float][vertices count:uint][[vector2int : int32_t * 2] * vertices count]
    /// </summary>
    public struct RoomCanvasInfo
    {
        public ushort chunkidx;
        public Color32[] pixels;
    }

    public struct RoomDrawCommandInfo
    {
        public uint[] drawkeys;
        public ushort CountOfCommands;
        public DrawCommand[] CommandInfos;
    }

    public class SerializeLib
    {
        public SerializeLib()
        {
            data = null;
            size = 0;
            capacity = 0;
        }

        public bool Init(int cap_)
        {
            if(cap_ < size)
            {
                return false;
            }

            if(cap_ == capacity)
            {
                return true;
            }

            try
            {
                data = new byte[cap_];
            }
            catch (Exception e)
            {
                Console.WriteLine($"SerializeLib::Init : {e.Message}");
                return false;
            }

            capacity = cap_;
            return true;
        }

        public void Flush()
        {
            size = 0;
        }

        public bool Resize(int cap_)
        {
            if (size > cap_)
            {
                return false;
            }

            if (capacity == cap_)
            {
                return true;
            }

            byte[] newData;

            try
            {
                newData = new byte[cap_];
            }
            catch (Exception e)
            {
                Console.WriteLine($"SerializeLib::Resize : {e.Message}");
                return false;
            }

            if (data != null)
            {
                Buffer.BlockCopy(data, 0, newData, 0, (int)size);
            }

            data = newData;
            capacity = cap_;

            return true;
        }

        unsafe public bool Push<T>(T rhs_) where T : unmanaged
        {
            if (typeof(T) == typeof(int) || typeof(T) == typeof(uint) ||
                typeof(T) == typeof(float) || typeof(T) == typeof(double) ||
                typeof(T) == typeof(ulong) || typeof(T) == typeof(long) ||
                typeof(T) == typeof(short) || typeof(T) == typeof(ushort) || typeof(T) == typeof(byte))
            {
                int tSize = System.Runtime.InteropServices.Marshal.SizeOf<T>();

                if (size + tSize > capacity)
                {
                    return false;
                }

                fixed (byte* pBuffer = data)
                {
                    T* pT = (T*)(pBuffer + size);
                    *pT = rhs_;
                }

                size += tSize;

                return true;
            }
            else
            {
                throw new ArgumentException($"Unsupported type : {typeof(T)}");
            }
        }

        public bool Push(string rhs_)
        {
            byte[] bytes = Encoding.GetEncoding("euc-kr").GetBytes(rhs_);

            uint len = (uint)bytes.Length;

            if (size + len + sizeof(uint) > capacity)
            {
                return false;
            }

            data[size] = (byte)(len & 0xFF);
            data[size + 1] = (byte)(len >> 8 & 0xFF);
            data[size + 2] = (byte)(len >> 16 & 0xFF);
            data[size + 3] = (byte)(len >> 24 & 0xFF);

            size += 4;

            Buffer.BlockCopy(bytes, 0, data, size, (int)len);

            size += (int)len;

            return true;
        }

        public bool Push(byte[] rhs_, uint size_)
        {
            if(size + size_ + sizeof(uint) > capacity)
            {
                return false;
            }

            data[size] = (byte)(size_ & 0xFF);
            data[size + 1] = (byte)(size_ >> 8 & 0xFF);
            data[size + 2] = (byte)(size_ >> 16 & 0xFF);
            data[size + 3] = (byte)(size_ >> 24 & 0xFF);

            size += 4;

            if(rhs_ != null)
            {
                Buffer.BlockCopy(rhs_, 0, data, size, (int)size_);

                size += (int)size_;
            }

            return true;
        }

        public int GetSize() { return size; }
        public int GetCap() { return capacity; }
        public byte[] GetData() { return data; }

        private byte[] data;
        private int size;
        private int capacity;
    }

    public class DeserializeLib
    {
        public DeserializeLib()
        {
            data = null;
            size = 0;
            idx = 0;
        }

        public DeserializeLib(byte[] bytes, int size_)
        {
            if (bytes != null)
            {
                try
                {
                    data = new byte[size_];
                }
                catch (Exception e)
                {
                    Console.WriteLine($"DeserializeLib() : {e.Message}");
                }

                Buffer.BlockCopy(bytes, 0, data, 0, size_);
                size = size_;
                idx = 0;
            }
        }

        public bool Reserve(int size_)
        {
            if(data == null || size_ != data.Length)
            {
                try
                {
                    data = new byte[size_];
                    size = size_;
                    idx = 0;
                }
                catch (Exception e)
                {
                    Console.WriteLine($"DeserializeLib() : {e.Message}");
                    return false;
                }
            }            

            return true;
        }

        public bool Init(byte[] bytes, int size_)
        {
            if(bytes == null)
            {
                return false;
            }

            if(data == null || size_ > data.Length)
            {
                try
                {
                    data = new byte[size_];
                }
                catch (Exception e)
                {
                    Console.WriteLine($"DeserializeLib() : {e.Message}");
                    return false;
                }
            }

            Buffer.BlockCopy(bytes, 0, data, 0, size_);
            size = size_;
            idx = 0;

            return true;
        }

        unsafe public bool Get<T>(ref T rhs_) where T : unmanaged
        {
            if (typeof(T) == typeof(int) || typeof(T) == typeof(uint) ||
                typeof(T) == typeof(float) || typeof(T) == typeof(double) ||
                typeof(T) == typeof(ulong) || typeof(T) == typeof(long) ||
                typeof(T) == typeof(short) || typeof(T) == typeof(ushort) || typeof(T) == typeof(byte))
            {
                int tSize = System.Runtime.InteropServices.Marshal.SizeOf<T>();

                if (size - idx < tSize)
                {
                    return false;
                }

                fixed (byte* pbyte = data)
                {
                    T* pT = (T*)(pbyte + idx);
                    rhs_ = *pT;
                }
                idx += tSize;

                return true;
            }
            else
            {
                throw new ArgumentException($"Unsupported type : {typeof(T)}");
            }
        }

        public bool Get(ref string rhs_)
        {
            if (size - idx < sizeof(uint))
            {
                return false;
            }
            uint len = (uint)(data[idx] | data[idx + 1] << 8 | data[idx + 2] << 16 | data[idx + 3] << 24);

            if (len == 0 || len > MAX_STRING_SIZE || size - idx - sizeof(uint) < (int)len)
            {
                return false;
            }

            idx += sizeof(uint);

            byte[] SubBytes = new byte[len];
            Buffer.BlockCopy(data, idx, SubBytes, 0, (int)len);

            rhs_ = Encoding.GetEncoding("euc-kr").GetString(SubBytes);

            idx += (int)len;

            return true;
        }

        public int Get(ref byte[] rhs_)
        {
            if (size - idx < sizeof(uint))
            {
                return -1;
            }

            uint len = (uint)(data[idx] | data[idx + 1] << 8 | data[idx + 2] << 16 | data[idx + 3] << 24);

            if(rhs_ == null || len > rhs_.Length)
            {
                rhs_ = new byte[len];
            }

            idx += sizeof(uint);
            Buffer.BlockCopy(data, idx, rhs_, 0, (int)len);
            idx += (int)len;

            return (int)len;
        }

        public int GetRemainSize() { return size - idx; }

        private byte[] data;
        private int size;
        private int idx;
    }

    public Serializer(ActivationMode mode)
    {
        if(mode != ActivationMode.DLIB_ONLY)
        {
            slib = new SerializeLib();
            slib.Resize(MAX_PACKET_SIZE);
        }

        if(mode != ActivationMode.SLIB_ONLY)
        {
            dlib = new DeserializeLib();
            dlib.Reserve(MAX_PACKET_SIZE);
        }
    }

    public bool Serialize(ReqMessage req_, ref byte[] out_)
    {
        if(slib == null)
        {
            return false;
        }

        slib.Flush();

        bool bRet = slib.Push((int)req_.type) &&
            slib.Push(req_.reqNo) &&
            slib.Push(req_.payLoad, req_.payLoadSize);

        if(!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];
        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }

    public bool Serialize(CreateRoomParameter param_, ref byte[] out_)
    {
        if (slib == null)
        {
            return false;
        }

        slib.Flush();

        bool bRet = slib.Push(param_.name, param_.nameLen) &&
            slib.Push(param_.pw, param_.pwLen) &&
            slib.Push(param_.maxUsers);

        if (!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];

        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }

    public bool Serialize(SetNicknameParameter param_, ref byte[] out_)
    {
        if (slib == null) { return false; }

        slib.Flush();

        bool bRet = slib.Push(param_.nickName, param_.nickNameLen);

        if(!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];

        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }

    public bool Serialize(DrawParameter param_, ref byte[] out_)
    {
        if (slib == null) { return false; }

        slib.Flush();

        bool bRet = slib.Push(param_.drawNum) &&
            slib.Push(param_.drawPosX) && slib.Push(param_.drawPosY) &&
            slib.Push(param_.width) &&
            slib.Push(param_.r) && slib.Push(param_.g) && slib.Push(param_.b);

        if (!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];

        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }

    public bool Serialize(DrawEndParameter param_, ref byte[] out_)
    {
        if (slib == null) { return false; }

        slib.Flush();

        bool bRet = slib.Push(param_.drawNum);

        if (!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];

        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }

    public bool Serialize(EnterRoomParameter param_, ref byte[] out_)
    {
        if (slib == null) { return false; }

        slib.Flush();

        bool bRet = slib.Push(param_.roomnumber) &&
            slib.Push(param_.password, param_.pwLen);

        if (!bRet)
        {
            return false;
        }

        out_ = new byte[slib.GetSize()];

        Array.Copy(slib.GetData(), out_, slib.GetSize());

        return true;
    }
    
    public bool Deserialize(byte[] bytes_, uint size_, ref ResMessage res_)
    {
        if(dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        int rescode = 0;

        bool bRet = dlib.Get(ref res_.reqNo) &&
                dlib.Get(ref rescode);
        int iRet = dlib.Get(ref res_.payLoad);

        if (!bRet || iRet == -1)
        {
            return false;
        }

        res_.resCode = (InfoType)rescode;
        res_.payLoadSize = (uint)iRet;

        return true;
    }
    
    public bool Deserialize(byte[] bytes_, uint size_, ref CreateRoomRes res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        bool bRet = dlib.Get(ref res_.roomNum);

        if (!bRet)
        {
            return false;
        }

        int iRet = dlib.Get(ref res_.roomName);

        if(iRet < 1 || iRet > MAX_ROOM_NAME_LEN)
        {
            return false;
        }
        res_.roomNameLen = (ushort)iRet;

        iRet = dlib.Get(ref res_.password);

        if(iRet > MAX_ROOM_NAME_LEN)
        {
            return false;
        }
        res_.pwLen = (ushort)iRet;


        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref EnterRoomRes res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        int iRet = dlib.Get(ref res_.roomName);

        if (iRet < 1 || iRet > MAX_ROOM_NAME_LEN)
        {
            return false;
        }
        res_.roomNameLen = (ushort)iRet;

        iRet = dlib.Get(ref res_.password);

        if (iRet > MAX_ROOM_NAME_LEN)
        {
            return false;
        }
        res_.pwLen = (ushort)iRet;


        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref RoomEnterInfo res_)
    {
        if(dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_ , (int)size_);
        bool bRet = dlib.Get(ref res_.userNum);

        int iRet = dlib.Get(ref res_.nickname);

        if(!bRet || iRet > MAX_ROOM_NAME_LEN || iRet < 1)
        {
            return false;
        }

        res_.nicknameLen = (ushort)iRet;

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref RoomExitInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);
        bool bRet = dlib.Get(ref res_.userNum);

        if(!bRet)
        {
            return false;
        }

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref ChatInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);
        bool bRet = dlib.Get(ref res_.userNum);
        int iRet = dlib.Get(ref res_.chat);

        if (!bRet || iRet == 0 || iRet > MAX_CHATTING_LEN)
        {
            return false;
        }

        res_.chatLen = (ushort)iRet;

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref DrawInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);
        bool bRet = dlib.Get(ref res_.drawNum) && dlib.Get(ref res_.userNum) &&
            dlib.Get(ref res_.drawPosX) && dlib.Get(ref res_.drawPosY) &&
            dlib.Get(ref res_.width);

        if (!bRet)
        {
            return false;
        }

        byte br = 0, bg = 0, bb = 0;

        bRet = dlib.Get(ref br) && dlib.Get(ref bg) && dlib.Get(ref bb);

        if (!bRet)
        {
            return false;
        }

        res_.r = (float)br / 255;
        res_.g = (float)bg / 255;
        res_.b = (float)bb / 255;

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref DrawEndInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);
        bool bRet = dlib.Get(ref res_.drawNum) && dlib.Get(ref res_.userNum);

        if (!bRet)
        {
            return false;
        }

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref UndoInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);
        bool bRet = dlib.Get(ref res_.drawNum) && dlib.Get(ref res_.userNum);

        if (!bRet)
        {
            return false;
        }

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref RoomCanvasInfo res_)
    {
        if(dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        // ----- texture -----
        
        bool bRet = dlib.Get(ref res_.chunkidx);

        if(!bRet)
        {
            return false;
        }

        res_.pixels = new Color32[CANVAS_INFO_CHUNK_COUNT_OF_PIXELS];

        for(int i = 0; i < CANVAS_INFO_CHUNK_COUNT_OF_PIXELS; i++)
        {
            bRet = bRet && dlib.Get(ref res_.pixels[i].r) &&
                dlib.Get(ref res_.pixels[i].g) &&
                dlib.Get(ref res_.pixels[i].b);

            res_.pixels[i].a = 255;

            if (!bRet)
            {
                return false;
            }
        }

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref RoomDrawCommandInfo res_)
    {
        if (dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        // ----- Commands -----
        bool bRet = dlib.Get(ref res_.CountOfCommands);

        if (!bRet)
        {
            return false;
        }

        res_.drawkeys = new uint[res_.CountOfCommands];
        res_.CommandInfos = new DrawCommand[res_.CountOfCommands];

        for (int commandidx = 0; commandidx < res_.CountOfCommands; commandidx++)
        {
            int verticesSize = 0;

            uint key = 0;
            byte r = 0, g = 0, b = 0;
            float width = 0f;

            bRet = dlib.Get(ref key) &&
                dlib.Get(ref r) &&
                dlib.Get(ref g) &&
                dlib.Get(ref b) &&
                dlib.Get(ref width) &&
                dlib.Get(ref verticesSize);

            res_.CommandInfos[commandidx] = new DrawCommand(new Color((float)r / 255, (float)g / 255, (float)b / 255, 1f), width);
            res_.drawkeys[commandidx] = key;

            if (!bRet)
            {
                return false;
            }

            res_.CommandInfos[commandidx].vertices = new Vector2Int[verticesSize];

            for (int vertexidx = 0; vertexidx < verticesSize; vertexidx++)
            {
                short xval = 0, yval = 0;

                bRet = bRet && dlib.Get(ref xval) && dlib.Get(ref yval);

                if (!bRet)
                {
                    return false;
                }

                res_.CommandInfos[commandidx].vertices[vertexidx].x = xval;
                res_.CommandInfos[commandidx].vertices[vertexidx].y = yval;
            }
        }

        return true;
    }

    public bool Deserialize(byte[] bytes_, uint size_, ref RoomInfos res_)
    {
        if(dlib == null)
        {
            return false;
        }

        dlib.Init(bytes_, (int)size_);

        bool bRet = dlib.Get(ref res_.RoomCount);

        if (!bRet)
        {
            return false;
        }

        res_.infos = new RoomInfo[res_.RoomCount];

        for(int roomidx = 0; roomidx < res_.RoomCount; roomidx++)
        {
            bRet = dlib.Get(ref res_.infos[roomidx].roomNum);

            if(!bRet)
            {
                return false;
            }

            int iRet = dlib.Get(ref res_.infos[roomidx].roomName);

            if(iRet == 0)
            {
                return false;
            }

            res_.infos[roomidx].roomNameLen = (ushort)iRet;

            bRet = dlib.Get(ref res_.infos[roomidx].nowUsers) && dlib.Get(ref res_.infos[roomidx].maxUsers);

            if (!bRet)
            {
                return false;
            }
        }

        return true;
    }
}
