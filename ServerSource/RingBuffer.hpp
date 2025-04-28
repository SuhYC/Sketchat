#pragma once

#include <string>
#include <mutex>
#include <utility>
#include <Windows.h>


/// <summary>
/// �ۼ��ſ� ����� ����.
/// ��Ŷ�� ������ [uint;PayloadSize][int(enum);ReqType][uint;ReqNo][serialized;Payload]
/// ��Ŷ�� �ٸ� ����ȭ ���̺귯���� �ƴ� ���� ���� ���� ���̺귯���� ����Ѵ�.
/// 
/// ������ �Ķ���͸��� �Է��ϰ� ���� �Է��ϴ� ����� �ƴ�
/// ������� �Ķ������ ���� �Է��ϴ� ����̱� ������
/// �ϴ� ���ۿ� ��� ������ ���̷� �߶� ó���ϴ� ����� �ʿ��ϴ�.
/// 
/// ������ �����͸����� ó���� �ȵǴ� ��찡 ���� �� ������
/// ������ �����ʹ� �ϴ� ���, ó���� �� �ִ� �����͸� ó���ϴ� ���
/// 
/// �׷��� �ϴ� ������ ���.
/// </summary>
/// 
class RingBuffer
{
public:
	RingBuffer() : m_Capacity(0), m_ReadPos(0), m_WritePos(0), m_pData(nullptr)
	{

	}

	~RingBuffer()
	{
		if (m_pData != nullptr)
		{
			delete[] m_pData;
		}
	}

	bool Init(const int capacity_)
	{
		if (capacity_ <= 0)
		{
			return false;
		}

		m_Capacity = capacity_;
		m_ReadPos = 0;
		m_WritePos = 0;

		if (m_pData != nullptr)
		{
			delete m_pData;
		}
		m_pData = new char[capacity_];
		ZeroMemory(m_pData, capacity_);

		return true;
	}


	bool enqueue(char* in_, int size_)
	{
		// ������ �ȴ���
		if (size_ <= 0)
		{
			return false;
		}

		// ���ʿ� ������
		if (size_ >= m_Capacity)
		{
			return false;
		}

		// ���� ���� �ʰ�
		if ((m_WritePos > m_ReadPos && (m_WritePos + size_) >= m_ReadPos + m_Capacity) ||
			(m_WritePos < m_ReadPos && (m_WritePos + size_) >= m_ReadPos))
		{
			return false;
		}

		char* source = in_;
		int leftsize = size_;

		// ������ �����Ͱ� ���������� ���� ������ -> ���� �����ؾ��� (���κ�, ó���κ����� ������)
		if (m_WritePos + size_ >= m_Capacity)
		{
			int availableSpace = m_Capacity - m_WritePos;
			CopyMemory(&m_pData[m_WritePos], source, availableSpace);
			m_WritePos = 0;
			source += availableSpace;
			leftsize -= availableSpace;
		}

		CopyMemory(&m_pData[m_WritePos], source, leftsize);
		m_WritePos += leftsize;

		m_WritePos %= m_Capacity;

		return true;
	}

	// ret : peeked Data size
	int dequeue(char* out_, int maxSize_)
	{
		//std::lock_guard<std::mutex> guard(m_mutex);

		if (IsEmpty())
		{
			return 0;
		}

		int remain;

		// �Ϲ����� ���
		if (m_WritePos > m_ReadPos)
		{
			remain = m_WritePos - m_ReadPos;
			if (remain > maxSize_)
			{
				remain = maxSize_;
			}

			CopyMemory(out_, &m_pData[m_ReadPos], remain);
			m_ReadPos += remain;
			m_ReadPos %= m_Capacity;
			return remain;
		}
		// ���������� ������ ������ ��
		else if (m_WritePos == 0)
		{
			remain = m_Capacity - m_ReadPos;
			if (remain > maxSize_)
			{
				remain = maxSize_;
			}
			CopyMemory(out_, &m_pData[m_ReadPos], remain);
			m_ReadPos += remain;
			m_ReadPos %= m_Capacity;
			return remain;
		}
		// ���������� ������ �а� ���������� �� �պ��� �̾ ������ ��
		else
		{
			remain = m_Capacity - m_ReadPos;

			if (remain > maxSize_)
			{
				remain = maxSize_;
				CopyMemory(out_, &m_pData[m_ReadPos], remain);
				m_ReadPos += remain;
				m_ReadPos %= m_Capacity;
				return remain;
			}

			CopyMemory(out_, &m_pData[m_ReadPos], remain);

			if (maxSize_ - remain < m_WritePos)
			{
				CopyMemory(out_ + remain, &m_pData[0], maxSize_ - remain);
				m_ReadPos = maxSize_ - remain;
				m_ReadPos %= m_Capacity;
				return maxSize_;
			}

			CopyMemory(out_ + remain, &m_pData[0], m_WritePos);
			m_ReadPos = m_WritePos;
			return remain + m_WritePos;
		}
	}

	int Size() const
	{
		if (IsEmpty())
		{
			return 0;
		}

		if (m_ReadPos < m_WritePos)
		{
			return m_WritePos - m_ReadPos;
		}

		return m_WritePos + m_Capacity - m_ReadPos;
	}
	bool IsEmpty() const { return m_ReadPos == m_WritePos; }
	bool IsFull() const { return (m_WritePos + 1) % m_Capacity == m_ReadPos; }
private:
	int m_ReadPos;
	int m_WritePos;

	char* m_pData;
	int m_Capacity;

	//std::mutex m_mutex;
};