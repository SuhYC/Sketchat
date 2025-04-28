#pragma once

#include "User.hpp"
#include <vector>

class UserManager
{
public:
	~UserManager()
	{
		Clear();
	}

	void Init(const int maxCount_)
	{
		users.clear();
		users.reserve(maxCount_);
		for (int i = 0; i < maxCount_; i++)
		{
			users.push_back(new User(i));
		}
	}

	void Clear()
	{
		for (auto* i : users)
		{
			if (i != nullptr)
			{
				delete i;
				i = nullptr;
			}
		}

		users.clear();
	}

	User* GetUserByConnIndex(const unsigned short connectionIndex_) { return users[connectionIndex_]; }

private:
	std::vector<User*> users;
};