using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Net;
using Unity.VisualScripting;


/// <summary>
/// ��Ʈ��ũ �ۼ��� ó���� ���� ��ũ��Ʈ.
/// 
/// MTU�� ���� ���� ������ �ذ��ϱ� ����
/// ������ �޽����� ũ�⸦ ����� �����Ͽ� �ۼ����Ѵ�.
/// 
/// '[' + size + ']' + payload
/// 
/// ������ �޽����� ����� �����ϴ°� ResHandler�� �����Ѵ�.
/// </summary>
public class NetworkManager : MonoBehaviour
{
    private const int port = 12345;
    private const string serverIp = "127.0.0.1";
    private NetworkStream _stream;
    private TcpClient _tcpClient;

    public const int IO_BUF_SIZE = 4096;

    private bool m_IsRun;

    private byte[] _buffer;

    private RingBuffer _SendBuffer;
    private byte[] _SendingData;

    private PacketHandler _PacketHandler;

    private static NetworkManager _instance;

    public static NetworkManager Instance
    {
        get
        {
            if (_instance == null)
            {
                _instance = FindAnyObjectByType<NetworkManager>();
                if (_instance == null)
                {
                    GameObject netManagerObj = new GameObject("NetworkManager");
                    _instance = netManagerObj.AddComponent<NetworkManager>();
                }
            }
            return _instance;
        }
    }

    async void Awake()
    {
        if (_instance != null && _instance != this)
        {
            Destroy(this.gameObject);
            return;
        }
        else
        {
            _instance = this;
            DontDestroyOnLoad(this.gameObject);
        }

        _buffer = new byte[IO_BUF_SIZE];
        _SendingData = new byte[IO_BUF_SIZE];
        _SendBuffer = new RingBuffer();

        _PacketHandler = new PacketHandler();
        _PacketHandler.Init();

        await ConnectToTcpServer(serverIp, port);
    }

    /// <summary>
    /// ���� ����.
    /// ���� ������������Լ�RecvMsg()�� ȣ���Ѵ�. RecvMsg�� ������ ������ ������ ���ο��� �ݺ�.
    /// </summary>
    /// <param name="host"></param>
    /// <param name="port"></param>
    /// <returns></returns>
    private async Task ConnectToTcpServer(string host, int port)
    {
        try
        {
            Application.runInBackground = true;
            _tcpClient = new TcpClient();
            _tcpClient.NoDelay = true;
            await _tcpClient.ConnectAsync(host, port);
            _stream = _tcpClient.GetStream();
        }
        catch (Exception e)
        {
            Debug.Log($"NetworkManager::ConnectToTcpServer : ���� ����. {e.Message}");
            return;
        }

        Debug.Log($"NetworkManager::ConnectToTcpServer : ���� �Ϸ�");

        m_IsRun = true;


        await Task.WhenAll(RecvMsg(), SendIO());

        return;
    }

    private void OnApplicationQuit()
    {
        End();
    }

    public void End()
    {
        m_IsRun = false;

        _stream?.Close();
        _tcpClient?.Close();
    }

    /*
    /// <summary>
    /// PacketMaker.instance.ToReqMessage�� ���� ���� ��Ŷ�� ������ ��.
    /// </summary>
    /// <param name="msg">jsonstr of ReqMessage</param>
    /// <returns></returns>
    public async Task SendMsg(string msg)
    {
        if (_tcpClient != null && _tcpClient.Connected)
        {
            byte[] tmp = Encoding.GetEncoding("euc-kr").GetBytes(msg);

            int size = tmp.Length;

            byte[] data = new byte[size + sizeof(uint)];

            data[0] = (byte)(size & 0xFF);
            data[1] = (byte)((size >> 8) & 0xFF);
            data[2] = (byte)((size >> 16) & 0xFF);
            data[3] = (byte)((size >> 24) & 0xFF);

            Buffer.BlockCopy(tmp, 0, data, sizeof(uint), size);

            await _SendBuffer.EnqueueWithLock(data, size + sizeof(uint));
        }
    }
    */

    public async Task SendMsg(byte[] msg, uint size)
    {
        if (_tcpClient != null && _tcpClient.Connected)
        {
            byte[] data = new byte[size + sizeof(uint)];

            data[0] = (byte)(size & 0xFF);
            data[1] = (byte)((size >> 8) & 0xFF);
            data[2] = (byte)((size >> 16) & 0xFF);
            data[3] = (byte)((size >> 24) & 0xFF);

            Buffer.BlockCopy(msg, 0, data, sizeof(uint), (int)size);

            await _SendBuffer.EnqueueWithLock(data, (int)size + sizeof(uint));
        }
    }

    private async Task SendIO()
    {
        while (m_IsRun)
        {
            int size = await _SendBuffer.DequeueWithLock(_SendingData);

            if (size == 0)
            {
                await Task.Delay(30);
                continue;
            }

            await _stream.WriteAsync(_SendingData, 0, size);
            //Debug.Log($"Networkmanager::SendIO : SendMsg size:{size}");
            await Task.Delay(30); // ������ ��û ����
        }
    }

    /// <summary>
    /// ���������� �����κ����� ������ üũ.
    /// </summary>
    /// <returns></returns>
    private async Task RecvMsg()
    {
        try
        {
            while (true)
            {
                int bytesRead = await _stream.ReadAsync(_buffer);
                if (bytesRead > 0)
                {
                    //Debug.Log($"NetworkManager::RecvMsg : recv {bytesRead} bytes.");

                    await _PacketHandler.HandlePacket(_buffer, bytesRead);
                }
                // ��������, ���� �߻�
                else
                {
                    break;
                }
            }
        }
        catch (Exception e)
        {
            Debug.Log($"NetworkManager::RecvMsg : {e.Message}");
        }
    }

    public void PushReq(Serializer.ReqMessage req_)
    {
        _PacketHandler.PushReq(req_);
    }
}
