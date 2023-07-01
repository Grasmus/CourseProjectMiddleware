#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <ctime>
#include <chrono>

#define BUFFER_SIZE 26

namespace loggernamespace
{
	class Logger
	{
	private:
		const std::string path{ "Logs/log.txt" };

		std::ofstream file{};

		bool isFileOpen{};
		std::string address{};

	public:
		Logger();
		~Logger();

		void addLog(std::string message);
		void initialize();
		bool isInitialized();
		void setAddress(std::string address);
	};
}
