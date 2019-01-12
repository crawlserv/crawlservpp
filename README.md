<i><b>WARNING:</b> This application is under development. It is neither complete nor adequately documented yet. ~~strikethrough~~ means a feature is not implemented yet.</i>

# crawlserv++
<b>crawlserv++</b> is an application for crawling websites and analyzing textual content on these websites.

The architecture of <b>crawlserv++</b> consists of three distinct components:

* The <b>command-and-control server</b>, written in C++ (source code in [`crawlserv/src`](crawlserv/src)),
* a web server hosting the <b>frontend</b> written in HTML, PHP and JavaScript (source code in [`crawlserv_frontend/crawlserv`](crawlserv_frontend/crawlserv)),
* a mySQL <b>database</b> containing all data (i.e. thread status, configurations, logs, crawled content, parsed data as well as the results of all analyses).

## Command-and-Control Server

The command-and-control server contains an embedded web server (implemented by using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the frontend by [cross-origin resource sharing](https://en.wikipedia.org/wiki/Cross-origin_resource_sharing) of JSON code.

In the configuration file, access can (and should) be restricted to specific IPs only.

### Classes, Namespaces and Structures

**NB!** All non-external classes are part of the namespace `crawlservpp`.

The source code of the server consists of the following classes (as of January 2019):

* <b>[`Global::App`](crawlserv/src/Global/App.cpp)</b>: Main application class that processes command line arguments, writes console output, loads the configuration file, asks for the database password, creates and starts the server.
* <b>[`Global::ConfigFile`](crawlserv/src/Global/ConfigFile.cpp)</b>: A simple one line one entry configuration file where each line consists of a `key=value` pair.
* <b>[`Global::Database`](crawlserv/src/Global/Database.cpp)</b>: Database access for the server and its threads (parent class with server-specific and basic functionality only).
* <b>[`Global::Server`](crawlserv/src/Global/Server.cpp)</b>: Command-and-control server implementing a HTTP server for interaction with the frontend, managing threads and performing server commands.
* <b>[`Module::Config`](crawlserv/src/Module/Config.cpp)</b>: Abstract class as base for module-specific configuration classes.
* <b>[`Module::DBThread`](crawlserv/src/Module/DBThread.cpp)</b>: Database functionality for threads (child of the `Global::Database` class).
* <b>[`Module::DBWrapper`](crawlserv/src/Module/DBWrapper.cpp)</b>: Interface for the database access of threads (wraps the `Module::DBThread` class).
* <b>[`Module::Thread`](crawlserv/src/Module/Thread.cpp)</b>: Interface for a single thread implementing module-independent functionality (database connection, thread status, thread ticks, exception handling).
* <b>~~`Module::Analyzer::Config`~~</b>: Analyzing configuration. See ~~[analyzer.json](crawlserv_frontend/crawlserv/json/analyzer.json)~~ for all configuration entries.
* ~~<b>`Module::Analyzer::Database`</b>~~: Database access for analyzers (implements the `Module::DBWrapper` interface).
* ~~<b>`Module::Analyzer::Thread`</b>~~: Implementation of the `Module::Thread` interface for analyzers.
* <b>[`Module::Crawler::Config`](crawlserv/src/Module/Crawler/Config.cpp)</b>: Crawling configuration. See [crawler.json](crawlserv_frontend/crawlserv/json/crawler.json) for all configuration entries.
* <b>[`Module::Crawler::Database`](crawlserv/src/Module/Crawler/Database.cpp)</b>: Database access for crawlers (implements the `Module::DBWrapper` interface).
* <b>[`Module::Crawler::Thread`](crawlserv/src/Module/Crawler/Thread.cpp)</b>: Implementation of the `Module::Thread` interface for crawlers.
* <b>~~`Module::Extractor::Config`~~</b>: Extracting configuration. See ~~[extractor.json](crawlserv_frontend/crawlserv/json/extractor.json)~~ for all configuration entries.
* ~~<b>`Module::Extractor::Database`</b>~~: Database access for extractors (implements the `Module::DBWrapper` interface).
* ~~<b>`Module::Extractor::Thread`</b>~~: Implementation of the `Module::Thread` interface for extractors.
* <b>[`Module::Parser::Config`](crawlserv/src/Module/Parser/Config.cpp)</b>: Parsing configuration. See [parser.json](crawlserv_frontend/crawlserv/json/parser.json) for all configuration entries.
* <b>[`Module::Parser::Database`](crawlserv/src/Module/Parser/Database.cpp)</b>: Database access for parsers (implements the `Module::DBWrapper` interface).
* <b>[`Module::Parser::Thread`](crawlserv/src/Module/Parser/Thread.cpp)</b>: Implementation of the `Module::Thread` interface for parsers.
* <b>[`Network::Config`](crawlserv/src/Network/Config.cpp)</b>: Network configuration. This class is both used by the crawler and the extractor. See [crawler.json](crawlserv_frontend/crawlserv/json/parser.json) or ~~[extractor.json](crawlserv_frontend/crawlserv/json/extractor.json)~~ for all configuration entries.
* <b>[`Network::Curl`](crawlserv/src/Network/Curl.cpp)</b>: Provide networking functionality by using the [libcurl library](https://curl.haxx.se/libcurl/). This class is used by both the crawler and the extractor.
* <b>[`Parsing::URI`](crawlserv/src/Parsing/URI.cpp)</b>: URL parsing, domain checking and sub-URL extraction.
* <b>[`Parsing::XML`](crawlserv/src/Parsing/XML.cpp)</b>: Parse HTML documents into clean XML.
* <b>[`Query::Container`](crawlserv/src/Query/Container.cpp)</b>: Abstract class for query management in child classes.
* <b>[`Query::RegEx`](crawlserv/src/Query/RegEx.cpp)</b>: Using the [PCRE2 library](https://www.pcre.org/) to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
* <b>[`Query::XPath`](crawlserv/src/Query//XPath.cpp)</b>: Using the [pugixml parser library](https://github.com/zeux/pugixml) to implement a XPath query with boolean, single and/or multiple results.
* <b>[`Timer::SimpleHR`](crawlserv/src/Timer/SimpleHR.cpp)</b>: Simple high resolution timer for getting the time since creation in microseconds.
* <b>[`Timer::StartStop`](crawlserv/src/Timer/StartStop.cpp)</b>: Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
* <b>[`Timer::StartStopHR`](crawlserv/src/Timer/StartStopHR.cpp)</b>: High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
The following additional namespaces are used (to be found in [`crawlserv/src/Helper`](crawlserv/src/Helper)):
* <b>[`Helper::Algos`](crawlserv/src/Helper/Algos.h)</b>: Global helper functions for algorithms.
* <b>[`Helper::DateTime`](crawlserv/src/Helper/DateTime.cpp)</b>: Global helper functions for date/time and time to string conversion using [Howard E. Hinnant's date.h library](https://howardhinnant.github.io/date/date.html).
* <b>[`Helper::FileSystem`](crawlserv/src/Helper/FileSystem.cpp)</b>: Global helper functions for file system operations using [Boost.Filesystem](https://www.boost.org/doc/libs/1_68_0/libs/filesystem/doc/index.htm).
* <b>[`Helper::Json`](crawlserv/src/Helper/Json.cpp)</b>: Global helper functions for converting to JSON.
* <b>[`Helper::Portability`](crawlserv/src/Helper/Portability.cpp)</b>: Global helper functions for portability.
* <b>[`Helper::Strings`](crawlserv/src/Helper/Strings.cpp)</b>: Global helper functions for string processing and manipulation.
* <b>[`Helper::Utf8`](crawlserv/src/Helper/Utf8.cpp)</b>: Global helper functions for UTF-8 conversion using the [UTF8-CPP library](http://utfcpp.sourceforge.net/).
* <b>[`Helper::Versions`](crawlserv/src/Helper/Versions.cpp)</b>: Get the versions of the different libraries used by the server.

The following custom structures are used (to be found in [`crawlserv/src/Struct`](crawlserv/src/Struct)):

* <b>[`Struct::DatabaseSettings`](crawlserv/src/Struct/DatabaseSettings.h)</b>: Basic database settings (host, port, user, password, schema).
* <b>[`Struct::IdString`](crawlserv/src/Struct/IdString.h)</b>: A simple `[id,string]` pair.
* <b>[`Struct::Memento`](crawlserv/src/Struct/Memento.h)</b>: URL and timestamp of a memento (i.e. an archived website).
* <b>[`Struct::PreparedSqlStatement`](crawlserv/src/Struct/PreparedSqlStatement.h)</b>: Content of and pointer to a prepared SQL statement.
* <b>[`Struct::ServerCommandArgument`](crawlserv/src/Struct/ServerCommandArgument.h)</b>: The `[name,value]` pair of a server command argument.
* <b>[`Struct::ServerSettings`](crawlserv/src/Struct/ServerSettings.h)</b>: Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
* <b>[`Struct::ThreadDatabaseEntry`](crawlserv/src/Struct/ThreadDatabaseEntry.h)</b>: Thread status as saved in the database (ID, module, status message, pause status, options, ID of last processed URL).
* <b>[`Struct::ThreadOptions`](crawlserv/src/Struct/ThreadOptions.h)</b>: Basic thread options (IDs of website, URL list and configuration).

The [`main.cpp`](crawlserv/src/main.cpp) source file as entry point of the application only consists of one line of code that invokes the constructor (with the command line arguments as function arguments) and the `run()` function of the `Global::App` class. The latter also returns the return value for the `main` function (either `EXIT_SUCCESS` or `EXIT_FAILURE` as defined by the ISO C99 Standard, e.g. in `stdlib.h` of the GNU C Library).

### Configuration

The server needs a configuration file as argument, the test configuration can be found at [`crawlserv/config`](crawlserv/config). Replace the values in this file with those for your own database and server settings. The password for granting the server (full) access to the database will be prompted when starting the server.

The testing environment consists of one PC that runs all three components of the application that can only be accessed locally (by using ``localhost``). In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the web server running the frontend uses port 80 for interaction with the user (i.e. his\*her web browser). The mySQL database server uses (default) port 3306.

### Server Commands

The server performs commands and sends back their results. Some commands need to be confirmed before being actually performed and some commands can be restricted by the configuration file loaded when starting the server. The following commands are implemented (as of November 2018):

* <b>`addconfig`</b> (arguments: `website`, `module`, `name`, `config`): Add a configuration to the database.
* <b>`addquery`</b> (arguments: `website`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`): Add a RegEx or XPath query to the database.
* <b>`addurllist`</b> (arguments: `website`, `name`, `namespace`): Add a URL list to a website in the database.
* <b>`addwebsite`</b> (arguments: `name`, `namespace`, `domain`): Add a website to the database.
* <b>`allow`</b> (argument: `ip`): Allow access for the specified IP(s).
* <b>`clearlog`</b> (optional argument: `module`): Clear the logs of a specified module or all logs if no module is specified.
* <b>`deleteconfig`</b> (argument: `id`): Delete a configuration from the database.
* <b>`deletequery`</b> (argument: `id`): Delete a RegEx or XPath query from the database.
* <b>`deleteurllist`</b> (argument: `id`): Delete a URL list (and all associated data) from the database.
* <b>`deletewebsite`</b> (argument: `id`): Delete a website (and all associated data) from the database.
* <b>`disallow`</b>: Revoke access from all except the initial IP(s) specified by the configuration file.
* <b>`duplicateconfig`</b> (argument: `id`): Duplicate the specified configuration.
* <b>`duplicatequery`</b> (argument: `id`): Duplicate the specified RegEx or XPath query.
* <b>`duplicatewebsite`</b> (argument: `id`): Duplicate the specified website.
* <b>`kill`</b>: kill the server.
* <b>`log`</b> (argument: `entry`): Write a log entry by the frontend into the database.
* ~~<b>`pauseanalyzer`</b>~~ (argument: `id`): Pause a running analyzer by its ID.
* <b>`pausecrawler`</b> (argument: `id`): Pause a running crawler by its ID.
* ~~<b>`pauseextractor`</b>~~ (argument: `id`): Pause a running extractor by its ID.
* <b>`pauseparser`</b> (argument: `id`): Pause a running parser by its ID.
* ~~<b>`startanalyzer`</b>~~ (arguments: `website`, `urllist`, `config`): Start an analyzer using the specified website, URL list and configuration.
* <b>`startcrawler`</b> (arguments: `website`, `urllist`, `config`): Start a crawler using the specified website, URL list and configuration.
* ~~<b>`startextractor`</b>~~ (arguments: `website`, `urllist`, `config`): Start an extractor using the specified website, URL list and configuration.
* <b>`startparser`</b> (arguments: `website`, `urllist`, `config`): Start a parser using the specified website, URL list and configuration.
* ~~<b>`stopanalyzer`</b>~~ (argument: `id`): Stop a running analyzer by its ID.
* <b>`stopcrawler`</b> (argument: `id`): Stop a running crawler by its ID.
* ~~<b>`stopextractor`</b>~~ (argument: `id`): Stop a running extractor by its ID.
* <b>`stopparser`</b> (argument: `id`): Stop a running parser by its ID.
* <b>`testquery`</b> (arguments: `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`, `text`): Test a query on the specified text.
* ~~<b>`unpauseanalyzer`</b>~~ (argument: `id`): Unpause a paused analyzer by its ID.
* <b>`unpausecrawler`</b> (argument: `id`): Unpause a paused crawler by its ID.
* ~~<b>`unpauseextractor`</b>~~ (argument: `id`): Unpause a paused extractor by its ID.
* <b>`unpauseparser`</b> (argument: `id`): Unpause a paused parser by its ID.
* <b>`updateconfig`</b> (arguments: `id`, `name`, `config`): Update an existing configuration in the database.
* <b>`updatequery`</b> (arguments: `id`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`): Update an existing RegEx or XPath query in the database.
* <b>`updateurllist`</b> (arguments: `id`, `name`, `namespace`): Update an existing URL list in the database.
* <b>`updatewebsite`</b> (arguments: `id`, `name`, `namespace`, `domain`): Update an existing website in the database.

The commands and their replies are using the JSON format (implemented by using the [RapidJSON library](https://github.com/Tencent/rapidjson)). See the following examples:

<i><b>Command from frontend to server:</b> Delete the URL list with the ID #1</i>

```json
{
 "cmd":"deleteurllist",
 "id":1,
}
```

<i><b>Response by server:</b> Command needs to be confirmed</i>

```json
{
 "confirm":true,
 "text":"Do you really want to delete this URL list?\n!!! All associated data will be lost !!!"
}
````

<i><b>Response from frontend:</b> Confirm command</i>

```json
{
 "cmd":"deleteurllist",
 "id":1,
 "confirmed":true
}
````

<i><b>Response from server:</b> Success (otherwise `"failed":true` would be included in the response).</i>

```json
{
 "text":"URL list deleted."
}
```

For more information on the server commands, see the [source code](crawlserv/src/Global/Server.cpp) of the `Global::Server` class.

### Threads

As can be seen from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the `Global::Thread` template class:

* <b>crawler</b>: Crawling of websites (using custom URLs and following links to the same \[sub-\]domain, downloading plain content to the database and optionally checking archives using the [Memento API](http://www.mementoweb.org/guide/quick-intro/)).
* <b>parser</b>: Parsing data from URLs and downloaded content using user-defined RegEx and XPath queries.
* ~~<b>extractor</b>~~: Downloading additional data such as comments and social media content.
* ~~<b>analyzer</b>~~: Analyzing textual data using different methods and algorithms.

The server and each thread have their own connections to the database. These connections are handled by inheritance from the `Global::Database` class. Additionally, thread connections to the database (instances of `Module::DBThread` as child class of `Global::Database`) are wrapped through the `Module::DBWrapper` class to protect the threads (i.e. their programmers) from accidentally using the server connection to the database and thus compromising thread-safety. See the Classes, Namespaces and Structures section above as well as the corresponsing source code for details.

### Third-party Libraries

The following third-party libraries are used:

* [Boost C++ Libraries](https://www.boost.org/)
* [libcurl](https://curl.haxx.se/libcurl/)
* [Mongoose Embedded Web Server](https://github.com/cesanta/mongoose) (included in `crawlserv/src/external/mongoose.*`)
* [MySQL Connector/C++ 8.0](https://dev.mysql.com/doc/connector-cpp/8.0/en/)
* [Perl Compatible Regular Expressions 2](https://www.pcre.org/)
* [pugixml](https://github.com/zeux/pugixml)
* [RapidJSON](https://github.com/Tencent/rapidjson) (included in `crawlserv/src/external/rapidjson`)
* [HTML Tidy API](http://www.html-tidy.org/)
* [uriparser](https://github.com/uriparser/uriparser)
* [UTF8-CPP](http://utfcpp.sourceforge.net/) (included in `crawlserv/src/external/utf8*`)

The following libraries need to be manually linked after compiling the source code: `boost_system mysqlcppconn curl pthread stdc++fs pcre2-8 pugixml tidy uriparser`.

## Frontend

The frontend is a simple HTML/PHP and JavaScript application that has read-only access to the database on its own and can (under certain conditions) interact with the command-and-control server using the above listed commands when the user wants to perform actions that could change the content of the database. The frontend provides the following menu structure:

* <b>Server</b>: Authorize additional IPs or revoke authorization for all custom IPs, run custom commands and kill the server.
* <b>Websites</b>: Manage websites and their URL lists including the download of URL lists as text files. 
* <b>Queries</b>: Manage RegEx and XPath queries saved in the database including the test of such queries on custom texts by the command-and-control server using designated worker threads to avoid interference with the main functionality of the server.
* <b>Crawlers</b>: Manage crawling configurations in simple or advanced mode.
* <b>Parsers</b>: Manage parsing configurations in simple or advanced mode.
* ~~<b>Extractors</b>~~: Manage extracting configurations in simple or advanced mode.
* ~~<b>Analyzers</b>~~: Manage analyzing configurations in simple or advanced mode.
* <b>Threads</b>: Currently active threads and their status. Start, pause and stop specific threads.
* ~~<b>Search</b>:~~ Search crawled content.
* ~~<b>Content</b>:~~ View crawled content.
* ~~<b>Import/Export</b>:~~ Import and export URL lists and possibly other data.
* ~~<b>Statistics</b>:~~ Show specific statistics derived from the database.
* <b>Logs</b>: Show and delete log entries.

### Configuration

The frontend uses [`crawlserv_frontend/crawlserv/config.php`](crawlserv_frontend/crawlserv/config.php) to gain read-only access to the database. For security reasons, the database account used by the frontend should only have `SELECT` privilege! See this file for details about the test configuration (including the database schema and the user name and password for read-only access to the test database). Replace those values with those for your own database.

The testing environment consists of one PC that runs all three components of the application which can only be accessed locally (by using ``localhost``). Therefore, the (randomly created) password in `config.php` is irrelevant for usage outside the original test environment and needs to be replaced! In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the web server running the frontend uses port 80 for interaction with the user (i.e. his\*her web browser). The mySQL database server uses (default) port 3306.

## Database

The application uses exactly one database schema and all tables are prefixed with `crawlserv_`. The following main tables are used:

* <b>`analyzedtables`</b>: Index of result tables for analyzing.
* <b>`configs`</b>: Crawling, parsing, extracting and analyzing configurations.
* <b>`extractedtables`</b>: Index of result tables for extracting.
* <b>`log`</b>: Log entries.
* <b>`parsedtables`</b>: Index of result tables for parsing.
* <b>`queries`</b>: RegEx and XPath queries.
* <b>`threads`</b>: Thread status.
* <b>`urllists`</b>: URL lists.
* <b>`websites`</b>: Websites.

If not already existing, these tables will be created on startup of the command-and-control server by executing the SQL commands in [`crawlserv/sql/init.sql`](crawlserv/sql/init.sql). See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (a-z, A-Z, 0-9, $, \_) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* <b>`<namespace of website>_<namespace of URL list>`</b>: Content of the URL list.
* <b>~~`<namespace of website>_<namespace of URL list>_analyzed_<name of target table>`~~</b>: Analyzing results.
* <b>`<namespace of website>_<namespace of URL list>_crawled`</b>: Crawled content.
* <b>~~`<namespace of website>_<namespace of URL list>_extracted_<name of target table>`~~</b>: Extracting results.
* <b>`<namespace of website>_<namespace of URL list>_links`</b>: Linkage information (which URLs link to which other URLs).
* <b>`<namespace of website>_<namespace of URL list>_parsed_<name of target table>`</b>: Parsing results.

See the source code of the `Global::Database::addUrlList(...)` function in [`crawlserv/src/Global/Database.cpp`](crawlserv/src/Global/Database.cpp) for details about the structure of the non-result tables. Most of the columns of the result tables are specified by the respective parsing, extracting and analyzing configurations. See the code of the `Module::Parser::Database::initTargetTable(...)`, ~~`Module::Extractor::Database::initTargetTable(...)`~~ and ~~`Module::Analyzer::Database::initTargetTable(...)`~~ functions in [`crawlserv/src/Module/Parser/Database.cpp`](crawlserv/src/Module/Parser/Database.cpp), ~~[`crawlserv/src/Module/Extractor/Database.cpp`](crawlserv/src/Extractor/Database.cpp)~~ and ~~[`crawlserv/src/Module/Analyzer/Database.cpp`](crawlserv/src/Module/Analyzer/Database.cpp)~~ for details.

## Platform

At the moment, this software has been developed for and tested on <b>Linux only</b>.

Developed with Eclipse 2018-09 (4.9.0), Eclipse CDT 9.5.4, Eclipse PDT 6.1.0 and Eclipse Web Tools Platform 3.11.0. Compiled and linked with g++ 7.3.0. Tested with Apache/2.4.29 and mySQL 14.14 on Ubuntu 18.04.1 (64-bit).

[GitHub wide](https://github.com/mdo/github-wide) is recommended for browsing the source code online.
