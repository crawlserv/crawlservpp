/*
 * App.h
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef GLOBAL_APP_H_
#define GLOBAL_APP_H_

#include "ConfigFile.h"
#include "Server.h"

#include "../Helper/DateTime.h"
#include "../Helper/Portability.h"
#include "../Helper/Versions.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/ServerSettings.h"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace crawlservpp::Global {
	class App {
	public:
		App(int argc, char * argv[]);
		virtual ~App();

		int run();

	private:
		bool running;
		crawlservpp::Global::Server * server;

		// static helper functions
		static void outputHeader();
		static bool checkArgumentNumber(int argc, std::string& errorTo);
		static bool loadConfig(const std::string& fileName, crawlservpp::Struct::DatabaseSettings& dbSettings, crawlservpp::Struct::ServerSettings& serverSettings, std::string& errorTo);
		static bool getPassword(crawlservpp::Struct::DatabaseSettings& dbSettings);
	};
}

#endif /* GLOBAL_APP_H_ */
