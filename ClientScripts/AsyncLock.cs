using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

/// <summary>
/// async/await�󿡼� ���� ����� �� �ֵ��� �ϴ� Ŭ����.
/// �⺻������ �񵿱����α׷��ֿ��� �Ϲ����� Lock�� ����� �� ����.
/// Lock���� ��ȣ�����ϴ� ��쿡 �����尡 �� ������ ���µ�,
/// async/await�� �����帧�� �������� �������� �̵��ϴ� ���� �ƴ϶� �����ϴ� Task�� �����ϴ� ���̱� ����.
/// await�� ȣ��Ǿ� �ٸ� �Լ��� �����ϴ��� ������� ������� �ʾұ� ������
/// Lock�� �ɾ������� �ٸ� Task�� �����帧�� ��ȣ�������� �ʰ� ������ �� �ִ�.
/// 
/// �׷��� SemaphoreSlim�� WaitAsync()�� �̿��� Task������ ��ȣ������ ����Ѵ�.
/// �׸��� IDisposable �������̽��� ����� ������ ������ ��ȣ������ ������ �� �ֵ��� �Ѵ�.
/// </summary>
public sealed class AsyncLock
{
    private readonly SemaphoreSlim _semaphore = new SemaphoreSlim(1, 1);

    /// <summary>
    /// ���� :
    /// private static readonly AsyncLock _asyncLock = new AsyncLock(); <br/>
    /// using (await _asyncLock.LockAsync()) { ... }
    /// </summary>
    /// <returns></returns>
    public async Task<IDisposable> LockAsync()
    {
        await _semaphore.WaitAsync();
        return new Handler(_semaphore);
    }

    private sealed class Handler : IDisposable
    {
        private readonly SemaphoreSlim _semaphore;
        private bool _disposed = false;

        public Handler(SemaphoreSlim semaphore)
        {
            _semaphore = semaphore;
        }


        /// <summary>
        /// IDisposable �������̽��� ����Ͽ�
        /// using �������� ������ ���� �� �ڵ����� ȣ��ȴ�.
        /// �������� ��� ���� �ƴ� ���� �� ȣ��ǹǷ�
        /// ���ϴ� �������� lock�� �����ȴ�.
        /// </summary>
        public void Dispose()
        {
            if (!_disposed)
            {
                _semaphore.Release();
                _disposed = true;
            }
        }
    }
}
