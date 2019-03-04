/*
 * App.cpp
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#include "../Main/App.h"

namespace crawlservpp::Main {

App * App::instance = NULL;

// constructor: show header, check arguments, load configuration file, get database password, initialize and run the server
App::App(int argc, char * argv[]) noexcept {
	try {
		crawlservpp::Struct::DatabaseSettings dbSettings;
		crawlservpp::Struct::ServerSettings serverSettings;
		std::string error;

		// set running state
		this->running = true;

		// save instance and register signals
		App::instance = this;
		struct sigaction sigIntHandler;
		sigIntHandler.sa_handler = App::signal;
		sigemptyset(&sigIntHandler.sa_mask);
		sigIntHandler.sa_flags = 0;
		sigaction(SIGINT, &sigIntHandler, NULL);
		sigaction(SIGTERM, &sigIntHandler, NULL);

		// show header
		this->outputHeader();

		// check number of arguments
		this->checkArgumentNumber(argc);

		// load configuration file
		this->loadConfig(argv[1], dbSettings, serverSettings);

		// get password
		if(this->getPassword(dbSettings) && this->running) {
			// create server and run!
			this->server = std::make_unique<Server>(dbSettings, serverSettings);
			std::cout << "Server is up and running." << std::flush;
		}
		else this->running = false;
	}
	catch(const std::exception& e) {
		std::cout << "[ERROR] " << e.what() << std::endl;
	}
	catch(...) {
		std::cout << "[ERROR] Unknown exception in App::App()" << std::endl;
	}
}

// destructor: clean-up server
App::~App() {
	if(this->server) {
		// server up-time message
		std::cout << std::endl << "Up-time: " << crawlservpp::Helper::DateTime::secondsToString(server->getUpTime()) << ".";
		std::cout << std::endl << "> Waiting for threads..." << std::flush;
		this->server.reset();
	}

	// quit message
	std::cout << std::endl << "Bye bye." << std::endl;
}

// run app
int App::run() noexcept {
	if(this->server && this->running) {
		try {
			while(this->server->tick() && this->running) {}
			return EXIT_SUCCESS;
		}
		catch(const std::exception& e) {
			std::cout << std::endl << "[ERROR] " << e.what();
		}
		catch(...) {
			std::cout << std::endl << "[ERROR] Unknown exception in App::run()";
		}
	}
	return EXIT_FAILURE;
}

// static signal handler (forward the signal to the class)
void App::signal(int num) {
	if(App::instance) App::instance->shutdown(num);
}

// in-class signal handler
void App::shutdown(int num) {
	std::cout << std::endl << "[CANCEL] ";
	switch(num) {
	case SIGINT:
		std::cout << "Interruption request signal (SIGINT)";
		break;
	case SIGTERM:
		std::cout << "Termination request signal (SIGTERM)";
		break;
	default:
		std::cout << "Unknown signal (#" << num << ")";
	}
	std::cout << " received." << std::flush;
	this->running = false;
}

// helper function: get database password from user, return false on cancel
bool App::getPassword(crawlservpp::Struct::DatabaseSettings& dbSettings) {
	// prompt password for database
	std::cout << "Enter password for " << dbSettings.user << "@" << dbSettings.host << ":" << dbSettings.port << ": ";
	char input = 0;
	bool inputLoop = true;
	bool inputCancel = false;

	do {
		switch(input = crawlservpp::Helper::Portability::getch()) {
		case '\r':
			// ignore carriage return
			break;
		case '\n':
			// ENTER: end input loop
			inputLoop = false;
			break;
		case '\b':
		case 127:
			// BACKSPACE/DELETE: delete last character from password (if it exists)
			if(!dbSettings.password.empty()) dbSettings.password.pop_back();
			break;
		case 27:
			// ESCAPE -> cancel and end input loop
			inputCancel = true;
			inputLoop = false;
			break;
		default:
			// add other characters to password
			dbSettings.password.push_back(input);
		}

	}
	while(inputLoop && this->running);
	std::cout << std::endl;

	if(inputCancel) return false;
	return true;
}

// static helper function: show version (and library versions)
void App::outputHeader() {
	std::cout << "crawlserv++ v0.1 by Ans using" << std::endl;
	std::cout << crawlservpp::Helper::Versions::getLibraryVersions(" ") << std::endl;
}

// static helper function: check number of command line arguments, throws std::runtime_error
void App::checkArgumentNumber(int argc) {
	if(argc != 2) throw std::runtime_error("USAGE: crawlserv <config_file>");
}

// static helper function: load database and server settings from configuration file, throws std::runtime_error
void App::loadConfig(const std::string& fileName, crawlservpp::Struct::DatabaseSettings& dbSettings,
		crawlservpp::Struct::ServerSettings& serverSettings) {
	ConfigFile configFile(fileName);

	dbSettings.host = configFile.getValue("db_host");

	try {
		dbSettings.port = boost::lexical_cast<unsigned short>(configFile.getValue("db_port"));
	}
	catch(const boost::bad_lexical_cast& e) {
		throw(std::runtime_error(fileName + ": Could not convert config file entry \"db_port\" (=\""
				+ configFile.getValue("db_port") + "\") to numeric value"));
	}

	dbSettings.user = configFile.getValue("db_user");
	dbSettings.name = configFile.getValue("db_name");

	if(!configFile.getValue("server_client_compress").empty()) {
		try {
			dbSettings.compression = boost::lexical_cast<bool>(configFile.getValue("server_client_compress"));
		}
		catch(const boost::bad_lexical_cast& e) {
			throw std::runtime_error(fileName + ": Could not convert config file entry \"server_client_compress\" (=\""
					+ configFile.getValue("server_client_compress") + "\") to boolean value");
		}
	}

	serverSettings.port = configFile.getValue("server_port");
	serverSettings.allowedClients = configFile.getValue("server_allow");

	if(!configFile.getValue("server_logs_deletable").empty()) {
		try {
			serverSettings.logsDeletable = boost::lexical_cast<bool>(configFile.getValue("server_logs_deletable"));
		}
		catch(const boost::bad_lexical_cast& e) {
			throw std::runtime_error(fileName + ": Could not convert config file entry \"server_logs_deletable\" (=\""
					+ configFile.getValue("server_logs_deletable") + "\") to boolean value");
		}
	}
	else serverSettings.logsDeletable = false;

	if(!configFile.getValue("server_data_deletable").empty()) {
		try {
			serverSettings.dataDeletable = boost::lexical_cast<bool>(configFile.getValue("server_data_deletable"));
		}
		catch(const boost::bad_lexical_cast& e) {
			throw std::runtime_error(fileName + ": Could not convert config file entry \"server_data_deletable\" (=\""
					+ configFile.getValue("server_data_deletable") + "\") to boolean value");
		}
	}
	else serverSettings.dataDeletable = false;
}

}
