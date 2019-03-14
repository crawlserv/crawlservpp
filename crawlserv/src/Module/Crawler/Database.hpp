/*
 * Database.hpp
 *
 * This class provides database functionality for a crawler thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_DATABASE_HPP_
#define MODULE_CRAWLER_DATABASE_HPP_

#include "../../Struct/TableLockProperties.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/TableLock.hpp"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <utility>

namespace crawlservpp::Module::Crawler {

	class Database : public Wrapper::Database {
		// for convenience
		typedef Main::Database::Exception DatabaseException;
		typedef Struct::TableLockProperties TableLockProperties;
		typedef Wrapper::TableLock<Wrapper::Database> TableLock;

		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

	public:
		// constructor
		Database(Module::Database& dbRef);

		// destructor
		virtual ~Database();

		// setters
		void setId(unsigned long crawlerId);
		void setNamespaces(const std::string& website, const std::string& urlList);
		void setUrlListNamespace(const std::string& urlListNamespace);
		void setRecrawl(bool isRecrawl);
		void setLogging(bool isLogging);
		void setVerbose(bool isVerbose);
		void setUrlCaseSensitive(bool isUrlCaseSensitive);
		void setUrlDebug(bool isUrlDebug);
		void setUrlStartupCheck(bool isUrlStartupCheck);

		// prepare SQL statements for crawler
		void prepare();

		// URL functions
		unsigned long getUrlId(const std::string& url);

		IdString getNextUrl(unsigned long currentUrlId);
		bool addUrlIfNotExists(const std::string& urlString, bool manual);
		unsigned long addUrlsIfNotExist(std::queue<std::string, std::deque<std::string>>& urls);
		unsigned long addUrlGetId(const std::string& urlString, bool manual);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL checking functions
		void urlDuplicationCheck();
		void urlHashCheck();
		void urlEmptyCheck(const std::vector<std::string>& urlsAdded);

		// URL locking functions
		std::string getUrlLock(unsigned long urlId);
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

	protected:
		// options
		std::string idString;
		std::string websiteName;
		std::string urlListName;
		bool recrawl;
		bool logging;
		bool verbose;
		bool urlCaseSensitive;
		bool urlDebug;
		bool urlStartupCheck;

		// table names
		std::string urlListTable;
		std::string crawlingTable;

	private:
		// IDs of prepared SQL statements
		struct {
			unsigned short getUrlId;
			unsigned short getNextUrl;
			unsigned short addUrlIfNotExists;
			unsigned short add10UrlsIfNotExist;
			unsigned short add100UrlsIfNotExist;
			unsigned short add1000UrlsIfNotExist;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short getUrlLock;
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
			unsigned short urlEmptyCheck;
		} ps;

		// helper function
		std::string queryAddUrlsIfNotExist(unsigned int numberOfUrls, const std::string& hashQuery);
	};

	} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_DATABASE_HPP_ */
