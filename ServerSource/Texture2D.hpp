#pragma once;

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <functional>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#include "Serialization.hpp"

const uint32_t CANVAS_WIDTH = 512;
const uint32_t CANVAS_HEIGHT = 512;
const uint32_t MAX_STACK = 50;

class Color
{
public:
	Color() : r(1.0f), g(1.0f), b(1.0f)
	{

	}

	Color(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}

	float r;
	float g;
	float b;
	//float a;

	Color& operator=(const Color& other_)
	{
		if (&other_ == this)
		{
			return *this;
		}

		this->r = other_.r;
		this->g = other_.g;
		this->b = other_.b;

		return *this;
	}
};

class Vector2Int
{
public:
	Vector2Int(int32_t x_, int32_t y_) : x(x_), y(y_) {}

	int32_t x, y;
};


float lerp(float from, float to, float t) {
	return from + (to - from) * t;
}

class DrawCommand
{
public:
	DrawCommand(Color& col_, float width_) : DrawColor(col_), DrawWidth(width_)
	{
		vertices.reserve(2);
	}

	void Push(Vector2Int& vertex)
	{
		if (vertices.size() == vertices.capacity())
		{
			vertices.reserve(vertices.size() * 2);
		}

		vertices.emplace_back(vertex.x, vertex.y);
	}

	Color DrawColor;
	float DrawWidth;
	std::vector<Vector2Int> vertices;
};


class Texture2D
{
public:
	Texture2D() : m_width(CANVAS_WIDTH), m_height(CANVAS_HEIGHT)
	{
		m_pixels = new Color[CANVAS_WIDTH * CANVAS_HEIGHT];
	}

	Texture2D(uint32_t width, uint32_t height) : m_width(width), m_height(height)
	{
		m_pixels = new Color[width * height];
	}

	~Texture2D()
	{
		delete[] m_pixels;
	}

	void Init(uint32_t width = CANVAS_WIDTH, uint32_t height = CANVAS_HEIGHT)
	{
		if (m_pixels != nullptr)
		{
			delete[] m_pixels;
		}

		m_pixels = new Color[width * height];
		m_width = width;
		m_height = height;

		return;
	}

	void SetPixel(uint32_t x_, uint32_t y_, Color& col_)
	{
		if (x_ >= m_width || y_ >= m_height)
		{
			return;
		}

		int idx = (y_ * m_width) + x_;

		m_pixels[idx] = col_;

		return;
	}

	void DrawAt(int x, int y, float brushSize_, Color& color_)
	{
		for (int i = -(int)brushSize_; i <= brushSize_; i++)
		{
			for (int j = -(int)brushSize_; j <= brushSize_; j++)
			{
				if (i * i + j * j <= brushSize_ * brushSize_) // 원 안에 있는 픽셀만
				{
					int dx = x + i;
					int dy = y + j;

					if (dx >= 0 && dx < m_width && dy >= 0 && dy < m_height)
					{
						SetPixel(dx, dy, color_);
					}
				}
			}
		}
	}

	void DrawLine(Vector2Int& from, Vector2Int& to, float brushSize_, Color& color_)
	{
		int dx = std::abs(to.x - from.x);
		int dy = std::abs(to.y - from.y);
		int steps = (dx > dy) ? dx : dy;

		for (int i = 0; i <= steps; i++)
		{
			float t = steps == 0 ? 0 : (float)i / steps;
			int x = static_cast<int>(std::round(lerp(from.x, to.x, t)));
			int y = static_cast<int>(std::round(lerp(from.y, to.y, t)));
			DrawAt(x, y, brushSize_, color_);
		}

		return;
	}

	void DoCommand(DrawCommand* pCommand)
	{
		DrawAt(pCommand->vertices[0].x, pCommand->vertices[0].y, pCommand->DrawWidth, pCommand->DrawColor);

		if (pCommand->vertices.size() > 1)
		{
			for (int i = 0; i < pCommand->vertices.size() - 1; i++)
			{
				Vector2Int from = pCommand->vertices[i];
				Vector2Int to = pCommand->vertices[i + 1];

				DrawLine(from, to, pCommand->DrawWidth, pCommand->DrawColor);
			}
		}
	}

	Color* m_pixels;
	uint32_t m_width;
	uint32_t m_height;
};



class CommandStack
{
public:
	CommandStack()
	{

	}

	/// <summary>
	/// 계속해서 들어오는 드래그 동작을 하나의 Command로 만든다.
	/// 이후 Undo로 하나의 Command씩 지울 수 있게 하기 위함.
	/// </summary>
	/// <param name="Drawkey_"></param>
	/// <param name="vertex_"></param>
	/// <param name="color_"></param>
	/// <param name="width"></param>
	void AddVertexToCommand(uint32_t Drawkey_, Vector2Int& vertex_, Color& color_, float width)
	{
		std::lock_guard<std::mutex> guard(m_mutex);

		auto itr = DrawCommands.find(Drawkey_);

		DrawCommand* pCommand = nullptr;

		if (itr == DrawCommands.end())
		{
			pCommand = new DrawCommand(color_, width);
			DrawCommands.emplace(Drawkey_, pCommand);
		}
		else
		{
			pCommand = itr->second;
		}

		pCommand->Push(vertex_);
	}

