#include "Db.h"

namespace db{
	Db::Db()
	{
	}

	Db::~Db()
	{
	}

	bool Db::fileExists(const string& filename)
	{
		ifstream file(filename);
		return file.good();
	}

	long long Db::generateUniqueId()
	{
		std::random_device rd;
		std::mt19937_64 generator(rd());
		std::uniform_int_distribution<long long> distribution;
		return distribution(generator) % (10000000 + 1);
	}

	void Db::createDataBase(vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger>& logger)
	{
		fstream file("database.dat", ios::in | ios::out | ios::app);

		if (!file)
		{
			logger->addLog("Failed to open the database file.");
			exit(EXIT_FAILURE);
		}
		for (const auto& address : *addresses) {
			file << address.id << " " << address.pcAddress << " " << address.isDecisionMakerCenter << '\n';
		}
		file.close();
	}

	vector<userAddressInfo> Db::readAddresses(bool * isDecisionMakerCenter, string hostname, unique_ptr<loggernamespace::Logger>& logger)
	{
		vector<userAddressInfo> users;
		userAddressInfo tempUser;
		fstream file("database.dat", ios::in | ios::out | ios::app);
		if (!file) {
			logger->addLog("Failed to open the database file.");
			exit(EXIT_FAILURE);
		}
		while (file) {
			file >> tempUser.id >> tempUser.pcAddress >> tempUser.isDecisionMakerCenter;
			if (!file)
				break;
			if (tempUser.pcAddress != hostname)
			{
				users.push_back(tempUser);
			}
			else
			{
				*isDecisionMakerCenter = tempUser.isDecisionMakerCenter;
			}
		}

		file.close();

		return users;
	}

	void Db::updateDataBase(string filename, vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger>& logger)
	{
		ofstream outputFile(filename, std::ios::trunc);
		if (!outputFile) {
			logger->addLog("Failed to open the database file for writing.");
			return;
		}

		for (const auto& address : *addresses) {
			outputFile << address.id << ' ' << address.pcAddress << ' ' << address.isDecisionMakerCenter << '\n';
		}
		outputFile.close();
	}
}
