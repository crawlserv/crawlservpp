/*
 * Database.h
 *
 * This class provides database functionality for a crawler thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_DATABASE_H_
#define MODULE_CRAWLER_DATABASE_H_

#include "../Database.h"

#include "../../Struct/UrlProperties.h"
#include "../../Wrapper/Database.h"
#include "../../Wrapper/TableLock.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace crawlservpp::Module::Crawler {
	class Database : public Wrapper::Database {
		// for convenience
		typedef Main::Database::Exception DatabaseException;
		typedef Struct::UrlProperties UrlProperties;
		typedef Wrapper::TableLock<Wrapper::Database> TableLock;

		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

	public:
		// constructor
		Database(Module::Database& dbRef);

		// destructor
		virtual ~Database();

		// setters
		void setId(unsigned long crawlerId);
		void setWebsiteNamespace(const std::string& websiteNamespace);
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
		bool isUrlExists(const std::string& urlString);
		void getUrlIdLockId(UrlProperties& urlProperties);
		bool isUrlCrawled(unsigned long crawlingId);
		UrlProperties getNextUrl(unsigned long currentUrlId);
		void addUrl(const std::string& urlString, bool manual);
		void addUrls(const std::vector<std::string>& urls);
		unsigned long addUrlGetId(const std::string& urlString, bool manual);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL checking functions
		void urlDuplicationCheck();
		void urlHashCheck();

		// URL locking functions
		bool isUrlLockable(unsigned long lockId);
		bool checkUrlLock(unsigned long lockId, const std::string& lockTime);
		std::string getUrlLock(unsigned long lockId);
		void getUrlLockId(UrlProperties& urlProperties);
		std::string lockUrl(UrlProperties& urlProperties, unsigned long lockTimeout);
		void unLockUrl(unsigned long lockId);

		// crawling functions
		void saveContent(unsigned long urlId, unsigned int response, const std::string& type, const std::string& content);
		void saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response, const std::string& type,
				const std::string& content);
		void setUrlFinished(unsigned long crawlingId);
		bool isArchivedContentExists(unsigned long urlId, const std::string& timeStamp);

		// helper functions (using multiple database commands)
		bool renewUrlLock(unsigned long lockTimeout, UrlProperties& urlProperties,
				std::string& lockTime);

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
			unsigned short isUrlExists;
			unsigned short getUrlIdLockId;
			unsigned short isUrlCrawled;
			unsigned short getNextUrl;
			unsigned short addUrl;
			unsigned short add10Urls;
			unsigned short add100Urls;
			unsigned short add1000Urls;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short isUrlLockable;
			unsigned short checkUrlLock;
			unsigned short getUrlLock;
			unsigned short getUrlLockId;
			unsigned short lockUrl;
			unsigned short addUrlLock;
			unsigned short unLockUrl;
			unsigned short saveContent;
			unsigned short saveArchivedContent;
			unsigned short setUrlFinished;
			unsigned short isArchivedContentExists;
			unsigned short urlDuplicationCheck;
			unsigned short urlHashCheck;
		} ps;

		// helper function
		std::string queryAddUrls(unsigned int numberOfUrls, const std::string& hashQuery);
	};
}

#endif /* MODULE_CRAWLER_DATABASE_H_ */
