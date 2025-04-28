#include "SketchatServer.hpp"
#include <conio.h>
#include <Windows.h>

const int gBindPort = 12345;
const unsigned short gMaxClient = 100;

int main()
{
	SketchatServer server(gBindPort, gMaxClient);
	server.Start();
	std::cout << "아무키나 누르면 종료됩니다.\n";
	_getch();

	server.End();

}