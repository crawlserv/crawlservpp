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
 * Server.hpp
 *
 * The command-and-control server.
 *
 * Uses the mongoose and RapidJSON libraries to implement a HTTP server
 *  for receiving JSON-formatted commands and sending JSON-formatted
 *  replies from/to the crawlserv_frontend.
 *
 *  Also handles all threads for the different modules as well as
 *   specific worker threads for specific server tasks.
 *
 *  Created on: Oct 7, 2018
 *      Author: ans
 */

#ifndef MAIN_SERVER_HPP_
#define MAIN_SERVER_HPP_

/*
 * DEBUGGING
 */

// directives that allow to deactivate whole components for debugging purposes ONLY
#ifndef NDEBUG
	//#define MAIN_SERVER_DEBUG_NOCRAWLERS
	//#define MAIN_SERVER_DEBUG_NOPARSERS
	//#define MAIN_SERVER_DEBUG_NOEXTRACTORS
	//#define MAIN_SERVER_DEBUG_NOANALYZERS
#endif

/*
 * MACROS
 */

// macro for server commands

//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_CMD(X, Y)	if(name == (X)) { \
										response = Y(); \
										return true; \
									}


// macros for exception handling of worker threads

//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_WORKER_BEGIN	try {
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_WORKER_END(X)	} \
									catch(const std::exception& e) { \
										(X) = ServerCommandResponse::failed( \
												e.what() \
										); \
									}

/*
 * INCLUDES
 */

#include "Database.hpp"
#include "Exception.hpp"
#include "WebServer.hpp"

