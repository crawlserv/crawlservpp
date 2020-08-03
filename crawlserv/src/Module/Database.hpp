/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Database.hpp
 *
 * Database functionality for a single thread.
 *
 * Only implements module-independent functionality. For module-specific functionality use the
 *  child classes of the Wrapper::Database interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_DATABASE_HPP_
#define MODULE_DATABASE_HPP_

#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/ModuleOptions.hpp"

#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint64_t
#include <fstream>		// std::flush, std::ofstream
#include <limits>		// std::numeric_limits
#include <memory>		// std::unique_ptr
#include <queue>		// std::queue
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view

namespace crawlservpp::Wrapper {

	class Database;

} /* namespace crawlservpp::Wrapper */

namespace crawlservpp::Module {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! First argument in a SQL query.
	constexpr auto sqlArg1{1};

	//! Second argument in a SQL query.
	constexpr auto sqlArg2{2};

	//! Third argument in a SQL query.
	constexpr auto sqlArg3{3};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class handling database access for threads.
	/*!
	 * Only implements module-independent functionality.
	 * For module-specific functionality, use the child
	 * classes of Wrapper::Database instead.
	 */
	class Database : public Main::Database {
		//! Allows access to module threads.
		friend class Wrapper::Database;

		// for convenience
		using DatabaseSettings = Struct::DatabaseSettings;
		using ModuleOptions = Struct::ModuleOptions;

		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		///@name Construction and Destruction
		///@{

		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);
		~Database() override;

		///@}
		///@name Setters (Module)
		///@{

		void setOptions(const ModuleOptions& moduleOptions);
		void setThreadId(std::uint64_t id);
		void setLogging(std::uint8_t level, std::uint8_t min, std::uint8_t verbose);

		///@}
		///@name Preparation (Module)
		///@{

		void prepare();

		///@}
		///@name Logging (Module)
		///@{

		void log(std::uint8_t level, const std::string& logEntry);
		void log(std::uint8_t level, std::queue<std::string>& logEntries);
		bool isLogLevel(uint8_t level) const;

		///@}
		///@name Threads (Module)
		///@{

		void setThreadStatusMessage(std::uint64_t threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadProgress(std::uint64_t threadId, float threadProgress, std::uint64_t threadRunTime);
		void setThreadLast(std::uint64_t threadId, std::uint64_t threadLast);

		///@}

		//! Class for Module::Database exceptions.
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Database(Database&) = delete;

		//! Deleted copy assignment operator.
		Database& operator=(Database&) = delete;

		//! Deleted move constructor.
		Database(Database&&) = delete;

		//! Deleted move assignment operator.
		Database& operator=(Database&&) = delete;

		///@}

	private:
		// general thread options
		ModuleOptions options;
		std::string threadIdString;
		std::string websiteIdString;
		std::string urlListIdString;
		std::uint8_t loggingLevel{std::numeric_limits<std::uint8_t>::max() - 1};
		std::uint8_t loggingMin{1};
		std::uint8_t loggingVerbose{std::numeric_limits<std::uint8_t>::max()};
		std::ofstream loggingFile;
		bool debugLogging{false};
		const std::string_view debugDir;

		// private helper function
		void initDebugLogging();

		// IDs of prepared SQL statements
		struct _ps {
			std::uint16_t setThreadStatusMessage;
			std::uint16_t setThreadProgress;
			std::uint16_t setThreadLast;
		} ps{};
	};

} /* namespace crawlservpp::Module */

#endif /* MODULE_DATABASE_HPP_ */
