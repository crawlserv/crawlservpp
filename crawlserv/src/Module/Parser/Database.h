/*
 * Database.h
 *
 * This class provides database functionality for a parser thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_H_
#define MODULE_PARSER_DATABASE_H_

#include "../DBThread.h"
#include "../DBWrapper.h"

#include "../../Struct/IdString.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Database : public crawlservpp::Module::DBWrapper {
	public:
		Database(crawlservpp::Module::DBThread& dbRef);
		virtual ~Database();

		// prepare target table and SQL statements for parser
		void initTargetTable(unsigned long websiteId, unsigned long listId, const std::string& websiteNameSpace,
				const std::string& urlListNameSpace, const std::string& tableName, const std::vector<std::string> * fields);
		bool prepare(unsigned long parserId, unsigned long websiteId, unsigned long listId, const std::string& tableName, bool reparse,
				bool verbose);

		// table function
		void lockUrlList();

		// URL functions
		bool isUrlParsed(unsigned long urlId);
		crawlservpp::Struct::IdString getNextUrl(unsigned long currentUrlId);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		bool isUrlLockable(unsigned long urlId);
		bool checkUrlLock(unsigned long urlId, const std::string& lockTime);
		std::string getUrlLock(unsigned long urlId);
		std::string lockUrl(unsigned long lockTimeout, unsigned long urlId);
		void unLockUrl(unsigned long urlId);

		// parsing functions
		bool getLatestContent(unsigned long urlId, unsigned long index, crawlservpp::Struct::IdString& contentTo);
		std::vector<crawlservpp::Struct::IdString> getAllContents(unsigned long urlId);
		void updateOrAddEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void setUrlFinished(unsigned long urlId);

	private:
		// table names
		std::string urlListTable;
		std::string targetTable;

		// pointer to field names
		const std::vector<std::string> * fieldNames;

		// prepared SQL statements
		unsigned short psIsUrlParsed;
		unsigned short psGetNextUrl;
		unsigned short psGetUrlPosition;
		unsigned short psGetNumberOfUrls;
		unsigned short psIsUrlLockable;
		unsigned short psCheckUrlLock;
		unsigned short psGetUrlLock;
		unsigned short psLockUrl;
		unsigned short psUnLockUrl;
		unsigned short psGetLatestContent;
		unsigned short psGetAllContents;
		unsigned short psSetUrlFinished;
		unsigned short psGetEntryId;
		unsigned short psUpdateEntry;
		unsigned short psAddEntry;
		unsigned short psUpdateParsedTable;

		// internal helper functions
		unsigned long getEntryId(unsigned long contentId);
		void updateEntry(unsigned long entryId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void addEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void updateParsedTable();
	};
}

#endif /* MODULE_PARSER_DATABASE_H_ */
