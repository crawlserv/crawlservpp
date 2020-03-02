/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
#include <locale>	// std::locale
#include <memory>	// std::unique_ptr
#include <queue>	// std::queue
#include <sstream>	// std::ostringstream
#include <string>	// std::string, std::to_string
#include <utility>	// std::pair

namespace crawlservpp::Module::Crawler {

	class Database : public Wrapper::Database {
		// for convenience
		using IdString = std::pair<unsigned long, std::string>;
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
		unsigned long getUrlId(const std::string& url);
		IdString getNextUrl(unsigned long currentUrlId);
		bool addUrlIfNotExists(const std::string& urlString, bool manual);
		unsigned long addUrlsIfNotExist(std::queue<std::string>& urls, bool manual);
		unsigned long addUrlGetId(const std::string& urlString, bool manual);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL checking functions
		void urlDuplicationCheck();
		void urlHashCheck();
		void urlEmptyCheck();
		void urlUtf8Check();

		// URL locking functions
		std::string getUrlLockTime(unsigned long urlId);
		bool isUrlCrawled(unsigned long urlId);
		std::string lockUrlIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout);
		void unLockUrlIfOk(unsigned long urlId, const std::string& lockTime);
		void setUrlFinishedIfOk(unsigned long urlId, const std::string& lockTime);

		// crawling functions
		void saveContent(
				unsigned long urlId,
				unsigned int response,
				const std::string& type,
				const std::string& content
		);
		void saveArchivedContent(
				unsigned long urlId,
				const std::string& timeStamp,
				unsigned int response,
				const std::string& type,
				const std::string& content);
		bool isArchivedContentExists(unsigned long urlId, const std::string& timeStamp);

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
			unsigned short getUrlId;
			unsigned short getNextUrl;
			unsigned short addUrlIfNotExists;
			unsigned short add10UrlsIfNotExist;
			unsigned short add100UrlsIfNotExist;
			unsigned short add1000UrlsIfNotExist;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short getUrlLockTime;
			unsigned short isUrlCrawled;
			unsigned short addUrlLockIfOk;
			unsigned short renewUrlLockIfOk;
			unsigned short unLockUrlIfOk;
			unsigned short setUrlFinishedIfOk;
			unsigned short saveContent;
			unsigned short saveArchivedContent;
			unsigned short isArchivedContentExists;
			unsigned short urlDuplicationCheck;
			unsigned short urlHashCheck;
			unsigned short urlHashCorrect;
			unsigned short urlEmptyCheck;
			unsigned short getUrls;
			unsigned short removeDuplicates;
		} ps;

		// helper functions
		std::string queryAddUrlsIfNotExist(unsigned int numberOfUrls, const std::string& hashQuery);
		std::queue<std::string> getUrls();
		unsigned int removeDuplicates(const std::string& url);
	};

	} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_DATABASE_HPP_ */