#include "../Data/Compression/Gzip.hpp"
#include "../Data/Compression/Zip.hpp"
#include "../Data/Compression/Zlib.hpp"
#include "../Data/File.hpp"
#include "../Data/ImportExport/OpenDocument.hpp"
#include "../Data/ImportExport/Text.hpp"
#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Module/Analyzer/Algo/All.hpp"
#include "../Module/Crawler/Thread.hpp"
#include "../Module/Extractor/Thread.hpp"
#include "../Module/Parser/Thread.hpp"
#include "../Module/Thread.hpp"
#include "../Query/JsonPath.hpp"
#include "../Query/JsonPointer.hpp"
#include "../Query/RegEx.hpp"
#include "../Query/XPath.hpp"
#include "../Struct/AlgoThreadProperties.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/NetworkSettings.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/ServerCommandResponse.hpp"
#include "../Struct/ServerSettings.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Timer/SimpleHR.hpp"
#include "../Wrapper/Database.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::count, std::count_if, std::find_if, std::remove_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint16_t, std::uint32_t, std::uint64_t
#include <exception>	// std::exception
#include <iostream>		// std::cout, std::flush
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <optional>		// std::nullopt, std::optional
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <thread>		// std::thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Main {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The name of the (sub-)directory for the file cache.
	inline constexpr auto cacheDir{"cache"sv};

	//! The name of the (sub-)directory for cookies.
	inline constexpr auto cookieDir{"cookies"sv};

	//! The name of the (sub-)directory for downloads.
	inline constexpr auto downloadDir{"dl"sv};

	//! The name of the (sub-)directory for debugging.
	inline constexpr auto debugDir{"debug"sv};

	//! The name of the (sub-)directory for dictionaries.
	inline constexpr auto dictDir{"dict"sv};

	//! The name of the (sub-)directory for language models.
	inline constexpr auto mdlDir{"mdl"sv};

	//! The timeout in milliseconds for the polling of the web server.
	inline constexpr auto webServerPollTimeOutMs{1000};

	//! The HTTP status code for GET replies indicating the status of the server.
	inline constexpr auto statusHttpCode{200};

	//! The HTTP content type for GET replies indicating the status of the server.
	inline constexpr auto statusHttpContentType{"text/plain"};

	//! The HTTP status code for POST replies.
	inline constexpr auto replyHttpCode{200};

	//! The HTTP content type for POST replies.
	inline constexpr auto replyHttpContentType{"application/json"};

	//! The HTTP status code for OPTIONS replies.
	inline constexpr auto optionsHttpCode{200};

	//! The minimum length of namespaces.
	inline constexpr auto minNameSpaceLength{3};

	//! The minimum length of namespaces, as string.
	inline constexpr auto minNameSpaceLengthString{"three"sv};

	//! The beginning of URLs using the HTTP protocol.
	inline constexpr auto httpString{"http://"sv};

	//! The beginning of URLs using the HTTPS protocol.
	inline constexpr auto httpsString{"https://"sv};

	//! The number of XML warnings by default.
	inline constexpr auto xmlWarningsDefault{25};

	///@}

	/*
	 * DECLARATION
	 */

	//! The command-and-control server.
	/*!
	 * Uses the Main::WebServer and the @c rapidJSON
	 *  library to implement a HTTP server for
	 *  receiving JSON-formatted commands and
	 *  sending JSON-formatted replies from/to the
	 *  @c crawlserv_frontend(s).
	 *
	 * Also handles the threads for the different modules
	 *  as well as worker threads for specific server
	 *  tasks.
	 *
	 * For more information about the @c rapidJSON library,
	 *  see its <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \sa Main::WebServer
	 */
	class Server final {
		// for convenience
		using DatabaseException = Database::Exception;
		using IncorrectPathException = Database::IncorrectPathException;
		using PrivilegesException = Database::PrivilegesException;
		using StorageEngineException = Database::StorageEngineException;
		using WrongArgumentsException = Database::WrongArgumentsException;

		using DateTimeException = Helper::DateTime::Exception;
		using JsonException = Helper::Json::Exception;
		using LocaleException = Helper::DateTime::LocaleException;

		using XMLException = Parsing::XML::Exception;

		using JSONPathException = Query::JsonPath::Exception;
		using JSONPointerException = Query::JsonPointer::Exception;
		using RegExException = Query::RegEx::Exception;
		using XPathException = Query::XPath::Exception;

		using AlgoThreadProperties = Struct::AlgoThreadProperties;
		using ConfigProperties = Struct::ConfigProperties;
		using DatabaseSettings = Struct::DatabaseSettings;
		using NetworkSettings = Struct::NetworkSettings;
		using QueryProperties = Struct::QueryProperties;
		using ServerCommandResponse = Struct::ServerCommandResponse;
		using ServerSettings = Struct::ServerSettings;
		using ThreadDatabaseEntry = Struct::ThreadDatabaseEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using UrlListProperties = Struct::UrlListProperties;
		using WebsiteProperties = Struct::WebsiteProperties;

		using ConnectionPtr = mg_connection *;
		using ConstConnectionPtr = const mg_connection *;
		using StringString = std::pair<std::string, std::string>;
		using Queries = std::vector<std::pair<std::string, std::vector<StringString>>>;

	public:
		///@name Construction and Destruction
		///@{

		Server(
				const ServerSettings& serverSettings,
				const DatabaseSettings& databaseSettings,
				const NetworkSettings& networkSettings
		);
		virtual ~Server();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] const std::string& getStatus() const;
		[[nodiscard]] std::int64_t getUpTime() const;
		[[nodiscard]] std::size_t getActiveThreads() const;
		[[nodiscard]] std::size_t getActiveWorkers() const;

		///@}
		///@name Server Tick
		///@{

		bool tick();

		///@}
		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Server(Server&) = delete;

		//! Deleted copy assignment operator.
		Server& operator=(Server&) = delete;

		//! Deleted move constructor.
		Server(Server&&) = delete;

		//! Deleted move assignment operator.
		Server& operator=(Server&&) = delete;

		///@}

	private:
		// settings
		ServerSettings settings;
		DatabaseSettings dbSettings;
		NetworkSettings netSettings;

		// database
		Database database;

		// status
		std::string status;
		std::string allowed;
		bool running{true};
		std::chrono::steady_clock::time_point uptimeStart;
		bool offline{true};

		// threads
		std::vector<std::unique_ptr<Module::Crawler::Thread>> crawlers;
		std::vector<std::unique_ptr<Module::Parser::Thread>> parsers;
		std::vector<std::unique_ptr<Module::Extractor::Thread>> extractors;
		std::vector<std::unique_ptr<Module::Analyzer::Thread>> analyzers;
		std::vector<std::thread> workers;
		std::vector<bool> workersRunning;
		mutable std::mutex workersLock;

		// pointer to information for basic commands
		rapidjson::Document cmdJson;
		std::string cmdIp;

		/*
		 * NOTE: The web server needs to be declared after/destroyed before the database and any data,
		 * 		  because it is doing one last poll on destruction!
		*/

		// web server
		WebServer webServer;

		// setter
		void setStatus(const std::string& statusMsg);

		// event handlers
		void onAccept(ConnectionPtr connection);
		void onRequest(
				ConnectionPtr connection,
				const std::string& method,
				const std::string& body,
				void * data
		);

		// server commands
		[[nodiscard]] std::string cmd(
				ConnectionPtr connection,
				const std::string& msgBody,
				bool& threadStartedTo,
				bool& fileDownloadTo
		);
		bool cmd(const std::string& name, ServerCommandResponse& response);

		[[nodiscard]] ServerCommandResponse cmdKill();
		[[nodiscard]] ServerCommandResponse cmdAllow();
		[[nodiscard]] ServerCommandResponse cmdDisallow();

		[[nodiscard]] ServerCommandResponse cmdLog();
		[[nodiscard]] ServerCommandResponse cmdClearLogs();

		[[nodiscard]] ServerCommandResponse cmdStartCrawler();
		[[nodiscard]] ServerCommandResponse cmdPauseCrawler();
		[[nodiscard]] ServerCommandResponse cmdUnpauseCrawler();
		[[nodiscard]] ServerCommandResponse cmdStopCrawler();

		[[nodiscard]] ServerCommandResponse cmdStartParser();
		[[nodiscard]] ServerCommandResponse cmdPauseParser();
		[[nodiscard]] ServerCommandResponse cmdUnpauseParser();
		[[nodiscard]] ServerCommandResponse cmdStopParser();
		[[nodiscard]] ServerCommandResponse cmdResetParsingStatus();

		[[nodiscard]] ServerCommandResponse cmdStartExtractor();
		[[nodiscard]] ServerCommandResponse cmdPauseExtractor();
		[[nodiscard]] ServerCommandResponse cmdUnpauseExtractor();
		[[nodiscard]] ServerCommandResponse cmdStopExtractor();
		[[nodiscard]] ServerCommandResponse cmdResetExtractingStatus();

		[[nodiscard]] ServerCommandResponse cmdStartAnalyzer();
		[[nodiscard]] ServerCommandResponse cmdPauseAnalyzer();
		[[nodiscard]] ServerCommandResponse cmdUnpauseAnalyzer();
		[[nodiscard]] ServerCommandResponse cmdStopAnalyzer();
		[[nodiscard]] ServerCommandResponse cmdResetAnalyzingStatus();

		[[nodiscard]] ServerCommandResponse cmdPauseAll();
		[[nodiscard]] ServerCommandResponse cmdUnpauseAll();

		[[nodiscard]] ServerCommandResponse cmdAddWebsite();
		[[nodiscard]] ServerCommandResponse cmdUpdateWebsite();
		[[nodiscard]] ServerCommandResponse cmdDeleteWebsite();
		[[nodiscard]] ServerCommandResponse cmdDuplicateWebsite();

		[[nodiscard]] ServerCommandResponse cmdAddUrlList();
		[[nodiscard]] ServerCommandResponse cmdUpdateUrlList();
		[[nodiscard]] ServerCommandResponse cmdDeleteUrlList();

		[[nodiscard]] ServerCommandResponse cmdAddQuery();
		[[nodiscard]] ServerCommandResponse cmdUpdateQuery();
		[[nodiscard]] ServerCommandResponse cmdMoveQuery();
		[[nodiscard]] ServerCommandResponse cmdDeleteQuery();
		[[nodiscard]] ServerCommandResponse cmdDuplicateQuery();

		[[nodiscard]] ServerCommandResponse cmdAddConfig();
		[[nodiscard]] ServerCommandResponse cmdUpdateConfig();
		[[nodiscard]] ServerCommandResponse cmdDeleteConfig();
		[[nodiscard]] ServerCommandResponse cmdDuplicateConfig();

		[[nodiscard]] static ServerCommandResponse cmdListDicts();
		[[nodiscard]] static ServerCommandResponse cmdListMdls();

		[[nodiscard]] ServerCommandResponse cmdWarp();

		[[nodiscard]] ServerCommandResponse cmdDownload();

		void cmdImport(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);
		void cmdMerge(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);
		void cmdExport(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		void cmdDeleteUrls(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		void cmdTestQuery(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		// server initialization
		static void initCacheDir();
		static void initDir(std::string_view directory);
		static void initDebuggingDir(bool isEnabled, std::string_view directory);
		void initDatabase(std::uint16_t sleepOnSqlErrorS);
		void initCallbacks();
		void initWebServer(const std::string& port, const std::string& corsOrigins);
		void initThreads();
		void initStartLogging();

		// server cleanup
		void clearModuleThreads();
		void clearWorkerThreads();
		void clearLogShutdown();

		// server tick
		void tickPollWebServer();
		void tickRemoveFinishedModuleThreads();
		void tickRemoveFinishedWorkerThreads();
		void tickReconnectIfOffline();

		// internal helper functions
		void continueParserThread(const ThreadDatabaseEntry& entry);
		void continueAnalyzerThread(const ThreadDatabaseEntry& entry);
		static std::string getIp(ConstConnectionPtr connection, std::string_view function);
		bool isWebsiteInUse(std::uint64_t website, ServerCommandResponse& responseTo) const;
		bool isUrlListInUse(std::uint64_t urlList, ServerCommandResponse& responseTo) const;
		bool checkConfig(std::uint64_t config, ServerCommandResponse& responseTo);
		bool checkConfig(std::uint64_t website, std::uint64_t config, ServerCommandResponse& responseTo);

		bool getArgument(
				const std::string& name,
				std::string& out,
				bool optional,
				bool notEmpty,
				std::string& outError
		);
		bool getArgument(
				const std::string& name,
				std::uint64_t& out,
				std::string& outError
		);
		bool getArgument(
				const std::string& name,
				bool& out,
				bool optional,
				std::string& outError
		);

		void workerEnd(
				std::size_t threadIndex,
				ConnectionPtr connection,
				const std::string& message,
				const ServerCommandResponse& response
		);

		void initWorkerDatabase(Module::Database& db);

		// internal static helper functions
		[[nodiscard]] static bool workerBegin(
				const std::string& message,
				rapidjson::Document& json,
				ServerCommandResponse& response
		);

		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				std::string& out,
				bool optional,
				bool notEmpty,
				std::string& outError
		);
		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				std::uint64_t& out,
				std::string& outError
		);
		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				bool& out,
				bool optional,
				std::string& outError
		);

		static void correctDomain(std::string& inOut);

		[[nodiscard]] static bool checkNameSpace(
				const std::string& name,
				ServerCommandResponse& responseTo
		);

		[[nodiscard]] static std::uint32_t getAlgoFromConfig(
				const rapidjson::Document& json
		);

		[[nodiscard]] static std::string generateReply(
				const ServerCommandResponse& response,
				const std::string& msgBody
		);

		[[nodiscard]] static std::string dateTimeTest(
				const std::string& input,
				const std::string& format,
				const std::string& locale
		);

		static bool cmdExportGetArguments(
				const rapidjson::Document& json,
				std::string& dataTypeTo,
				std::string& fileTypeTo,
				std::string& compressionTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportRetrieveAndConvert(
				const rapidjson::Document& json,
				const std::string& dataType,
				const std::string& fileType,
				Module::Database& db,
				std::string& contentTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportCompress(
				const std::string& dataType,
				const std::string& compression,
				std::string contentInOut,
				ServerCommandResponse& responseTo
		);
		static void cmdExportWrite(
				const std::string& content,
				ServerCommandResponse& responseTo
		);
		static void cmdExportLogSuccess(
				Module::Database& db,
				std::size_t size,
				const std::string& timeString
		);
		static bool cmdExportRetrieveUrlList(
				const rapidjson::Document& json,
				Module::Database& db,
				std::queue<std::string>& urlsTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportRetrieveTable(
				std::string_view type,
				const rapidjson::Document& json,
				Module::Database& db,
				std::string& nameTo,
				std::vector<std::vector<std::string>>& contentTo,
				bool& isColumnNamesTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportGetUrlListArguments(
				const rapidjson::Document& json,
				std::uint64_t& websiteTo,
				std::uint64_t& urlListTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportGetTableArguments(
				const rapidjson::Document& json,
				std::uint64_t& websiteTo,
				std::uint64_t& urlListTo,
				std::uint64_t& sourceTableTo,
				bool& isColumnNamesTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportCheckWebsiteUrlList(
				Module::Database& db,
				std::uint64_t websiteId,
				std::uint64_t urlListId,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportCheckTargetTable(
				Module::Database& db,
				std::string_view dataType,
				std::uint64_t websiteId,
				std::uint64_t urlListId,
				std::uint64_t tableId,
				ServerCommandResponse& responseTo
		);
		static void cmdExportGetTableContent(
				Module::Database& db,
				std::string_view dataType,
				std::uint64_t websiteId,
				std::uint64_t urlListId,
				std::uint64_t tableId,
				std::string& nameTo,
				std::vector<std::vector<std::string>>& contentTo,
				bool isIncludeColumnNames
		);
		static void cmdExportLog(
				Module::Database& db,
				std::string_view entryType,
				std::string_view entryTypes,
				std::string_view listType,
				std::uint64_t entryNum
		);
		static bool cmdExportUrlListAsText(
				const rapidjson::Document& json,
				std::queue<std::string>& data,
				std::string& contentTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdExportUrlListAsSpreadsheet(
				const rapidjson::Document& json,
				std::queue<std::string>& data,
				std::string& contentTo,
				ServerCommandResponse& responseTo
		);
		static void cmdExportTableAsSpreadsheet(
				const std::string& tableName,
				const std::vector<std::vector<std::string>>& tableContent,
				std::string& contentTo,
				bool isColumnNames
		);
		static bool cmdExportGetFirstLineHeader(
				const rapidjson::Document& json,
				std::optional<std::string>& optHeaderTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdDeleteUrlsGetArguments(
				const rapidjson::Document& json,
				std::uint64_t& urlListTo,
				std::uint64_t& queryTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdDeleteUrlsGetWebsite(
				Module::Database& db,
				std::uint64_t urlList,
				std::uint64_t& websiteTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdDeleteUrlsGetQuery(
				Module::Database& db,
				std::uint64_t query,
				std::string& regExTo,
				ServerCommandResponse& responseTo
		);
		static bool cmdDeleteUrlsGetUrls(
				Module::Database& db,
				std::uint64_t urlList,
				const std::string& regEx,
				std::queue<std::uint64_t>& toDeleteTo,
				ServerCommandResponse& responseTo
		);
		static ServerCommandResponse cmdDeleteUrlsDelete(
				Module::Database& db,
				std::uint64_t urlList,
				std::queue<std::uint64_t>& toDelete
		);
		static ServerCommandResponse cmdDeleteUrlsConfirm(std::size_t number);

		// static template helper functions for different kinds of threats
		template<typename T> static void interruptModuleThreads(
				std::vector<std::unique_ptr<T>>& threads
		) {
			for(auto& thread : threads) {
				thread->Module::Thread::interrupt();
			}
		}

		template<typename T> static void waitForModuleThreads(
				std::vector<std::unique_ptr<T>>& threads,
				std::string_view moduleName,
				std::queue<std::string>& logEntriesTo
		) {
			for(auto& thread : threads) {
				if(thread) {
					// save the ID of the thread before ending it
					const auto id{thread->getId()};

					// wait for thread
					thread->Module::Thread::end();

					// log interruption
					std::string logString{moduleName};

					logString += " #";
					logString += std::to_string(id);
					logString += " interrupted.";

					logEntriesTo.emplace(logString);
				}
			}

			threads.clear();
		}

		template<typename T> static std::size_t countModuleThreads(
				const std::vector<std::unique_ptr<T>>& threads
		) {
			return std::count_if(
					threads.cbegin(),
					threads.cend(),
					[](const auto& thread) {
							return thread->isRunning();
					}
			);
		}

		template<typename T> static void removeFinishedModuleThreads(
				std::vector<std::unique_ptr<T>>& threads
		) {
			threads.erase(
					std::remove_if(
							threads.begin(),
							threads.end(),
							[](auto& crawler) {
								if(crawler->isShutdown() && crawler->isFinished()) {
									crawler->Module::Thread::end();

									return true;
								}

								return false;
							}
					),
					threads.end()
			);
		}

		template<typename T> void continueModuleThread( /* (for analyzer and extractor only) */
				const ThreadDatabaseEntry& entry,
				std::vector<std::unique_ptr<T>>& to
		) {
			to.push_back(
					std::make_unique<T>(
							this->database,
							cookieDir,
							entry.options,
							this->netSettings,
							entry.status
					)
			);

			to.back()->Module::Thread::start();

			// write to log
			this->database.log(
					entry.options.module
					+ " #"
					+ std::to_string(entry.status.id)
					+ " continued."
			);
		}

		// static template helper functions for different kinds of database connections
		template<typename DB> static bool checkWebsite(
				DB& db,
				std::uint64_t website,
				ServerCommandResponse& responseTo
		) {
			if(db.isWebsite(website)) {
				return true;
			}

			responseTo = ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(website)
					+ " not found."
			);

			return false;
		}

		template<typename DB> static bool checkUrlList(
				DB& db,
				std::uint64_t urlList,
				ServerCommandResponse& responseTo
		) {
			if(db.isUrlList(urlList)) {
				return true;
			}

			responseTo = ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(urlList)
					+ " not found."
			);

			return false;
		}

		template<typename DB> static bool checkUrlList(
				DB& db,
				std::uint64_t website,
				std::uint64_t urlList,
				ServerCommandResponse& responseTo
		) {
			if(db.isUrlList(website, urlList)) {
				return true;
			}

			responseTo = ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(urlList)
					+ " for website #"
					+ std::to_string(website)
					+ " not found."
			);

			return false;
		}

		template<typename DB> static bool checkQuery(
				DB& db,
				std::uint64_t queryId,
				ServerCommandResponse& responseTo
		) {
			if(db.isQuery(queryId)) {
				return true;
			}

			responseTo = ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(queryId)
					+ " not found."
			);

			return false;
		}

		template<typename DB> static bool checkQuery(
				DB& db,
				std::uint64_t website,
				std::uint64_t queryId,
				ServerCommandResponse& responseTo
		) {
			if(db.isQuery(website, queryId)) {
				return true;
			}

			responseTo = ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(queryId)
					+ " for website #"
					+ std::to_string(website)
					+ " not found."
			);

			return false;
		}
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_SERVER_HPP_ */
