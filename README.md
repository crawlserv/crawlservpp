# crawlserv++
<b>crawlserv++</b> is an application for crawling websites and analyzing textual content on these websites.

<i><b>WARNING:</b> This application is under development. It is neither complete nor adequately documented yet.</i>

The architecture of <b>crawlserv++</b> consists of three main parts:

* The <b>crawlserv++ command-and-control server</b>, written in C++ (source code in `crawlserv/src`)
* A webserver hosting the <b>crawlserv++ frontend</b> written in HTML, PHP and JavaScript (source code in `crawlserv_frontend/crawlserv`)
* A mySQL <b>database</b> containing all data (i.e. thread status, configurations, logs, crawled content, parsed data as well as the results of all analyses)

## crawlserv++ Command-and-Control Server

This server contains an embedded web server (implemented by using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the <b>crawlserv++ frontend</b> by cross-origin resource sharing (CORS). Access can (and should) be restricted to whitelisted IPs only.

### Server Commands

The server performs commands and sends back their results. Some commands need to be confirmed before actually performed and some commands can be restricted by the configuration file loaded by the server. The following commands are implemented (as of November 2018):

* <b>`addconfig`</b> (arguments: `website`, `module`, `name`, `config`): add a configuration to the database
* <b>`addquery`</b> (arguments: `website`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`): add a RegEx or XPath query to the database
* <b>`addurllist`</b> (arguments: `website`, `name`, `namespace`): add a URL list to a website in the database
* <b>`addwebsite`</b> (arguments: `name`, `namespace`, `domain`): add a website to the database
* <b>`allow`</b> (argument: `ip`): allow access for the specified IP(s)
* <b>`clearlog`</b> (optional argument: `module`): clear the logs of a specified module or all logs if no module is specified
* <b>`deleteconfig`</b> (argument: `id`): delete a configuration from the database
* <b>`deletequery`</b> (argument: `id`): delete a RegEx or XPath query from the database
* <b>`deleteurllist`</b> (argument: `id`): delete a URL list (and all associated data) from the database
* <b>`deletewebsite`</b> (argument: `id`): delete a website (and all associated data) from the database
* <b>`disallow`</b>: revoke access from all except the initial IP(s) specified by the configuration file
* <b>`duplicateconfig`</b> (argument: `id`): duplicate the specified configuration
* <b>`duplicatequery`</b> (argument: `id`): duplicate the specified RegEx or XPath query
* <b>`duplicatewebsite`</b> (argument: `id`): duplicate the specified website
* <b>`kill`</b>: kill the server
* <b>`log`</b> (argument: `entry`): write a log entry by the frontend into the database
* ~~<b>`pauseanalyzer`</b>~~ (argument: `id`): pause a running analyzer by its id
* <b>`pausecrawler`</b> (argument: `id`): pause a running crawler by its id
* ~~<b>`pauseextractor`</b>~~ (argument: `id`): pause a running extractor by its id
* ~~<b>`pauseparser`</b>~~ (argument: `id`): pause a running parser by its id
* ~~<b>`startanalyzer`</b>~~ (arguments: `website`, `urllist`, `config`): start an analyzer using the specified website, URL list and configuration
* <b>`startcrawler`</b> (arguments: `website`, `urllist`, `config`): start a crawler using the specified website, URL list and configuration
* ~~<b>`startextractor`</b>~~ (arguments: `website`, `urllist`, `config`): start an extractor using the specified website, URL list and configuration
* ~~<b>`startparser`</b>~~ (arguments: `website`, `urllist`, `config`): start a parser using the specified website, URL list and configuration
* ~~<b>`stopanalyzer`</b>~~ (argument: `id`): stop a running analyzer by its id
* <b>stopcrawler</b> (argument: `id`): stop a running crawler by its id
* ~~<b>`stopextractor`</b>~~ (argument: `id`): stop a running extractor by its id
* ~~<b>`stopparser`</b>~~ (argument: `id`): stop a running parser by its id

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

<i><b>Response from server:</b> Success (otherwise `"failed":true` would be sent).</i>

```json
{
 "text":"URL list deleted"
}
```

For more information on the server commands, check out the `Server` class of the server.

### Threads

As can be seen from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the `Thread` template class:

* <b>crawler</b>: Crawling of a website (using custom URLs and following links, downloading the plain content to the database and optionally checking archives using the [Memento Protocol](http://mementoweb.org/)).
* ~~<b>parser</b>~~: Parsing data from the downloaded content with the help of user-defined RegEx and XPath queries.
* ~~<b>extractor</b>~~: Downloading additional data such as comments and social media content.
* ~~<b>analyzer</b>~~: Analyzing textual data using different methods and algorithms.

The server and each thread have their own connection to the database. These connections are handled by inheritance from the `Database` class. Additionally, thread connections to the database (`DatabaseThread` as child class of `Database`) are wrapped through the `DatabaseModule` class to protect the threads from accidentally using the server connection to the database. See the corresponsing source code for details.

### Classes and Namespaces

The source code of the server consists of the following classes (as of November 2018):

* <b>`App`</b>: Main application class, processing command line arguments, showing initial console output, loading configuration file, creating and starting the server.
* <b>~~`ConfigAnalyzer`~~</b>: Analyzing configuration.
* <b>`ConfigCrawler`</b>: Crawling configuration.
* <b>~~`ConfigExtractor`~~</b>: Extracting configuration.
* <b>`ConfigFile`</b>: A simple one line one entry configuration file where each line consists of a `key=value` pair.
* <b>`ConfigParser`</b>: Parsing configuration.
* <b>`Database`</b>: Database access for the server and its threads (parent class with only basic functionality).
* <b>`DatabaseCrawler`</b>: Database access for crawlers (implements `DatabaseModule` interface).
* <b>`DatabaseModule`</b>: Interface for the database access of threads (wraps `DatabaseThread`).
* <b>`DatabaseThread`</b>: Database functionality for threads (child of `Database`).
* <b>`Networking`</b>: Provide networking functionality by using the libcurl library.
* <b>`RegEx`</b>: Using the [PCRE2 library](https://www.pcre.org/) to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
* <b>`Server`</b>: Command-and-control server implementing a HTTP server for interaction with the frontend, managing threads and performing server commands.
* <b>`Thread`</b>: Interface for a single thread implementing module-independent functionality (database connection, thread status, thread ticks, exception handling).
* ~~<b>`ThreadAnalyzer`</b>~~: Implementation of the `Thread` interface for analyzers.
* <b>`ThreadCrawler`</b>: Implementation of the `Thread` interface for crawlers.
* ~~<b>`ThreadExtractor`</b>~~: Implementation of the `Thread` interface for extractors.
* ~~<b>`ThreadParser`</b>~~: Implementation of the `Thread` interface for parsers.
* <b>`TimerSimpleHR`</b>: Simple high resolution timer for getting the time since creation in microseconds.
* <b>`TimerStartStop`</b>: Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
* <b>`TimerStartStopHR`</b>: High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
* <b>`URIParser`</b>: URL parsing, domain checking and sub-URL extraction.
* <b>`XMLDocument`</b>: Parse HTML documents into clean XML.
* <b>`XPath`</b>: Using the [pugixml parser library](https://github.com/zeux/pugixml) to implement a XPath query with boolean, single and/or multiple results.

The following additional namespaces are used:

* <b>`Helpers`</b>: Global helper functions for timing, file system operations, string manipulation, memento parsing, time conversion and portability.
* <b>`Versions`</b>: Get the versions of the different libraries used by the command-and-control server.

The `main.cpp` source file as entry point of the application contains only one line of code that invokes the constructor (with the command line arguments as function arguments) and the `run()` function of the `App` class. The latter also returns the return value for the `main` function (either `EXIT_SUCCESS` or `EXIT_FAILURE` as defined by the ISO C99 Standard, e.g. in `stdlib.h` of the GNU C Library).

### Third-party Libraries

The following third-party libraries are used:

* [Boost C++ Libraries](https://www.boost.org/)
* [libcurl](https://curl.haxx.se/libcurl/)
* [Mongoose Embedded Web Server](https://github.com/cesanta/mongoose) (included in `crawlserv/src/external/mongoose.*`)
* [Perl Compatible Regular Expressions 2](https://www.pcre.org/)
* [pugixml](https://github.com/zeux/pugixml)
* [RapidJSON](https://github.com/Tencent/rapidjson) (included in `crawlserv/src/external/rapidjson`)
* [HTML Tidy API](http://www.html-tidy.org/)
* [uriparser](https://github.com/uriparser/uriparser)
* [UTF8-CPP](http://utfcpp.sourceforge.net/) (included in `crawlserv/src/external/utf8*`)

## crawlserv++ Frontend

The frontend is a simple PHP and JavaScript application that has read-only access to the database and can (under certain conditions) interact with the command-and-control server when the user wants to perform actions that will change the content of the database. The frontend provides the following menu structure:

* <b>Server</b>: Authorize additional IPs or revoke authorization for all custom IPs, run custom commands and kill the server.
* <b>Websites</b>: Manage websites and their URL lists including the download of URL lists as text files. 
* <b>Queries</b>: Manage RegEx and XPath queries saved in the database including the test of such queries on custom texts by the command-and-control server using designated worker threads to avoid interference with the main functionality of the server.
* <b>Crawlers</b>: Manage crawling configurations in simple or advanced mode.
* ~~<b>Parsers</b>~~: Manage parsing configurations in simple or advanced mode.
* ~~<b>Extractors</b>~~: Manage extracting configurations in simple or advanced mode.
* ~~<b>Analyzers</b>~~: Manage analyzing configurations in simple or advanced mode.
* <b>Threads</b>: Currently active threads and their status. Start, pause and stop specific threads.
* ~~<b>Search</b>:~~ ...
* ~~<b>Content</b>:~~ ...
* ~~<b>Import/Export</b>:~~ ...
* ~~<b>Statistics</b>:~~ ...
* <b>Logs</b>: Show log entries and delete logs.

## Database

The application uses one database scheme and all tables will be prefixed with `crawlserv_`. The following main tables are used:

* <b>`log`</b>: Log entries.
* <b>`websites`</b>: Websites.
* <b>`urllists`</b>: URL lists.
* <b>`queries`</b>: RegEx and XPath queries.
* <b>`configs`</b>: Crawling, parsing, extracting and analyzing configurations.
* <b>`parsedtables`</b>: Result tables for parsing.
* <b>`extractedtables`</b>: Result tables for extracting.
* <b>`analyzedtables`</b>: Result tables for analyzing.
* <b>`threads`</b>: Thread status.

The tables will be created on startup of the command-and-control-server by executing the SQL commands in `crawlserv/sql/init.sql`. See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (a-z, A-Z, 0-9, $, \_) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* <b>`<namespace of website>_<namespace of URL list>`</b>: Content of the URL list.
* <b>`<namespace of website>_<namespace of URL list>_crawled`</b>: Crawled content.
* <b`<namespace of website>_<namespace of URL list>_links`</b>: Linkage information (which URLs link to which other URLs).

## Test Configuration

The testing environment consists of one PC that runs all components of the application which can only be accessed locally (by using ``localhost``). The command-and-control server needs a configuration file as argument, the test configuration can be found at `crawlserv/config`. The frontend uses `crawlserv_frontend/crawlserv/php/config.php` to provide access to the database. See those files for details about the test configuration.