	/// <summary>
	/// 완성된 Command를 Queuing하여 Undo할 수 있도록 만든다.
	/// 50회 까지 기록하며, 50회를 넘어가면 가장 오래된 Command를
	/// Texture에 융화시킨다.
	/// </summary>
	/// <param name="Drawkey_"></param>
	void Push(uint32_t Drawkey_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (DrawKeys.size() == MAX_STACK)
		{
			uint32_t headkey = *(DrawKeys.begin());
			DrawKeys.pop_front();

			auto itr = DrawCommands.find(headkey);

			if (itr != DrawCommands.end())
			{
				DrawCommand* pCommand = itr->second;
				DrawCommands.erase(headkey);

				initTexture.DoCommand(pCommand);
				delete pCommand;
			}
			
		}
		DrawKeys.push_back(Drawkey_);
	}

	/// <summary>
	/// queue에 있는 drawkey 중 가장 최근것을 제거한다.
	/// if ret == 0 : failed to undo
	/// </summary>
	/// <returns></returns>
	uint32_t Undo()
	{
		if (DrawKeys.empty())
		{
			return 0;
		}

		std::lock_guard<std::mutex> guard(m_mutex);

		uint32_t drawkey = DrawKeys.back();

		DrawKeys.pop_back();

		auto itr = DrawCommands.find(drawkey);

		if (itr != DrawCommands.end())
		{
			delete itr->second;

			DrawCommands.erase(itr);
		}

		return drawkey;
	}

	/// <summary>
    /// [count of pixels:uint][color * count : float * 3 * count][count of Commands:ushort][Command infos]
	/// [Command info] : [Color : float * 3][width : float][vertices count:uint][[vector2int : int32_t * 2] * vertices count]
	/// </summary>
	/// <param name="out_"></param>
	bool GetData(std::string& out_)
	{
		std::lock_guard<std::mutex> guard(m_mutex);

		SerializeLib slib;

		uint32_t size = sizeof(uint32_t) + initTexture.m_height * initTexture.m_width * sizeof(Color); // count of pixels + Texture
		
		for (auto itr = DrawKeys.begin(); itr != DrawKeys.end(); itr++)
		{
			auto mapitr = DrawCommands.find(*itr);

			if (mapitr != DrawCommands.end())
			{
				size += sizeof(float) * 4 + sizeof(uint32_t) +  // Color + width + vertices count
					sizeof(int32_t) * 2 * mapitr->second->vertices.size(); // vertices count * sizeof(Vector2Int)
			}
		}

		bool bRet = slib.Init(size);

		if (!bRet)
		{
			std::cerr << "Texture2D::CommandStack::GetData : failed to allocate buffer.\n";
			return false;
		}

		// ----- Texture -----

		bRet = bRet && slib.Push(initTexture.m_height * initTexture.m_width);

		if (!bRet)
		{
			std::cerr << "Texture2D::CommandStack::GetData : failed to Push count of pixels\n";
			return false;
		}

		for (int i = 0; i < initTexture.m_height * initTexture.m_width; i++)
		{
			bRet = bRet && slib.Push(initTexture.m_pixels[i].r) &&
				slib.Push(initTexture.m_pixels[i].g) && 
				slib.Push(initTexture.m_pixels[i].b);

			if (!bRet)
			{
				std::cerr << "Texture2D::CommandStack::GetData : failed to Push pixels\n";
				return false;
			}
		}

		// ----- Commands -----

		uint16_t commandCount = DrawKeys.size();
		bRet = bRet && slib.Push(commandCount);

		if (!bRet)
		{
			std::cerr << "Texture2D::CommandStack::GetData : failed to Push Command count\n";
			return false;
		}

		for (auto itr = DrawKeys.begin(); itr != DrawKeys.end(); itr++)
		{
			bRet = slib.Push(*itr);

			if (!bRet)
			{
				std::cerr << "Texture2D::CommandStack::GetData : failed to Push drawkey\n";
				return false;
			}

			auto mapitr = DrawCommands.find(*itr);

			if (mapitr != DrawCommands.end())
			{
				bRet = bRet && slib.Push(mapitr->second->DrawColor.r) &&
					slib.Push(mapitr->second->DrawColor.g) &&
					slib.Push(mapitr->second->DrawColor.b) &&
					slib.Push(mapitr->second->DrawWidth) &&
					slib.Push(mapitr->second->vertices.size());

				if (!bRet)
				{
					std::cerr << "Texture2D::CommandStack::GetData : failed to Push Command\n";
					return false;
				}

				for (auto vertexitr = mapitr->second->vertices.begin(); vertexitr != mapitr->second->vertices.end(); vertexitr++)
				{
					bRet = bRet && slib.Push(vertexitr->x) &&
						slib.Push(vertexitr->y);

					if (!bRet)
					{
						std::cerr << "Texture2D::CommandStack::GetData : failed to Push vertex\n";
						return false;
					}
				}
			}
		}

		out_.resize(slib.GetSize());

		CopyMemory(&(out_[0]), slib.GetData(), slib.GetSize());

		return true;
	}


private:
	Texture2D initTexture;
	std::list<uint32_t> DrawKeys;
	std::map<uint32_t, DrawCommand*> DrawCommands;
	
	std::mutex m_mutex;
};