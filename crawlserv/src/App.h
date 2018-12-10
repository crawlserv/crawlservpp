/*
 * App.h
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef APP_H_
#define APP_H_

#include "ConfigFile.h"
#include "Server.h"

#include "namespaces/DateTime.h"
#include "namespaces/Versions.h"
#include "structs/DatabaseSettings.h"
#include "structs/ServerSettings.h"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include "namespaces/Portability.h"

class App {
public:
	App(int argc, char * argv[]);
	virtual ~App();

	int run();

private:
	bool running;
	Server * server;

	// helper functions
	void outputHeader();
	bool checkArgumentNumber(int argc, std::string& errorTo);
	bool loadConfig(const std::string& fileName, DatabaseSettings& dbSettings, ServerSettings& serverSettings, std::string& errorTo);
	bool getPassword(DatabaseSettings& dbSettings);
};

#endif /* APP_H_ */
