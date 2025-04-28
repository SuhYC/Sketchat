#pragma once
#include <concurrent_queue.h>
#include "PacketData.hpp"

namespace NetworkPacket
{
	const int OBJECT_COUNT = 100;

	class PacketPool
	{
	public:
		PacketPool()
		{
			for (int i = 0; i < OBJECT_COUNT; i++)
			{
				q.push(new PacketData());
			}
		}

		~PacketPool()
		{
			PacketData* tmp;
			while (q.try_pop(tmp))
			{
				delete tmp;
			}
		}

		void Deallocate(PacketData* pPacket_)
		{
			pPacket_->Clear();
			q.push(pPacket_);
		}

		PacketData* Allocate()
		{
			PacketData* ret = nullptr;

			if (q.try_pop(ret))
			{
				return ret;
			}

			std::cerr << "PacketPool::Allocate : 메모리 풀 부족\n";
			try
			{
				ret = new PacketData();
			}
			catch (std::bad_alloc e)
			{
				std::cerr << "bad_alloc : 메모리 부족\n";
			}

			return ret;
		}

	private:
		Concurrency::concurrent_queue<PacketData*> q;
	};

}

