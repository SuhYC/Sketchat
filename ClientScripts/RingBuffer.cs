using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public class RingBuffer
{
    public const int BUFSIZE = 12800;

    private byte[] _data;
    private int _size;
    private readonly AsyncLock _asyncLock = new AsyncLock();

    public RingBuffer()
    {
        _data = new byte[BUFSIZE];
        _size = 0;
    }

    public void Init()
    {
        Array.Clear(_data, 0, BUFSIZE);
        _size = 0;
    }

    public async Task<bool> EnqueueWithLock(byte[] data_, int size_, int idx = 0)
    {
        using (await _asyncLock.LockAsync())
        {
            if (_size + size_ > BUFSIZE)
            {
                Debug.Log($"뻗");
                return false;
            }

            Buffer.BlockCopy(data_, idx, _data, _size, size_);
            _size += size_;
        }

        return true;
    }

    /// <summary>
    /// data_의 idx부터 size_만큼의 데이터를 복사
    /// </summary>
    /// <param name="data_"></param>
    /// <param name="idx"></param>
    /// <param name="size_"></param>
    /// <returns></returns>
    public bool Enqueue(byte[] data_, int size_, int idx = 0)
    {
        if (_size + size_ > BUFSIZE)
        {
            Debug.Log($"뻗");
            return false;
        }

        //Array.Copy(data_, idx, _data, _size, size_);
        Buffer.BlockCopy(data_, idx, _data, _size, size_);
        _size += size_;

        return true;
    }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="buffer"></param>
    /// <returns>내보낸 데이터 크기</returns>
    public int Dequeue(byte[] buffer)
    {
        if (_size == 0)
        {
            return 0;
        }

        if (_size > buffer.Length)
        {
            Buffer.BlockCopy(_data, 0, buffer, 0, buffer.Length);
            _size = _size - buffer.Length;

            Buffer.BlockCopy(_data, buffer.Length, _data, 0, _size);

            return buffer.Length;
        }
        else
        {
            int ret = _size;

            Buffer.BlockCopy(_data, 0, buffer, 0, _size);
            _size = 0;

            return ret;
        }
    }

    public async Task<int> DequeueWithLock(byte[] buffer)
    {
        using (await _asyncLock.LockAsync())
        {
            if (_size == 0)
            {
                return 0;
            }

            if (_size > buffer.Length)
            {
                Buffer.BlockCopy(_data, 0, buffer, 0, buffer.Length);
                _size = _size - buffer.Length;

                Buffer.BlockCopy(_data, buffer.Length, _data, 0, _size);

                return buffer.Length;
            }
            else
            {
                int ret = _size;

                Buffer.BlockCopy(_data, 0, buffer, 0, _size);
                _size = 0;

                return ret;
            }
        }
    }
}
