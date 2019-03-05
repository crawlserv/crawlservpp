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

#include "../../Wrapper/Database.h"

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
	class Database : public crawlservpp::Wrapper::Database {
		// for convenience
		typedef crawlservpp::Main::Database::Exception DatabaseException;

	public:
		// constructor
		Database(crawlservpp::Module::Database& dbRef);

		// destructor
		virtual ~Database();

		// setters
		void setId(unsigned long crawlerId);
		void setWebsiteNamespace(const std::string& websiteNamespace);
		void setUrlListNamespace(const std::string& urlListNamespace);
		void setRecrawl(bool isRecrawl);
		void setLogging(bool isLogging);
		void setVerbose(bool isVerbose);

		// prepare SQL statements for crawler
		void prepare();

		// table function
		void lockUrlList();

		// URL functions
		bool isUrlExists(const std::string& urlString);
		unsigned long getUrlId(const std::string& urlString);
		bool isUrlCrawled(unsigned long urlId);
		std::pair<unsigned long, std::string> getNextUrl(unsigned long currentUrlId);
		unsigned long addUrl(const std::string& urlString, bool manual);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		bool isUrlLockable(unsigned long urlId);
		bool checkUrlLock(unsigned long urlId, const std::string& lockTime);
		std::string getUrlLock(unsigned long urlId);
		std::string lockUrl(unsigned long lockTimeout, unsigned long urlId);
		void unLockUrl(unsigned long urlId);

		// crawling functions
		void saveContent(unsigned long urlId, unsigned int response, const std::string& type, const std::string& content);
		void saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response, const std::string& type,
				const std::string& content);
		void setUrlFinished(unsigned long urlId);
		bool isArchivedContentExists(unsigned long urlId, const std::string& timeStamp);

		// helper functions (using multiple database commands)
		bool renewUrlLock(unsigned long lockTimeout, unsigned long urlId, std::string& lockTime);

	protected:
		// options
		std::string idString;
		std::string websiteName;
		std::string urlListName;
		bool recrawl;
		bool logging;
		bool verbose;
		std::string urlListTable;
		std::string linkTable;

	private:
		// IDs of prepared SQL statements
		struct {
			unsigned short isUrlExists;
			unsigned short isUrlHashExists;
			unsigned short getUrlId;
			unsigned short isUrlCrawled;
			unsigned short getNextUrl;
			unsigned short addUrl;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short isUrlLockable;
			unsigned short checkUrlLock;
			unsigned short getUrlLock;
			unsigned short lockUrl;
			unsigned short unLockUrl;
			unsigned short saveContent;
			unsigned short saveArchivedContent;
			unsigned short setUrlFinished;
			unsigned short isArchivedContentExists;
		} ps;
	};
}

#endif /* MODULE_CRAWLER_DATABASE_H_ */
