#include "Logger.h"

namespace loggernamespace
{
	Logger::Logger()
	{
	}

	Logger::~Logger()
	{
		if (file.is_open())
		{
			file.close();
		}
	}

	void Logger::addLog(std::string message)
	{
		if (!isFileOpen)
		{
			std::cerr << "File is not open" << std::endl;

			return;
		}

		char currentDateTime[BUFFER_SIZE]{};

		std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		errno_t error{ ctime_s(currentDateTime, BUFFER_SIZE, &currentTime) };

		if (error)
		{
			std::cerr << "Error occured: " << error << std::endl;

			return;
		}

		currentDateTime[BUFFER_SIZE - 2] = '\0';

		if (address.size())
		{
			file << address << ": " 
				<< currentDateTime
				<< " "
				<< message << std::endl;
		}
		else
		{
			file << currentDateTime
				<< " "
				<< message << std::endl;
		}

		if (file.bad())
		{
			std::cerr << "Can't write log" << std::endl;
		}
	}

	void Logger::initialize()
	{
		file.open(path, std::ios::app | std::ios::out);

		if (!file.is_open())
		{
			std::cerr << "Can't create/open log file" << std::endl;
		}
		else
		{
			isFileOpen = true;
		}
	}

	bool Logger::isInitialized()
	{
		return !isFileOpen;
	}

	void Logger::setAddress(std::string address)
	{
		this->address = address;
	}
}