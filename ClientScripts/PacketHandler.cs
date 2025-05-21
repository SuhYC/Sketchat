using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.UIElements;


/// <summary>
/// �����κ��� ������ �޽������� ���̷ε� �κи� �и��ϴ� Ŭ����. <br/>
/// ���Ÿ޽����� ������ '[' + size + ']' + ResJsonStr �̴�. <br/>
/// [size]�κ��� ���ó���� ������ ��Ŷ������ �и��Ͽ� ResJsonStr�� �̾Ƴ���, <br/>
/// �ش� JsonStr�� PacketMaker�� HandleServerResponse�� �Ѱ� ó���Ѵ�.
/// 
/// ������ �޽����� MTU�� ���� Layer2�� ����ϱ� ���� ���ҵǾ��� �� �ִ�. <br/>
/// ������ �޽����� �޽��� ����(����� �ܼ��ϰ� string ���� �ϳ��� ��� ���� �����Ѵ�.)�� ��ġ�� <br/>
/// ������ �޽����� payload�κ��� size�� ����� �ѱ���� Ȯ���Ͽ� ����� ����� �Ǿ��ٸ� �и��Ѵ�. <br/>
/// 
/// �ѹ��� ����ó���� ������ ������ �ٸ� �����帧�� �����ϸ� �ȵǹǷ� AsyncLockŬ������ SemaphoreSlim�� ����Ѵ�. <br/>
/// ���ν����� �ϳ��� �����ߴ��� Task������ �����ϸ� �ΰ� �̻��� �����帧�� ������ �� �ִ� �Ӱ迵���̴�. <br/>
/// 
/// ������ �޽����� ���ҵǴ� �� �Ӹ� �ƴ϶� �������� ��Ŷ�� �������� ���� ��쵵 �����Ͽ����Ѵ�. <br/>
/// ���������� ����ť�� �̿��� �����۸� �����صξ���. <br/>
/// �ϴ� �����ۿ� �����͸� ��� �۽ſ�û�� �ϰ� �۽ſ�û�� �������Ǹ� �����ۿ� ��� �ٸ� �����͸� Ȯ���ϱ� ������ <br/>
/// �۽ſ������� ���� �ټ��� ��Ŷ�� �����ۿ� ���ٰ� �ѹ��� �۽ŵ� �� �ִ�. <br/>
/// �׷��Ƿ� �ݺ����� ���� ����� �����ϴ� ������ ������ ȣ�����ش�.
/// </summary>
public class PacketHandler
{
    private string _remainMsg;

    private ResHandler _resHandler;
    private RingBuffer RemainMsgBuffer;
    private byte[] _processBuffer;
    private byte[] _req;

    private uint[] header;

    private const int MAX_SIZE_OF_PACKET = 8192;

    public void Init()
    {
        header = new uint[1];
        _resHandler = new ResHandler();
        _resHandler.Init();

        RemainMsgBuffer = new RingBuffer();
        _processBuffer = new byte[RingBuffer.BUFSIZE];
        _req = new byte[MAX_SIZE_OF_PACKET];
    }

    public async Task HandlePacket(byte[] msg, int size_)
    {
        //Debug.Log($"ResHandler::HandlePacket : Start");

        RemainMsgBuffer.Enqueue(msg, size_);

        try
        {
            int len = RemainMsgBuffer.Dequeue(_processBuffer);
            int idx = 0;

            while (true)
            {
                //Debug.Log($"ResHandler::HandlePacket : While");
                // ���̷ε尡 �ƿ� ����
                if (len - idx < 5)
                {
                    RemainMsgBuffer.Enqueue(_processBuffer, len - idx, idx);

                    return;
                }

                uint length = (uint)(_processBuffer[idx] | _processBuffer[idx + 1] << 8 | _processBuffer[idx + 2] << 16 | _processBuffer[idx + 3] << 24);

                // ��ȿ���� ���� ���
                if (length == 0 || length > MAX_SIZE_OF_PACKET)
                {
                    Debug.Log($"ResHandler::HandlePacket : InValid Size : {length}");
                    //await _buffer.EnqueueWithLock(_processBuffer, len - idx, idx);
                    return;
                }

                if (len - idx >= length + sizeof(uint))
                {
                    Buffer.BlockCopy(_processBuffer, idx + sizeof(uint), _req, 0, (int)length);

                    idx += sizeof(uint) + (int)length;

                    // _req, length�� ó���۾���û
                    await _resHandler.HandleServerResponse(_req, length);
                }
                else
                {
                    //Debug.Log($"ResHandler::HandlePacket : Next");
                    RemainMsgBuffer.Enqueue(_processBuffer, len - idx, idx);

                    return;
                }
            }
        }
        catch (Exception e)
        {
            Debug.Log($"ResHandler::HandlePacket : {e.Message}, {e.StackTrace}");
        }
    }


    public void PushReq(Serializer.ReqMessage req)
    {
        _resHandler.PushReqMessage(req);
    }
}
