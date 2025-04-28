using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

/// <summary>
/// async/await상에서 락을 사용할 수 있도록 하는 클래스.
/// 기본적으로 비동기프로그래밍에서 일반적인 Lock은 사용할 수 없다.
/// Lock으로 상호배제하는 경우에 스레드가 그 권한을 갖는데,
/// async/await는 실행흐름이 스레드의 변경으로 이동하는 것이 아니라 실행하는 Task를 변경하는 것이기 때문.
/// await가 호출되어 다른 함수를 실행하더라도 스레드는 변경되지 않았기 때문에
/// Lock을 걸었음에도 다른 Task의 실행흐름이 상호배제되지 않고 진입할 수 있다.
/// 
/// 그래서 SemaphoreSlim의 WaitAsync()를 이용해 Task단위의 상호배제를 사용한다.
/// 그리고 IDisposable 인터페이스를 상속해 적절한 시점에 상호배제를 해제할 수 있도록 한다.
/// </summary>
public sealed class AsyncLock
{
    private readonly SemaphoreSlim _semaphore = new SemaphoreSlim(1, 1);

    /// <summary>
    /// 사용법 :
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
        /// IDisposable 인터페이스를 상속하여
        /// using 스코프의 실행이 끝날 때 자동으로 호출된다.
        /// 스코프를 벗어날 때가 아닌 끝날 때 호출되므로
        /// 원하는 시점까지 lock이 유지된다.
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
