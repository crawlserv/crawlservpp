_**WARNING!** This application is under development. It is neither complete nor adequately documented or tested._

 _~~Strikethrough~~ means a feature is not implemented yet._

# crawlserv++
**crawlserv++** is an application for crawling websites and analyzing textual content on these websites.

The architecture of **crawlserv++** consists of three distinct components:

* The **command-and-control server**, written in C++ (source code in [`crawlserv/src`](crawlserv/src)),
* a web server hosting the (quick 'n' dirty) **frontend** written in HTML, PHP and JavaScript (source code in [`crawlserv_frontend/crawlserv`](crawlserv_frontend/crawlserv)),
* a mySQL **database** containing all data (i.e. thread status, configurations, logs, crawled content, parsed and extracted data as well as the results of all analyses).

## Legal Notice

Before using crawlserv++ for crawling websites and other data, please make sure you are [legally allowed to do so](https://benbernardblog.com/web-scraping-and-crawling-are-perfectly-legal-right/).

You may _not_ use this software

* in any way that breaches any applicable local, national or international law or regulation;
* in any way that is unlawful or fraudulent, or has any unlawful or fraudulent purpose or effect;
* to make unauthorised attempts to access any third party networks.

## Building crawlserv++ on Linux

[![](https://travis-ci.org/crawlserv/crawlservpp.svg?branch=master)](https://travis-ci.org/crawlserv/crawlservpp) &mdash; *See [`.travis.yml`](.travis.yml) for example build environments.*

You can clone the complete source code into a sub-folder named `crawlserv` using `git`(https://git-scm.com/):

```
git clone https://github.com/crawlserv/crawlservpp crawlserv
git submodules init
git submodules update
```

The following additional components are required to build crawlserv++ on your system:

* [`cmake`](https://cmake.org/), version 3.8.2 or higher
* [`GNU Make`](https://www.gnu.org/software/make/) or a compatible Makefile parser
* [`gcc`](https://gcc.gnu.org/), version 6 or higher, or [`clang`](https://clang.llvm.org/), version 5 or higher – or any other modern C++ 17 compiler
* a multi-threading library supported by `cmake` like `pthreads` (e.g. `libpthread-stubs0-dev` on Ubuntu)
* a modern C++ Standard Library supporting C++ 17 (e.g. `libstdc++-7-dev`)
* the [`Boost.System`](https://www.boost.org/doc/libs/1_69_0/libs/system/doc/html/system.html) library (`libboost-system-dev`)
* the [`curl`](https://curl.haxx.se/libcurl/) library (e.g. `libcurl4-openssl-dev`)
* the [`tidy-html5`](http://www.html-tidy.org/) library, version 5 or higher (`libtidy-dev`*)
* the [`uriparser`](https://uriparser.github.io/) library (`liburiparser-dev`)
* the [`pugixml`](https://pugixml.org/) library (`libpugixml-dev`)
* the [`PCRE`](https://www.pcre.org/) library, version 2 (`libpcre2-dev`)
* the [MySQL Connector/C++](https://dev.mysql.com/doc/dev/connector-cpp/8.0/) library (`libmysqlcppconn-dev`)
* currently also the [GNU Aspell library](http://aspell.net/) (`libaspell-dev`)

*&ast; Older Linux distributions may only have `libtidy-dev` v0.9 available. Install it [manually](https://github.com/htacg/tidy-html5/releases) or find a pre-compiled version like the ones from Ondřej Surý's [PHP repository for Ubuntu](https://launchpad.net/~ondrej/+archive/ubuntu/php).*

After installing these components and cloning or downloading the source code, use the terminal to go to the `crawlserv` directory inside the downloaded files (it has to be where [`CMakeLists.txt`](crawlserv/CMakeLists.txt) is located) and run the following commands:

```
mkdir build
cd build
cmake ..
```

If `cmake` was successful and shows `Build files have been written to: [...]`, proceed with:

```
make
```

You can safely ignore warnings from external libraries as long as `make` finishes with `[100%] Built target crawlserv`.

The program should have been built inside the newly created `build` directory.

Leave this directory with `cd ..` before running it.

Note that you need to setup a MySQL server, a frontend (e.g. the one in `crawlserv_frontend` on a web server with PHP support) and personalize your configuration before finally starting the server with `./build/crawlserv config` or any other configuration file as argument. If you want to change the location of the program, make sure to take the [`sql`](crawlserv/sql) folder with you as it provides basic commands to initialize the database (creating all the global tables on first connection).

The program will ask you for the password of the chosen MySQL user before it proceeds. When `Server is up and running.` is displayed, switch to the frontend to take control of the command-and-control server.

Even without access to the frontend you can shut down the server from the terminal by sending a SIGINT signal (`CTRL+X`). It will wait for all running threads to avoid any loss of data.

## Command-and-Control Server

The command-and-control server contains an embedded web server (implemented using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the frontend by [cross-origin resource sharing](https://en.wikipedia.org/wiki/Cross-origin_resource_sharing) of JSON code.

In the configuration file, access can (and should) be restricted to specific IPs only.

### Classes, Namespaces and Structures

**NB!** All non-external classes are part of the namespace `crawlservpp`.

The source code of the server consists of the following classes (as of March 2019):

* **[`Main::App`](crawlserv/src/Main/App.cpp)**: Main application class that processes command line arguments, writes console output, loads the configuration file, asks for the database password, creates and starts the server.
* **[`Main::ConfigFile`](crawlserv/src/Main/ConfigFile.cpp)**: A simple one line one entry configuration file where each line consists of a `key=value` pair.
* **[`Main::Database`](crawlserv/src/Main/Database.cpp)**: Database access for the server and its threads (parent class with server-specific and basic functionality only).
* **[`Main::Exception`](crawlserv/src/Main/Exception.hpp)**: Class for custom exceptions.
* **[`Main::Server`](crawlserv/src/Main/Server.cpp)**: Command-and-control server managing threads and performing server commands.
* **[`Main::WebServer`](crawlserv/src/Main/WebServer.cpp)**: Implementation of an embedded HTTP server for interaction with the frontend using the [mongoose library](https://github.com/cesanta/mongoose).
* **[`Module::Config`](crawlserv/src/Module/Config.hpp)**: Abstract class as base for module-specific configuration classes.
* **[`Module::Database`](crawlserv/src/Module/Database.cpp)**: Database functionality for threads (child of the [`Main::Database`](crawlserv/src/Main/Database.cpp) class).
* **[`Module::Thread`](crawlserv/src/Module/Thread.cpp)**: Abstract class for a single thread implementing module-independent functionality (database connection, thread status, thread ticks, exception handling).
* **[`Module::Analyzer::Algo::MarkovText`](crawlserv/src/Module/Analyzer/Algo/MarkovText.cpp)**: Markov chain text generator [borrowed from Rosetta Code](https://rosettacode.org/wiki/Markov_chain_text_generator).
* **[`Module::Analyzer::Algo::MarkovTweet`](crawlserv/src/Module/Analyzer/Algo/MarkovTweet.cpp)**: Markov chain tweet generator [borrowed from Kelly Rauchenberger](https://github.com/hatkirby/rawr-ebooks).
* **[`Module::Analyzer::Config`](crawlserv/src/Module/Analyzer/Config.hpp)**: Analyzing configuration. See [analyzer.json](crawlserv_frontend/crawlserv/json/analyzer.json) for all configuration entries.
* **[`Module::Analyzer::Database`](crawlserv/src/Module/Analyzer/Database.cpp)**: Database access for analyzers (implements the [`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp) class).
* **[`Module::Analyzer::Thread`](crawlserv/src/Module/Analyzer/Thread.cpp)**: Implementation of the [`Module::Thread`](crawlserv/src/Module/Thread.cpp) class for analyzers. Abstract class to be inherited by algorithm classes.
* **[`Module::Crawler::Config`](crawlserv/src/Module/Crawler/Config.hpp)**: Crawling configuration. See [crawler.json](crawlserv_frontend/crawlserv/json/crawler.json) for all configuration entries.
* **[`Module::Crawler::Database`](crawlserv/src/Module/Crawler/Database.cpp)**: Database access for crawlers (implements the [`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp) class).
* **[`Module::Crawler::Thread`](crawlserv/src/Module/Crawler/Thread.cpp)**: Implementation of the [`Module::Thread`](crawlserv/src/Module/Thread.cpp) class for crawlers.
* **~~[`Module::Extractor::Config`](crawlserv/src/Module/Extractor/Config.hpp)~~**: Extracting configuration. See ~~[extractor.json](crawlserv_frontend/crawlserv/json/extractor.json)~~ for all configuration entries.
* **~~[`Module::Extractor::Database`](crawlserv/src/Module/Extractor/Database.cpp)~~**: Database access for extractors (implements the [`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp) class).
* **~~[`Module::Extractor::Thread`](crawlserv/src/Module/Extractor/Thread.cpp)~~**: Implementation of the [`Module::Thread`](crawlserv/src/Module/Thread.cpp) class for extractors.
* **[`Module::Parser::Config`](crawlserv/src/Module/Parser/Config.hpp)**: Parsing configuration. See [parser.json](crawlserv_frontend/crawlserv/json/parser.json) for all configuration entries.
* **[`Module::Parser::Database`](crawlserv/src/Module/Parser/Database.cpp)**: Database access for parsers (implements the [`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp) class).
* **[`Module::Parser::Thread`](crawlserv/src/Module/Parser/Thread.cpp)**: Implementation of the [`Module::Thread`](crawlserv/src/Module/Thread.cpp) class for parsers.
* **[`Network::Config`](crawlserv/src/Network/Config.hpp)**: Network configuration. This class is both used by the crawler and the extractor. See [crawler.json](crawlserv_frontend/crawlserv/json/parser.json) or ~~[extractor.json](crawlserv_frontend/crawlserv/json/extractor.json)~~ for all configuration entries.
* **[`Network::Curl`](crawlserv/src/Network/Curl.cpp)**: Provide networking functionality using the [libcurl library](https://curl.haxx.se/libcurl/). This class is used by both the crawler and the extractor.
* **[`Parsing::HTML`](crawlserv/src/Parsing/HTML.cpp)**: Clean HTML documents and parse them into XML using the [HTML Tidy API](http://www.html-tidy.org/).
* **[`Parsing::URI`](crawlserv/src/Parsing/URI.cpp)**: URL parsing, domain checking and sub-URL extraction using [uriparser](https://github.com/uriparser/uriparser).
* **[`Parsing::XML`](crawlserv/src/Parsing/XML.cpp)**: Parse XML documents using the [pugixml parser library](https://github.com/zeux/pugixml).
* **[`Query::Container`](crawlserv/src/Query/Container.cpp)**: Abstract class for query management in child classes.
* **[`Query::JSONPointer`](crawlserv/src/Query/JSONPointer.cpp)**: Using the [RapidJSON library](https://github.com/Tencent/rapidjson) to implement a JSONPointer with boolean and/or single result.
* **[`Query::RegEx`](crawlserv/src/Query/RegEx.cpp)**: Using the [PCRE2 library](https://www.pcre.org/) to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
* **[`Query::XPath`](crawlserv/src/Query/XPath.cpp)**: Using the [pugixml parser library](https://github.com/zeux/pugixml) to implement a XPath query with boolean, single and/or multiple results.
* **[`Timer::Simple`](crawlserv/src/Timer/Simple.cpp)**: Simple timer for getting the time since creation and later ticks in milliseconds.
* **[`Timer::SimpleHR`](crawlserv/src/Timer/SimpleHR.cpp)**: Simple high resolution timer for getting the time since creation and later ticks in microseconds.
* **[`Timer::StartStop`](crawlserv/src/Timer/StartStop.cpp)**: Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
* **[`Timer::StartStopHR`](crawlserv/src/Timer/StartStopHR.cpp)**: High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
* **[`Wrapper::Curl`](crawlserv/src/Wrapper/Curl.hpp)**: [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) wrapper for pointer to cURL instance, also handles global instance if necessary.
* **[`Wrapper::CurlList`](crawlserv/src/Wrapper/CurlList.hpp)**: RAII wrapper for pointer to URI query list.
* **[`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp)**: Abstract class for the database access of threads (wraps the [`Module::Database`](crawlserv/src/Module/Database.cpp) class).
* **[`Wrapper::DatabaseLock`](crawlserv/src/Wrapper/DatabaseLock.hpp)**: Template class for safe in-scope database locks.
* **[`Wrapper::PCRE`](crawlserv/src/Wrapper/PCRE.hpp)**: Wrapper for pointer to Perl-Compatible Regular Expression.
* **[`Wrapper::PCREMatch`](crawlserv/src/Wrapper/PCREMatch.hpp)**: RAII wrapper for pointer to Perl-Compatible Regular Expression match.
* **[`Wrapper::PreparedSqlStatement`](crawlserv/src/Wrapper/PreparedSqlStatement.hpp)**: RAII wrapper for prepared SQL statement pointer.
* **[`Wrapper::URI`](crawlserv/src/Wrapper/URI.hpp)**: Wrapper for pointer to URI structure.
* **[`Wrapper::URIQueryList`](crawlserv/src/Wrapper/URIQueryList.hpp)**: RAII wrapper for pointer to URI query list.

The following additional namespaces are used (to be found in [`crawlserv/src/Helper`](crawlserv/src/Helper)):

* **[`Helper::DateTime`](crawlserv/src/Helper/DateTime.hpp)**: Global helper functions for date/time and time to string conversion using [Howard E. Hinnant's date.h library](https://howardhinnant.github.io/date/date.html).
* **[`Helper::FileSystem`](crawlserv/src/Helper/FileSystem.hpp)**: Global helper functions for file system operations using `std::experimental::filesystem`.
* **[`Helper::Json`](crawlserv/src/Helper/Json.hpp)**: Global helper functions for converting to JSON using the [RapidJSON library](https://github.com/Tencent/rapidjson).
* **[`Helper::Portability`](crawlserv/src/Helper/Portability/)**: Additional headers for portability.
* **[`Helper::Strings`](crawlserv/src/Helper/Strings.hpp)**: Global helper functions for string processing and manipulation.
* **[`Helper::System`](crawlserv/src/Helper/System.hpp)**: Global system helper function.
* **[`Helper::Utf8`](crawlserv/src/Helper/Utf8.hpp)**: Global helper functions for UTF-8 conversion using the [UTF8-CPP library](http://utfcpp.sourceforge.net/).
* **[`Helper::Versions`](crawlserv/src/Helper/Versions.hpp)**: Get the versions of the different libraries used by the server.

The following custom structures are globally used (to be found in [`crawlserv/src/Struct`](crawlserv/src/Struct)):

* **[`Struct::AlgoThreadProperties`](crawlserv/src/Struct/AlgoThreadProperties.hpp)**: Properties of an algorithm thread.
* **[`Struct::ConfigItem`](crawlserv/src/Struct/ConfigItem.hpp)**: Configuration item (category, name, value).
* **[`Struct::ConfigProperties`](crawlserv/src/Struct/ConfigProperties.hpp)**: Basic configuration properties (module, name, configuration).
* **[`Struct::CorpusProperties`](crawlserv/src/Struct/CorpusProperties.hpp)**: Basic corpus properties (source type, table and field).
* **[`Struct::DatabaseSettings`](crawlserv/src/Struct/DatabaseSettings.hpp)**: Basic database settings (host, port, user, password, schema, compression).
* **[`Struct::ParsingEntry`](crawlserv/src/Struct/ParsingEntry.hpp)**: Parsed data (content ID, parsed ID, parsed date/time, parsed fields).
* **[`Struct::QueryProperties`](crawlserv/src/Struct/QueryProperties.hpp)**: Basic query properties (name, text, type, result type).
* **[`Struct::ServerSettings`](crawlserv/src/Struct/ServerSettings.hpp)**: Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
* **[`Struct::TableColumn`](crawlserv/src/Struct/TableColumn.hpp)**: Structure for table columns (name, type, reference, indexed).
* **[`Struct::TableProperties`](crawlserv/src/Struct/TableProperties.hpp)**: Table properties (name, columns, data directory, compression).
* **[`Struct::TargetTableProperties`](crawlserv/src/Struct/TargetTableProperties.hpp)**: Target table properties (type, website, URL list, name and full name, columns, data directory, compression).
* **[`Struct::ThreadDatabaseEntry`](crawlserv/src/Struct/ThreadDatabaseEntry.hpp)**: Thread status as saved in the database (ID, module, status message, pause status, options, ID of last processed URL).
* **[`Struct::ThreadOptions`](crawlserv/src/Struct/ThreadOptions.hpp)**: Basic thread options (IDs of website, URL list and configuration).
* **[`Struct::UrlListProperties`](crawlserv/src/Struct/UrlListProperties.hpp)**: Basic URL list properties (namespace, name).
* **[`Struct::WebsiteProperties`](crawlserv/src/Struct/WebsiteProperties.hpp)**: Basic website properties (domain, namespace, name).

Algorithms are registered in [`crawlserv/src/Module/Analyzer/Algo/All.cpp`](crawlserv/src/Module/Analyzer/Algo/All.cpp).

Additional structures for writing and getting custom data to and from the database are defined in [`crawlserv/src/Main/Data.hpp`](crawlserv/src/Main/Data.hpp).

The [`main.cpp`](crawlserv/src/main.cpp) source file as entry point of the application only consists of one line of code that invokes the constructor (with the command line arguments as function arguments) and the `run()` function of the [`Main::App`](crawlserv/src/Main/App.cpp) class. The latter also returns the return value for the `main` function (either `EXIT_SUCCESS` or `EXIT_FAILURE` as defined by the ISO C99 Standard, e.g. in `stdlib.h` of the GNU C Library).

### Server Commands

The server performs commands and sends back their results. Some commands need to be confirmed before being actually performed and some commands can be restricted by the configuration file loaded when starting the server. The following commands are implemented (as of February 2019):

* **`addconfig`** (arguments: `website`, `module`, `name`, `config`): Add a configuration to the database.
* **`addquery`** (arguments: `website`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`): Add a RegEx, XPath or JSONPointer query to the database.
* **`addurllist`** (arguments: `website`, `name`, `namespace`): Add a URL list to a website in the database.
* **`addwebsite`** (arguments: `name`, `namespace`, `domain`): Add a website to the database.
* **`allow`** (argument: `ip`): Allow access for the specified IP(s).
* **`clearlog`** (optional argument: `module`): Clear the logs of a specified module or all logs if no module is specified.
* **`deleteconfig`** (argument: `id`): Delete a configuration from the database.
* **`deletequery`** (argument: `id`): Delete a RegEx, XPath or JSONPpinter query from the database.
* **`deleteurllist`** (argument: `id`): Delete a URL list (and all associated data) from the database.
* **`deletewebsite`** (argument: `id`): Delete a website (and all associated data) from the database.
* **`disallow`**: Revoke access from all except the initial IP(s) specified by the configuration file.
* **`duplicateconfig`** (argument: `id`): Duplicate the specified configuration.
* **`duplicatequery`** (argument: `id`): Duplicate the specified RegEx, XPath or JSONPointer query.
* **`duplicatewebsite`** (argument: `id`): Duplicate the specified website.
* **`kill`**: kill the server.
* **`log`** (argument: `entry`): Write a log entry by the frontend into the database.
* **`pauseanalyzer`** (argument: `id`): Pause a running analyzer by its ID.
* **`pausecrawler`** (argument: `id`): Pause a running crawler by its ID.
* **~~`pauseextractor`~~** (argument: `id`): Pause a running extractor by its ID.
* **`pauseparser`** (argument: `id`): Pause a running parser by its ID.
* **`resetanalyzingstatus`** (argument: `urllist`): Reset the analyzing status of an ID-specified URL list.
* **`resetextractingstatus`** (argument: `urllist`): Reset the extracting status of an ID-specified URL list.
* **`resetparsingstatus`** (argument: `urllist`): Reset the parsing status of an ID-specified URL list.
* **`startanalyzer`** (arguments: `website`, `urllist`, `config`): Start an analyzer using the specified website, URL list and configuration.
* **`startcrawler`** (arguments: `website`, `urllist`, `config`): Start a crawler using the specified website, URL list and configuration.
* **~~`startextractor`~~** (arguments: `website`, `urllist`, `config`): Start an extractor using the specified website, URL list and configuration.
* **`startparser`** (arguments: `website`, `urllist`, `config`): Start a parser using the specified website, URL list and configuration.
* **`stopanalyzer`** (argument: `id`): Stop a running analyzer by its ID.
* **`stopcrawler`** (argument: `id`): Stop a running crawler by its ID.
* **~~`stopextractor`~~** (argument: `id`): Stop a running extractor by its ID.
* **`stopparser`** (argument: `id`): Stop a running parser by its ID.
* **`testquery`** (arguments: `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`, `text`): Test a query on the specified text.
* **`unpauseanalyzer`** (argument: `id`): Unpause a paused analyzer by its ID.
* **`unpausecrawler`** (argument: `id`): Unpause a paused crawler by its ID.
* **~~`unpauseextractor`~~** (argument: `id`): Unpause a paused extractor by its ID.
* **`unpauseparser`** (argument: `id`): Unpause a paused parser by its ID.
* **`updateconfig`** (arguments: `id`, `name`, `config`): Update an existing configuration in the database.
* **`updatequery`** (arguments: `id`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `textonly`): Update an existing RegEx, XPath or JSONPointer query in the database.
* **`updateurllist`** (arguments: `id`, `name`, `namespace`): Update an existing URL list in the database.
* **`updatewebsite`** (arguments: `id`, `name`, `namespace`, `domain`): Update an existing website in the database.
* **`warp`** (arguments: `thread`, `target`): Let a thread jump to the specified ID.

The commands and their replies are using the JSON format (implemented using the [RapidJSON library](https://github.com/Tencent/rapidjson)).

See the following examples.

**Command from frontend to server:** Delete the URL list with the ID #1.

```json
{
 "cmd":"deleteurllist",
 "id":1,
}
```

**Response by server:** Command needs to be confirmed.

```json
{
 "confirm":true,
 "text":"Do you really want to delete this URL list?\n!!! All associated data will be lost !!!"
}
````

**Response from frontend:** Confirm command.

```json
{
 "cmd":"deleteurllist",
 "id":1,
 "confirmed":true
}
````

**Response from server:** Success (otherwise `"failed":true` would be included in the response).

```json
{
 "text":"URL list deleted."
}
```

For more information on the server commands, see the [source code](crawlserv/src/Main/Server.cpp) of the `Main::Server` class.

### Threads

As can be seen from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the [`Module::Thread`](crawlserv/src/Module/Thread.cpp) template class:

* **crawler**: Crawling of websites (using custom URLs and following links to the same \[sub-\]domain, downloading plain content to the database and optionally checking archives using the [Memento API](http://www.mementoweb.org/guide/quick-intro/)).
* **parser**: Parsing data from URLs and downloaded content using user-defined RegEx, XPath and JSONPointer queries.
* **~~extractor~~**: Downloading additional data such as comments and social media content.
* **analyzer**: Analyzing textual data using different methods and algorithms.

Configurations for these modules are saved as JSON arrays in the shared `configs` table.

Analyzers are implemented by their own set of subclasses &mdash; algorithm classes. The following algorithms are implemented at the moment (as of February 2019):
 
* **MarkovText**: Markov Chain Text Generator.
* **MarkovTweet**: Markov Chain Tweet Generator.

The server and each thread have their own connections to the database. These connections are handled by inheritance from the [`Main::Database`](crawlserv/src/Main/Database.cpp) class. Additionally, thread connections to the database (instances of [`Module::Database`](crawlserv/src/Module/Database.cpp) as child class of `Main::Database`) are wrapped through the [`Wrapper::Database`](crawlserv/src/Wrapper/Database.hpp) class to protect the threads (i.e. their programmers) from accidentally using the server connection to the database and thus compromising thread-safety. See the Classes, Namespaces and Structures section above as well as the corresponsing source code for details.

The parser and analyzer threads may pre-cache (and therefore temporarily multiply) data in memory, while the crawler and ~~extractor~~ threads work directly on the database, which minimizes memory usage. Because the usual bottleneck for parsers and extractors are requests to the crawled/extracted website, **multiple threads are encouraged for crawling and extracting.** **Multiple threads for parsing and analyzing can be reasonable when using multiple CPU cores**, although some additional memory usage by the in-memory multiplication of data should be expected as well as some blocking because of simultaneous database access. At the same time, a slow database connection or server can have significant impact on performance in any case.

### Third-party Libraries

The following third-party libraries are used by the command-and-control server:

* [Boost C++ Libraries](https://www.boost.org/)
* [GNU Aspell](http://aspell.net/)
* [Howard E. Hinnant's date.h library](https://howardhinnant.github.io/date/date.html) (included in `crawlserv/src/_extern/date.h`)
* [libcurl](https://curl.haxx.se/libcurl/)
* [Mongoose Embedded Web Server](https://github.com/cesanta/mongoose) (included in `crawlserv/src/_extern/mongoose`)
* [MySQL Connector/C++ 8.0](https://dev.mysql.com/doc/connector-cpp/8.0/en/)
* [Perl Compatible Regular Expressions 2](https://www.pcre.org/)
* [pugixml](https://github.com/zeux/pugixml)
* [RapidJSON](https://github.com/Tencent/rapidjson) (included in `crawlserv/src/_extern/rapidjson`)
* [rawr-gen](https://github.com/hatkirby/rawr-ebooks) (included in `crawlserv/src/_extern/rawr`)
* [HTML Tidy API](http://www.html-tidy.org/)
* [uriparser](https://github.com/uriparser/uriparser)
* [UTF8-CPP](http://utfcpp.sourceforge.net/) (included in `crawlserv/src/_extern/utf8`)

While `date.h`, Mongoose, RapidJSON, `rawr-gen` and `UTF8-CPP` are included in the source code and compiled together with the server, all other libraries need to be externally present.

## Frontend

**NB!** The current frontend is a quick-and-dirty solution to test the full functionality of the server. Feel free to implement your own nice frontend solution in your favorite programming language – all you need is a read-only connection to the mySQL database and a HTTP connection for exchanging JSON with the command-and-control server. You may also want to use the provided JSON files in [`crawlserv_frontend/crawlserv/json`](crawlserv_frontend/crawlserv/json) as keeping them up-to-date will inform you about module-specific configuration changes and the implementation of new algorithms.

This frontend is a simple HTML/PHP and JavaScript application that has read-only access to the database on its own and can (under certain conditions) interact with the command-and-control server using the above listed commands when the user wants to perform actions that could change the content of the database.

It provides the following menu structure:

* **Server**: Authorize additional IPs or revoke authorization for all custom IPs, run custom commands and kill the server.
* **Websites**: Manage websites and their URL lists including the download of URL lists as text files. 
* **Queries**: Manage RegEx, XPath and JSONPointer queries saved in the database including the test of such queries on custom texts by the command-and-control server using designated worker threads to avoid interference with the main functionality of the server.
* **Crawlers**: Manage crawling configurations in simple or advanced mode.
* **Parsers**: Manage parsing configurations in simple or advanced mode.
* **~~Extractors~~**: Manage extracting configurations in simple or advanced mode.
* **Analyzers**: Manage analyzing configurations in simple or advanced mode.
* **Threads**: Currently active threads and their status. Start, pause and stop specific threads.
* **~~Search:~~** Search crawled content.
* **Content:** View crawled, parsed, ~~extracted~~ and ~~analyzed~~ content.
* **~~Import/Export:~~** Import and export URL lists and possibly other data.
* **~~Statistics:~~** Show specific statistics derived from the database.
* **Logs**: Show and delete log entries.

### Configuration

The server needs a configuration file as argument, the test configuration can be found at [`crawlserv/config`](crawlserv/config). Replace the values in this file with those for your own database and server settings. The password for granting the server (full) access to the database will be prompted when starting the server.

The frontend uses the [`config.php`](crawlserv_frontend/crawlserv/config.php) to gain read-only access to the database. For security reasons, the database account used by the frontend should only have `SELECT` privilege! See this file for details about the test configuration (including the database schema and the user name and password for read-only access to the test database). Replace those values with those for your own database.

The testing environment consists of one PC that runs all three components of the application which can only be accessed locally (using ``localhost``). Therefore, the (randomly created) password in [`config.php`](crawlserv_frontend/crawlserv/config.php) is irrelevant for usage outside the original test environment and needs to be replaced! In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the web server running the frontend uses port 80 for interaction with the user (i.e. his\*her web browser). The mySQL database server uses (default) port 3306.

Please note, that the mySQL server used by crawlserv++ might need some adjustments. First of all, the default character set should be set to standard UTF-8 (`utf8mb4`). Second of all, when processing large corpora, the `max_allowed_packet` should be adjusted, and maybe even set to the maximum value of 1 GiB. See this example `mysql.cnf`:

```
[mysqld]
character-set-server = utf8mb4
max_allowed_packet = 1G
```

On the client side, crawlserv++ will set these values automatically. Due to the restrictions of mySQL, saving corpora (and other field values) larger than 1 GiB is not supported. Larger text corpora will be re-created every time they are used. If logging has been enabled for the module, respective warnings will be written to the log table. Algorithms may either trim or ignore larger data values or throw exceptions logged by the server and stop to work.

Using some algorithms on large corpora may require a large amount of memory. Consider [adjusting the size of your swap](https://bogdancornianu.com/change-swap-size-in-ubuntu/) if memory usage reaches its limit to avoid the server from being killed by the operating system. 

## Database

The application uses exactly one database schema and all tables are prefixed with `crawlserv_`.

The following main tables are created and used:

* **`analyzedtables`**: Index of result tables for analyzing.
* **`configs`**: Crawling, parsing, extracting and analyzing configurations.
* **`corpora`**: Generated text corpora.
* **`extractedtables`**: Index of result tables for extracting.
* **`locales`**: List of locales installed on the server.
* **`log`**: Log entries.
* **`parsedtables`**: Index of result tables for parsing.
* **`queries`**: RegEx, XPath and JSONPointer queries.
* **`targetlocks`**: Locks for target tables.
* **`threads`**: Thread status.
* **`urllists`**: URL lists.
* **`websites`**: Websites.

If not already existing, these tables will be created on startup of the command-and-control server by executing the SQL commands in [`crawlserv/sql/init.sql`](crawlserv/sql/init.sql). See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (`a-z`, `A-Z`, `0-9`, `$`, `\_`) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* **`<namespace of website>_<namespace of URL list>`**: Content of the URL list.
* **`<namespace of website>_<namespace of URL list>_analyzed_<name of target table>`**: Analyzing results.
* **`<namespace of website>_<namespace of URL list>_analyzing_<name of target table>`**: Analyzing status.
* **`<namespace of website>_<namespace of URL list>_crawled`**: Crawled content.
* **`<namespace of website>_<namespace of URL list>_crawling`**: Crawling status.
* **~~`<namespace of website>_<namespace of URL list>_extracted_<name of target table>`~~**: Extracting results.
* **~~`<namespace of website>_<namespace of URL list>_extracting`~~**: Extracting status.
* **`<namespace of website>_<namespace of URL list>_parsed_<name of target table>`**: Parsing results.
* **`<namespace of website>_<namespace of URL list>_parsing`**: Parsing status.

See the source code of the `addUrlList(...)` function in [`Main::Database`](crawlserv/src/Main/Database.cpp) for details about the structure of the non-result tables. Most of the columns of the result tables are specified by the respective parsing, extracting and analyzing configurations. See the code of the `initTargetTable(...)` functions in [`Module::Parser::Database`](crawlserv/src/Module/Parser/Database.cpp), ~~[`Module::Extractor::Database`](crawlserv/src/Extractor/Database.cpp)~~ and [`Module::Analyzer::Database`](crawlserv/src/Module/Analyzer/Database.cpp) accordingly.

## Platform

At the moment, this software has been developed for and tested on **Linux only**.

Developed with Eclipse 2018-09 (4.9.0), Eclipse CDT 9.5.4, Eclipse PDT 6.1.0 and Eclipse Web Tools Platform 3.11.0. Compiled and linked with GNU Make 4.1, cmake/ccmake 3.10.2, gcc 7.3.0. Tested with Apache/2.4.29 and MySQL 8.0.15 for Linux on Ubuntu 18.04.1 (64-bit).

The frontend is optimized for current versions of Mozilla Firefox (e.g. v66.0), but should also run on Chromium (e.g. v73.0).
