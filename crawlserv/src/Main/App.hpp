/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * App.hpp
 *
 * The main application class that processes command line arguments, shows the initial header, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef MAIN_APP_HPP_
#define MAIN_APP_HPP_

#include "ConfigFile.hpp"
#include "Exception.hpp"
#include "Server.hpp"
#include "SignalHandler.hpp"
#include "Version.hpp"

#include "../Helper/DateTime.hpp"
#include "../Helper/Portability/compiler.h"
#include "../Helper/Portability/getch.h"
#include "../Helper/Versions.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/NetworkSettings.hpp"
#include "../Struct/ServerSettings.hpp"

#include <atomic>		// std::atomic
#include <csignal>		// SIGINT, SIGTERM, std::sig_atomic_t
#include <exception>	// std::exception
#include <iostream>		// std::cout, std::endl, std::flush
#include <memory>		// std::make_unique, std::unique_ptr
#include <string>		// std::string
#include <string_view>	// std::string_view_literals
#include <vector>		// std::vector

//! Namespace for the main classes of the program.
namespace crawlservpp::Main {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Number of arguments required by the application.
	inline constexpr auto argsRequired{2};

	//! First part of the password prompt.
	inline constexpr auto pwPrompt1{"Enter password for "sv};

	//! Second part of the password prompt.
	inline constexpr auto pwPrompt2{"@"sv};

	//! Third part of the password prompt.
	inline constexpr auto pwPrompt3{":"sv};

	//! Fourth part of the password prompt.
	inline constexpr auto pwPrompt4{": "sv};

	//! Message when done with the password input.
	inline constexpr auto doneMsg{"[DONE]"sv};

	//! Code for the backspace key.
	inline constexpr auto inputBackspace{127};

	//! Code for the CTRL+C keys or the end of the file.
	inline constexpr auto inputEof{-1};

	//! Code for the CTRL+C keys or the end of the text.
	inline constexpr auto inputEtx{3};

	//! Code for the Escape key.
	inline constexpr auto inputEsc{27};

	//! The current year.
	//NOLINTNEXTLINE(clang-diagnostic-string-plus-int, cppcoreguidelines-pro-bounds-pointer-arithmetic)
	inline constexpr auto year{__DATE__ + 7};

	//! The name of the application.
	inline constexpr auto descName{"crawlserv++ Command-and-Control Server"sv};

	//! The beginning of the version string.
	inline constexpr auto descVer{"Version "sv};

	//! The beginning of the copyright string.
	inline constexpr auto descCopyrightHead{"Copyright (C) "sv};

	//! The actual copyrigt.
	inline constexpr auto descCopyrightTail{" Anselm Schmidt (ans[ät]ohai.su)"sv};

	//! The text of the license.
	inline constexpr auto descLicense{
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
		"GNU General Public License for more details.\n\n"
		"You should have received a copy of the GNU General Public License\n"
		"along with this program. If not, see <https://www.gnu.org/licenses/>."sv
	};

	//! The string before the used libraries.
	inline constexpr auto descUsing{"using"sv};

	//! The usage string for the command line.
	inline constexpr auto descUsage{"USAGE: crawlserv <config_file> or crawlserv -v"};

	///@}

	/*
	 * DECLARATION
	 */

	//! %Main application.
	/*!
	 * This class
	 * - writes default output to @c stdout
	 * - checks the program arguments
	 * - loads the configuration file
	 * - runs the command-and-control server
	 * - handles signals by the operating system
	 */
	class App final : protected SignalHandler {
		// for convenience
		using DatabaseSettings = Struct::DatabaseSettings;
		using NetworkSettings = Struct::NetworkSettings;
		using ServerSettings = Struct::ServerSettings;

	public:
		///@name Construction and Destruction
		///@{

		explicit App(const std::vector<std::string>& args) noexcept;
		virtual ~App();

		///@}
		///@name Execution
		///@{

		int run() noexcept;

		///@}
		///@name Signal Handling
		///@{

		void shutdown(std::sig_atomic_t signal);

		///@}
		/**@name Copy and Move
		 * The class is neither copyable, nor moveable.
		 */
		///@{

		//! Deleted copy constructor.
		App(App&) = delete;

		//! Deleted copy assignment operator.
		App& operator=(App&) = delete;

		//! Deleted move constructor.
		App(App&&) = delete;

		//! Deleted move assignment operator.
		App& operator=(App&&) = delete;

		///@}

	private:
		std::atomic<bool> running{true};
		std::unique_ptr<Server> server;
		bool showVersionsOnly{false};

		// helper functions
		bool getPassword(DatabaseSettings& dbSettings);
		bool inputLoop(DatabaseSettings& dbSettings, bool& isCancelTo);

		// static helper functions
		static void outputHeader(bool showLibraryVersions);
		static void checkArgumentNumber(int args);
		static void loadConfig(
				const std::string& fileName,
				ServerSettings& serverSettings,
				DatabaseSettings& dbSettings,
				NetworkSettings& networkSettings
		);
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_APP_HPP_ */
