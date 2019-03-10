/*
 * App.hpp
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef MAIN_APP_HPP_
#define MAIN_APP_HPP_

#include "../Helper/DateTime.hpp"
#include "../Helper/Portability.hpp"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "../Helper/Versions.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/ServerSettings.hpp"
#include "ConfigFile.hpp"
#include "Server.hpp"

namespace crawlservpp::Main {

	class App {
		// for convenience
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::ServerSettings ServerSettings;

	public:
		App(int argc, char * argv[]) noexcept;
		virtual ~App();

		int run() noexcept;

		// signal handling
		static App * instance;
		static void signal(int num);
		void shutdown(int num);

		// not moveable, not copyable
		App(App&) = delete;
		App(App&&) = delete;
		App& operator=(App&) = delete;
		App& operator=(App&&) = delete;

	private:
		std::atomic<bool> running;
		std::unique_ptr<Server> server;

		// helper function
		bool getPassword(DatabaseSettings& dbSettings);

		// static helper functions
		static void outputHeader();
		static void checkArgumentNumber(int argc);
		static void loadConfig(const std::string& fileName, DatabaseSettings& dbSettings, ServerSettings& serverSettings);
	};

} /* crawlservpp::Main */

#endif /* MAIN_APP_HPP_ */
