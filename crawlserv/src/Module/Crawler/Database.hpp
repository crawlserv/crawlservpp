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
 * This class provides database functionality for a crawler thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_DATABASE_HPP_
#define MODULE_CRAWLER_DATABASE_HPP_

#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Helper/Utf8.hpp"
#include "../../Main/Exception.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/Database.hpp"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <chrono>	// std::chrono
#include <cstddef>	// std::size_t
#include <cstdint>	// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <locale>	// std::locale
#include <memory>	// std::unique_ptr
#include <queue>	// std::queue
#include <sstream>	// std::ostringstream
#include <string>	// std::string, std::to_string
#include <utility>	// std::pair

namespace crawlservpp::Module::Crawler {

	class Database : public Wrapper::Database {
		// for convenience
		using IdString = std::pair<std::uint64_t, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		// constructor
		Database(Module::Database& dbRef);

		// destructor
		virtual ~Database();

		// setters
		void setRecrawl(bool isRecrawl);
		void setUrlCaseSensitive(bool isUrlCaseSensitive);
		void setUrlDebug(bool isUrlDebug);
		void setUrlStartupCheck(bool isUrlStartupCheck);

		// prepare SQL statements for crawler
		void prepare();

		// URL functions
		std::uint64_t getUrlId(const std::string& url);
		IdString getNextUrl(std::uint64_t currentUrlId);
		bool addUrlIfNotExists(const std::string& urlString, bool manual);
		std::size_t addUrlsIfNotExist(std::queue<std::string>& urls, bool manual);
		std::uint64_t addUrlGetId(const std::string& urlString, bool manual);
		std::uint64_t getUrlPosition(std::uint64_t urlId);
		std::uint64_t getNumberOfUrls();

		// URL checking functions
		void urlDuplicationCheck();
		void urlHashCheck();
		void urlEmptyCheck();
		void urlUtf8Check();

		// URL locking functions
		std::string getUrlLockTime(std::uint64_t urlId);
		bool isUrlCrawled(std::uint64_t urlId);
		std::string lockUrlIfOk(std::uint64_t urlId, const std::string& lockTime, std::uint32_t lockTimeout);
		void unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime);
		void setUrlFinishedIfOk(std::uint64_t urlId, const std::string& lockTime);

		// crawling functions
		void saveContent(
				std::uint64_t urlId,
				std::uint32_t response,
				const std::string& type,
				const std::string& content
		);
		void saveArchivedContent(
				std::uint64_t urlId,
				const std::string& timeStamp,
				std::uint32_t response,
				const std::string& type,
				const std::string& content);
		bool isArchivedContentExists(std::uint64_t urlId, const std::string& timeStamp);

		// constant strings for table aliases (public)
		const std::string crawlingTableAlias;
		const std::string urlListTableAlias;

		// class for Crawler::Database exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// options
		bool recrawl;
		bool urlCaseSensitive;
		bool urlDebug;
		bool urlStartupCheck;

		// table names
		std::string urlListTable;
		std::string crawlingTable;

	private:
		// IDs of prepared SQL statements
		struct _ps {
			std::uint16_t getUrlId;
			std::uint16_t getNextUrl;
			std::uint16_t addUrlIfNotExists;
			std::uint16_t add10UrlsIfNotExist;
			std::uint16_t add100UrlsIfNotExist;
			std::uint16_t add1000UrlsIfNotExist;
			std::uint16_t getUrlPosition;
			std::uint16_t getNumberOfUrls;
			std::uint16_t getUrlLockTime;
			std::uint16_t isUrlCrawled;
			std::uint16_t addUrlLockIfOk;
			std::uint16_t renewUrlLockIfOk;
			std::uint16_t unLockUrlIfOk;
			std::uint16_t setUrlFinishedIfOk;
			std::uint16_t saveContent;
			std::uint16_t saveArchivedContent;
			std::uint16_t isArchivedContentExists;
			std::uint16_t urlDuplicationCheck;
			std::uint16_t urlHashCheck;
			std::uint16_t urlHashCorrect;
			std::uint16_t urlEmptyCheck;
			std::uint16_t getUrls;
			std::uint16_t removeDuplicates;
		} ps;

		// helper functions
		std::string queryAddUrlsIfNotExist(std::size_t numberOfUrls, const std::string& hashQuery);
		std::queue<std::string> getUrls();
		std::uint32_t removeDuplicates(const std::string& url);
	};

	} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_DATABASE_HPP_ */
