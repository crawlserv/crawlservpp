<i><b>WARNING:</b> This application is under development. It is neither complete nor adequately documented yet. ~~strikethrough~~ means a feature is not implemented yet.</i>

# crawlserv++
<b>crawlserv++</b> is an application for crawling websites and analyzing textual content on these websites.

The architecture of <b>crawlserv++</b> consists of three distinct components:

* The <b>command-and-control server</b>, written in C++ (source code in [`crawlserv/src`](crawlserv/src)),
* a webserver hosting the <b>frontend</b> written in HTML, PHP and JavaScript (source code in [`crawlserv_frontend/crawlserv`](crawlserv_frontend/crawlserv)),
* a mySQL <b>database</b> containing all data (i.e. thread status, configurations, logs, crawled content, parsed data as well as the results of all analyses).

## Command-and-Control Server

The command-and-control server contains an embedded web server (implemented by using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the frontend by [cross-origin resource sharing](https://en.wikipedia.org/wiki/Cross-origin_resource_sharing) of JSON code.

In the configuration file, access can (and should) be restricted to specific IPs only.

### Classes, Namespaces and Structures

The source code of the server consists of the following classes (as of November 2018):

* <b>[`App`](crawlserv/src/App.cpp)</b>: Main application class that processes command line arguments, writes console output, loads the configuration file, asks for the database password, creates and starts the server.
* <b>~~`ConfigAnalyzer`~~</b>: Analyzing configuration.
* <b>[`ConfigCrawler`](crawlserv/src/ConfigCrawler.cpp)</b>: Crawling configuration.
* <b>~~`ConfigExtractor`~~</b>: Extracting configuration.
* <b>[`ConfigFile`](crawlserv/src/ConfigFile.cpp)</b>: A simple one line one entry configuration file where each line consists of a `key=value` pair.
* <b>[`ConfigParser`](crawlserv/src/ConfigParser.cpp)</b>: Parsing configuration.
* <b>[`Database`](crawlserv/src/Database.cpp)</b>: Database access for the server and its threads (parent class with server-specific and basic functionality only).
* ~~<b>`DatabaseAnalyzer`</b>~~: Database access for analyzers (implements the `DatabaseModule` interface).
* <b>[`DatabaseCrawler`](crawlserv/src/DatabaseCrawler.cpp)</b>: Database access for crawlers (implements the `DatabaseModule` interface).
* ~~<b>`DatabaseExtractor`</b>~~: Database access for extractors (implements the `DatabaseModule` interface).
* <b>[`DatabaseModule`](crawlserv/src/DatabaseModule.cpp)</b>: Interface for the database access of threads (wraps the `DatabaseThread` class).
* ~~<b>`DatabaseParser`</b>~~: Database access for parsers (implements the `DatabaseModule` interface).
* <b>[`DatabaseThread`](crawlserv/src/DatabaseThread.cpp)</b>: Database functionality for threads (child of the `Database` class).
* <b>[`Networking`](crawlserv/src/Networking.cpp)</b>: Provide networking functionality by using the [libcurl library](https://curl.haxx.se/libcurl/).
* <b>[`RegEx`](crawlserv/src/RegEx.cpp)</b>: Using the [PCRE2 library](https://www.pcre.org/) to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
* <b>[`Server`](crawlserv/src/Server.cpp)</b>: Command-and-control server implementing a HTTP server for interaction with the frontend, managing threads and performing server commands.
* <b>[`Thread`](crawlserv/src/Thread.cpp)</b>: Interface for a single thread implementing module-independent functionality (database connection, thread status, thread ticks, exception handling).
* ~~<b>`ThreadAnalyzer`</b>~~: Implementation of the `Thread` interface for analyzers.
* <b>[`ThreadCrawler`](crawlserv/src/ThreadCrawler.cpp)</b>: Implementation of the `Thread` interface for crawlers.
* ~~<b>`ThreadExtractor`</b>~~: Implementation of the `Thread` interface for extractors.
* ~~<b>`ThreadParser`</b>~~: Implementation of the `Thread` interface for parsers.
* <b>[`TimerSimpleHR`](crawlserv/src/TimerSimpleHR.cpp)</b>: Simple high resolution timer for getting the time since creation in microseconds.
* <b>[`TimerStartStop`](crawlserv/src/TimerStartStop.cpp)</b>: Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
* <b>[`TimerStartStopHR`](crawlserv/src/TimerStartStopHR.cpp)</b>: High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
* <b>[`URIParser`](crawlserv/src/URIParser.cpp)</b>: URL parsing, domain checking and sub-URL extraction.
* <b>[`XMLDocument`](crawlserv/src/XMLDocument.cpp)</b>: Parse HTML documents into clean XML.
* <b>[`XPath`](crawlserv/src/XPath.cpp)</b>: Using the [pugixml parser library](https://github.com/zeux/pugixml) to implement a XPath query with boolean, single and/or multiple results.

The following additional namespaces are used (to be found in [`crawlserv/src/namespaces`](crawlserv/src/namespaces)):

* <b>[`Helpers`](crawlserv/src/namespaces/Helpers.cpp)</b>: Global helper functions for timing, file system operations, string manipulation, memento parsing, time conversion and portability.
* <b>[`Versions`](crawlserv/src/namespaces/Versions.cpp)</b>: Get the versions of the different libraries used by the server.

The following custom structures are used (to be found in [`crawlserv/src/structs`](crawlserv/src/structs)):

* <b>[`ConfigEntry`](crawlserv/src/structs/ConfigEntry.h)</b>: A `[name,value]` pair from the configuration file.
* <b>[`DatabaseSettings`](crawlserv/src/structs/DatabaseSettings.h)</b>: Basic database settings (host, port, user, password, schema).
* <b>[`IdString`](crawlserv/src/structs/IdString.h)</b>: A simple `[id,string]` pair.
* <b>[`Memento`](crawlserv/src/structs/Memento.h)</b>: URL and timestamp of a memento (archived website).
* <b>[`PreparedSqlStatement`](crawlserv/src/structs/PreparedSqlStatement.h)</b>: Content of and pointer to a prepared SQL statement.
* <b>[`ServerCommandArgument`](crawlserv/src/structs/ServerCommandArgument.h)</b>: The `[name,value]` pair of a server command argument.
* <b>[`ServerSettings`](crawlserv/src/structs/ServerSettings.h)</b>: Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
* <b>[`ThreadDatabaseEntry`](crawlserv/src/structs/ThreadDatabaseEntry.h)</b>: Thread status as saved in the database (id, module, status message, pause status, options, id of last processed URL).
* <b>[`ThreadOptions`](crawlserv/src/structs/ThreadOptions.h)</b>: Basic thread options (IDs of website, URL list and configuration).

The [`main.cpp`](crawlserv/src/main.cpp) source file as entry point of the application only consists of one line of code that invokes the constructor (with the command line arguments as function arguments) and the `run()` function of the `App` class. The latter also returns the return value for the `main` function (either `EXIT_SUCCESS` or `EXIT_FAILURE` as defined by the ISO C99 Standard, e.g. in `stdlib.h` of the GNU C Library).

### Configuration

The server needs a configuration file as argument, the test configuration can be found at [`crawlserv/config`](crawlserv/config). Replace the values in this file with those for your own database and server settings. The password for granting the server (full) access to the database will be prompted when starting the server.

The testing environment consists of one PC that runs all three components of the application that can only be accessed locally (by using ``localhost``). In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the webserver running the frontend uses port 80 for interaction with the user. The mySQL database server uses (default) port 3306.

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
* ~~<b>`pauseanalyzer`</b>~~ (argument: `id`): Pause a running analyzer by its id.
* <b>`pausecrawler`</b> (argument: `id`): Pause a running crawler by its id.
* ~~<b>`pauseextractor`</b>~~ (argument: `id`): Pause a running extractor by its id.
* ~~<b>`pauseparser`</b>~~ (argument: `id`): Pause a running parser by its id.
* ~~<b>`startanalyzer`</b>~~ (arguments: `website`, `urllist`, `config`): Start an analyzer using the specified website, URL list and configuration.
* <b>`startcrawler`</b> (arguments: `website`, `urllist`, `config`): Start a crawler using the specified website, URL list and configuration.
* ~~<b>`startextractor`</b>~~ (arguments: `website`, `urllist`, `config`): Start an extractor using the specified website, URL list and configuration.
* ~~<b>`startparser`</b>~~ (arguments: `website`, `urllist`, `config`): Start a parser using the specified website, URL list and configuration.
* ~~<b>`stopanalyzer`</b>~~ (argument: `id`): Stop a running analyzer by its id.
* <b>`stopcrawler`</b> (argument: `id`): Stop a running crawler by its id.
* ~~<b>`stopextractor`</b>~~ (argument: `id`): Stop a running extractor by its id.
* ~~<b>`stopparser`</b>~~ (argument: `id`): Stop a running parser by its id.

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

For more information on the server commands, see the [source code](crawlserv/src/Server.cpp) of the `Server` class.

### Threads

As can be seen from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the `Thread` template class:

* <b>crawler</b>: Crawling of websites (using custom URLs and following links to the same \[sub-\]domain, downloading plain content to the database and optionally checking archives using the [Memento API](http://timetravel.mementoweb.org/guide/api/)).
* ~~<b>parser</b>~~: Parsing data from URLs and downloaded content using user-defined RegEx and XPath queries.
* ~~<b>extractor</b>~~: Downloading additional data such as comments and social media content.
* ~~<b>analyzer</b>~~: Analyzing textual data using different methods and algorithms.

The server and each thread have their own connections to the database. These connections are handled by inheritance from the `Database` class. Additionally, thread connections to the database (instances of `DatabaseThread` as child class of `Database`) are wrapped through the `DatabaseModule` class to protect the threads from accidentally using the server connection to the database. See the next section on classes and namespaces as well as the corresponsing source code for details.

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

The testing environment consists of one PC that runs all three components of the application which can only be accessed locally (by using ``localhost``). Therefore, the (randomly created) password in `config.php` is irrelevant for usage outside the original test environment and needs to be replaced! In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the webserver running the frontend uses port 80 for interaction with the user. The mySQL database server uses (default) port 3306.

## Database

The application uses exactly one database schema and all tables are prefixed with `crawlserv_`. The following main tables are used:

* <b>`log`</b>: Log entries.
* <b>`websites`</b>: Websites.
* <b>`urllists`</b>: URL lists.
* <b>`queries`</b>: RegEx and XPath queries.
* <b>`configs`</b>: Crawling, parsing, extracting and analyzing configurations.
* <b>`parsedtables`</b>: Result tables for parsing.
* <b>`extractedtables`</b>: Result tables for extracting.
* <b>`analyzedtables`</b>: Result tables for analyzing.
* <b>`threads`</b>: Thread status.

If not already existing, these tables will be created on startup of the command-and-control server by executing the SQL commands in [`crawlserv/sql/init.sql`](crawlserv/sql/init.sql). See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (a-z, A-Z, 0-9, $, \_) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* <b>`<namespace of website>_<namespace of URL list>`</b>: Content of the URL list.
* <b>`<namespace of website>_<namespace of URL list>_crawled`</b>: Crawled content.
* <b>`<namespace of website>_<namespace of URL list>_links`</b>: Linkage information (which URLs link to which other URLs).

See the source code of the `Database::addUrlList(...)` function in [`crawlserv/src/Database.cpp`](crawlserv/src/Database.cpp) for details about the structure of these tables.

## Platform

At the moment, this software has been developed for and tested on <b>Linux only</b>.

Developed with Eclipse 2018-09 (4.9.0), Eclipse CDT 9.5.4, Eclipse PDT 6.1.0 and Eclipse Web Tools Platform 3.11.0. Compiled and linked with g++ 7.3.0. Tested with Apache/2.4.29 and mySQL 14.14 on Ubuntu 18.04.1 (64-bit).

[GitHub wide](https://github.com/mdo/github-wide) is recommended for browsing the source code online.
