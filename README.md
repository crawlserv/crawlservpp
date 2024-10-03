[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/banner2-direct.svg)](https://stand-with-ukraine.pp.ua)

_**WARNING!** This application is under development. It is neither complete nor adequately tested._

 _~~Strikethrough~~ means a feature is not implemented yet._

# crawlserv++

**crawlserv++** is an application for crawling websites and analyzing textual content on these websites.

For setting up **crawlserv++** on Ubuntu 20+, follow the extensive [step-by-step installation guide](https://github.com/crawlserv/crawlservpp/blob/master/INSTALL.md).

The architecture of **crawlserv++** consists of three distinct components:

* The **command-and-control server**, written in C++ (source code in [`crawlserv/src`](crawlserv/src)),
* a web server hosting the (quick 'n' dirty) **frontend** written in HTML, PHP and JavaScript (source code in [`crawlserv_frontend/crawlserv`](crawlserv_frontend/crawlserv)),
* a mySQL **database** containing all permanent data (i.e. thread status, configurations, logs, crawled content, parsed and extracted data as well as the results of all analyses).

## Legal Notice

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Before using crawlserv++ for crawling websites and other data, please make sure you are [legally allowed to do so](https://benbernardblog.com/web-scraping-and-crawling-are-perfectly-legal-right/).

You may _not_ use this software

* in any way that breaches any applicable local, national or international law or regulation;
* in any way that is unlawful or fraudulent, or has any unlawful or fraudulent purpose or effect;
* to make unauthorised attempts to access any third party networks.

## Building crawlserv++ on Linux

*See [`.travis.yml`](.travis.yml) for example build environments.*

You can clone the complete source code into the current folder using [`git`](https://git-scm.com/):

```
git clone https://github.com/crawlserv/crawlservpp .
git submodule init
git submodule update
```

The following additional components are required to build crawlserv++ on your system:

* [`cmake`](https://cmake.org/), version 3.8.2 or higher
* [`GNU Make`](https://www.gnu.org/software/make/) or a compatible Makefile parser
* [`gcc`](https://gcc.gnu.org/), version 7 or higher, or [`clang`](https://clang.llvm.org/), version 5 or higher – or any other modern C++ 17 compiler
* a multi-threading library supported by `cmake` like `pthreads` (e.g. `libpthread-stubs0-dev` on Ubuntu)
* a modern C++ Standard Library supporting C++ 17 (e.g., `libstdc++-11-dev`)
* the [`Boost.Iostreams`](https://www.boost.org/doc/libs/1_70_0/libs/iostreams/doc/index.html) library (`libboost-iostreams-dev`)
* the [`Boost.System`](https://www.boost.org/doc/libs/1_69_0/libs/system/doc/html/system.html) library (`libboost-system-dev`)
* the [Eigen](https://eigen.tuxfamily.org/) library (e.g., `libeigen3-dev`)
* the [GNU Aspell](http://aspell.net/) library (`libaspell-dev`)
* the [`libcurl`](https://curl.haxx.se/libcurl/) library (e.g., `libcurl4-openssl-dev`)
* the [`libzip`](https://libzip.org/) library (`libzip-dev`, `zipcmp`, `zipmerge`, and `ziptool`)
* the [MySQL Connector/C++](https://dev.mysql.com/doc/dev/connector-cpp/8.0/) library (`libmysqlcppconn-dev`)
* the [`PCRE`](https://www.pcre.org/) library, version 2 (`libpcre2-dev`)
* the [`pugixml`](https://pugixml.org/) library (`libpugixml-dev`)
* the [`tidy-html5`](http://www.html-tidy.org/) library, version 5 or higher (`libtidy-dev`*)
* the [`uriparser`](https://uriparser.github.io/) library, version 0.9.0 or higher (`liburiparser-dev`*)
* the [`zlib`](https://www.zlib.net/) library (preinstalled on many Linux systems)

*&ast; Older Linux distributions may only have `libtidy-dev` v0.9 and `liburiparser-dev` v0.8.4 available. Install the current [versions](https://github.com/htacg/tidy-html5/releases) [manually](https://github.com/uriparser/uriparser), or add a newer repository, e.g. on Ubuntu via:*

```
echo "deb http://cz.archive.ubuntu.com/ubuntu eoan main universe" | sudo tee -a  /etc/apt/sources.list
```

After installing these components and cloning or downloading the source code, use the terminal to go to the `crawlserv` directory inside the downloaded files (where [`CMakeLists.txt`](crawlserv/CMakeLists.txt) is located) and run the following commands:

```
mkdir build
cd build
cmake ..
```

In case of missing source files, make sure that you initialized and updated all submodules (see above).

If `cmake` was successful and shows `Build files have been written to: [...]`, proceed with:

```
make
```

You can safely ignore warnings from external libraries as long as `make` finishes with `[100%] Built target crawlserv`.

The program should have been built inside the newly created `build` directory.

Leave this directory with `cd ..` before running it.

Note that you need to setup a MySQL server, a frontend (e.g. the one in `crawlserv_frontend` on a web server with PHP support) and personalize your configuration before finally starting the server with `./build/crawlserv config` or any other configuration file as argument. Note that the given default configuration file **needs the TOR service running** at its default ports 9050 (SOCKS5 proxy) and 9051 (control port). Also note that, if you want to change the location of the program, make sure to take the [`sql`](crawlserv/sql) folder with you as it provides basic commands to initialize the database (creating all the global tables on first connection).

The program will ask you for the password of the chosen MySQL user before it proceeds. When `Server is up and running.` is displayed, switch to the frontend to take control of the command-and-control server.

Even without access to the frontend you can shut down the server from the terminal by sending a SIGINT signal (`CTRL+X`). It will wait for all running threads to avoid any loss of data.

**NB!** When compiling the sources manually, the following definitions need to be set in advance:

* `#define PCRE2_CODE_UNIT_WIDTH 8`
* `#define RAPIDJSON_NO_SIZETYPEDEFINE`
* `#define RAPIDJSON_HAS_STDSTRING`
* `#define ZLIB_CONST`
* `#define JSONCONS_NO_DEPRECATED` (optional, but recommended)
* `#define MG_ENABLE_LOG 0` (optional, but recommended)
* `#define MG_MAX_RECV_BUF_SIZE=10000000000` (to enable file uploads for up to 10 GB)
* `#define NDEBUG` (optional, but recommended, if you are not debugging the source code)

If you use `gcc`, add the following arguments to set all of these definitions:

`-DPCRE2_CODE_UNIT_WIDTH=8 -DRAPIDJSON_NO_SIZETYPEDEFINE -DRAPIDJSON_HAS_STDSTRING -DZLIB_CONST -DJSONCONS_NO_DEPRECATED -DMG_ENABLE_LOG=0 -DMG_MAX_RECV_BUF_SIZE=10000000000 -DNDEBUG`

## Command-and-Control Server

The command-and-control server contains an embedded web server (implemented using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the frontend by [cross-origin resource sharing](https://en.wikipedia.org/wiki/Cross-origin_resource_sharing) of JSON code.

In the configuration file, access can (and should) be restricted to specific IPs only.

### Source Code Documentation

[![Documentation](https://codedocs.xyz/crawlserv/crawlservpp.svg)](https://codedocs.xyz/crawlserv/crawlservpp/)

To build the [source code documentation](https://codedocs.xyz/crawlserv/crawlservpp/) you will need [`doxygen`](https://www.doxygen.nl) installed. Use the following command inside the root directory of the repository:

```
doxygen Doxyfile
```

The documentation will be written to `crawlserv/docs`.

### Server Commands

The server performs commands and sends back their results. Some commands need to be confirmed before being actually performed and some commands can be restricted by the configuration file loaded when starting the server. The following commands are implemented (as of December 2020):

* **`addconfig`** (arguments: `website`, `module`, `name`, `config`): Add a configuration to the database.
* **`addquery`** (arguments: `website`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `resultsubsets`, `textonly`): Add a RegEx, XPath or JSONPointer query to the database.
* **`addurllist`** (arguments: `website`, `name`, `namespace`): Add a URL list to a website in the database.
* **`addwebsite`** (arguments: `name`, `namespace`, `domain`): Add a website to the database.
* **`allow`** (argument: `ip`): Allow access for the specified IP(s).
* **`clearlogs`** (optional argument: `module`): Clear the logs of a specified module or all logs if no module is specified.
* **`deleteconfig`** (argument: `id`): Delete a configuration from the database.
* **`deletequery`** (argument: `id`): Delete a RegEx, XPath or JSONPpinter query from the database.
* **`deleteurllist`** (argument: `id`): Delete a URL list (and all associated data) from the database.
* **`deleteurls`** (arguments: `urllist`, `query`): Delete all URLs from the URL list that match the specified query.
* **`deletewebsite`** (argument: `id`): Delete a website (and all associated data) from the database.
* **`disallow`**: Revoke access from all except the initial IP(s) specified by the configuration file.
* **`download`** (argument: `filename`): Download file from the file cache of the server.
* **`duplicateconfig`** (argument: `id`): Duplicate the specified configuration.
* **`duplicatequery`** (argument: `id`): Duplicate the specified RegEx, XPath or JSONPointer query.
* **`duplicatewebsite`** (argument: `id`): Duplicate the specified website.
* **`export`** (arguments: `datatype`, `filetype`, `compression`, [...]): Export data from the database into a file.
* **`import`** (arguments: `datatype`, `filetype`, `compression`, `filename`, [...]): Import data from file into the database.
* **`kill`**: Kill the server.
* **`listdicts`**: Retrieve a list of dictionaries available on the server.
* **`listmdls`**: Retrieve a list of language models available on the server.
* **`log`** (argument: `entry`): Write a log entry by the frontend into the database.
* **`merge`** (arguments: `datatype`, [...]): Merge two tables in the database.
* **`movequery`** (arguments: `id`, `to`): Moves a query to another website.
* **`pauseall`**: Pause all running threads.
* **`pauseanalyzer`** (argument: `id`): Pause a running analyzer by its ID.
* **`pausecrawler`** (argument: `id`): Pause a running crawler by its ID.
* **`pauseextractor`** (argument: `id`): Pause a running extractor by its ID.
* **`pauseparser`** (argument: `id`): Pause a running parser by its ID.
* **`ping`**: Respond with `pong`.
* **`resetanalyzingstatus`** (argument: `urllist`): Reset the analyzing status of an ID-specified URL list.
* **`resetextractingstatus`** (argument: `urllist`): Reset the extracting status of an ID-specified URL list.
* **`resetparsingstatus`** (argument: `urllist`): Reset the parsing status of an ID-specified URL list.
* **`startanalyzer`** (arguments: `website`, `urllist`, `config`): Start an analyzer using the specified website, URL list and configuration.
* **`startcrawler`** (arguments: `website`, `urllist`, `config`): Start a crawler using the specified website, URL list and configuration.
* **`startextractor`** (arguments: `website`, `urllist`, `config`): Start an extractor using the specified website, URL list and configuration.
* **`startparser`** (arguments: `website`, `urllist`, `config`): Start a parser using the specified website, URL list and configuration.
* **`stopanalyzer`** (argument: `id`): Stop a running analyzer by its ID.
* **`stopcrawler`** (argument: `id`): Stop a running crawler by its ID.
* **`stopextractor`** (argument: `id`): Stop a running extractor by its ID.
* **`stopparser`** (argument: `id`): Stop a running parser by its ID.
* **`testquery`** (arguments: `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `resultsubsets`, `textonly`, `text`, `xmlwarnings`): Test a temporary query on the specified text.
* **`unpauseall`**: Unpause all paused threads.
* **`unpauseanalyzer`** (argument: `id`): Unpause a paused analyzer by its ID.
* **`unpausecrawler`** (argument: `id`): Unpause a paused crawler by its ID.
* **`unpauseextractor`** (argument: `id`): Unpause a paused extractor by its ID.
* **`unpauseparser`** (argument: `id`): Unpause a paused parser by its ID.
* **`updateconfig`** (arguments: `id`, `name`, `config`): Update an existing configuration in the database.
* **`updatequery`** (arguments: `id`, `name`, `query`, `type`, `resultbool`, `resultsingle`, `resultmulti`, `resultsubsets`, `textonly`): Update an existing RegEx, XPath or JSONPointer query in the database.
* **`updateurllist`** (arguments: `id`, `name`, `namespace`): Update an existing URL list in the database.
* **`updatewebsite`** (arguments: `id`, `name`, `namespace`, `domain`): Update an existing website in the database.
* **`warp`** (arguments: `thread`, `target`): Let a thread jump to the specified ID.

The commands and their replies are using the JSON format (implemented using the [RapidJSON library](https://github.com/Tencent/rapidjson)).

### File Cache

Apart from these commands, the server automatically handles HTTP file uploads sent as `multipart/form-data`. The part containing the content of the file needs to be named `fileToUpload` (case-sensitive). Uploaded files will be saved to the file cache of the server, using random strings of a specific length (defined by `Main::randFileNameLength` in [`crawlserv/src/Main/WebServer.hpp`](https://codedocs.xyz/crawlserv/crawlservpp/WebServer_8hpp_source.html)) as file names.

The cache is also used to store files generated on data export. Files in the cache can be downloaded using the `download` server command. Note that these files are **temporary** as the file cache will be cleared and all uploaded and/or generated files deleted as soon as the server is restarted. Permanent data will be written to the database instead.

### Example Exchange

Command from frontend to server: Delete the URL list with the ID #1.

```json
{
 "cmd": "deleteurllist",
 "id": 1,
}
```

Response from the server: Command needs to be confirmed.

```json
{
 "confirm": true,
 "text": "Do you really want to delete this URL list?\n!!! All associated data will be lost !!!"
}
````

Response from the frontend: Confirm command.

```json
{
 "cmd": "deleteurllist",
 "id": 1,
 "confirmed": true
}
````

Response from the server: Success (otherwise `"failed":true` would be included in the response).

```json
{
 "text": "URL list deleted."
}
```

For more information on the server commands, see the [documentation](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Main_1_1Server.html) of the `Main::Server` class.

### Threads

As can be seen from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the abstract [`Module::Thread`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Module_1_1Thread.html) class:

* **crawler**: Crawling of websites (using custom URLs and following links to the same \[sub-\]domain, downloading plain content to the database and optionally checking archives using the [Memento API](http://www.mementoweb.org/guide/quick-intro/)).
* **parser**: Parsing of data from URLs and downloaded content using user-defined RegEx, XPath and JSONPointer queries.
* **extractor**: Downloading of additional data such as comments and social media content.
* **analyzer**: Analyzing textual data using different methods and algorithms.

Configurations for these modules are saved as JSON arrays in the shared `configs` table.

Analyzers are implemented by their own set of subclasses &mdash; algorithm classes. The following algorithms are implemented at the moment (as of December 2020):
 
* **AssocOverTime**: Counts co-occurrences between a specific term and different categories in a text corpus over time.
* **CorpusGenerator**: Creates a text corpus and collects statistical information about it.
* **SentimentOverTime**: Analyzes the sentiment surrounding specific terms in a text corpus over time.
* ~~**TermsOverTime**: Counts specific tokens in a text corpus over time.~~
* **TopicModeller**: Generates topics from the documents in a corpus and classifies these documents.
* **WordsOverTime**: Counts articles, sentences, and words over time.

The server and each thread have their own connections to the database. These connections are handled by inheritance from the [`Main::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Main_1_1Database.html) class. Additionally, thread connections to the database (instances of [`Module::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Module_1_1Database.html) as child class of `Main::Database`) are wrapped through the [`Wrapper::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Wrapper_1_1Database.html) class to protect the threads (i.e. their programmers) from accidentally using the server connection to the database and thus compromising thread-safety. See the [source code documentation](https://codedocs.xyz/crawlserv/crawlservpp/) of the command-and-control server for further details.

The parser, extractor and analyzer threads may pre-cache (and therefore temporarily multiply) data in memory, while the crawler threads work directly on the database, which minimizes memory usage. Because the usual bottleneck for parsers and extractors are requests to the crawled/extracted website, **multiple threads are encouraged for crawling and extracting.** **Multiple threads for parsing can be reasonable when using multiple CPU cores**, although some additional memory usage by the in-memory multiplication of data should be expected as well as some blocking because of simultaneous database access. At the same time, a slow database connection or server can have significant impact on performance in any case.

Algorithms need to be specifically optimized for multi-threading. Otherwise, multiple analyzer threads will not improve performance and might even conflict with each other.

### Third-party Libraries

The following third-party libraries are used by the command-and-control server:

* [Asio C++ Library](http://think-async.com/Asio/) (included in `crawlserv/src/_extern/asio`)
* [Boost C++ Libraries](https://www.boost.org/) (`Boost.Core`, `Boost.Iostreams`, `Boost.LexicalCast` and `Boost.Strings`)
* [Eigen](https://eigen.tuxfamily.org/) (`libeigen3-dev`)
* [EigenRand](https://github.com/bab2min/EigenRand) (included in `crawlserv/src/_extern/EigenRand`)
* [GNU Aspell](http://aspell.net/)
* [Howard E. Hinnant's date.h library](https://howardhinnant.github.io/date/date.html) (included in `crawlserv/src/_extern/date`)
* [jsoncons](https://github.com/danielaparker/jsoncons/) (included in `crawlserv/src/_extern/jsoncons`)
* [libcurl](https://curl.haxx.se/libcurl/)
* [libzip](https://libzip.org/)
* [Mapbox Variant](https://github.com/mapbox/variant) (included in `crawlserv/src/_extern/variant`)
* [Mongoose Embedded Web Server](https://github.com/cesanta/mongoose) (included in `crawlserv/src/_extern/mongoose`)
* [MySQL Connector/C++ 8.0](https://dev.mysql.com/doc/connector-cpp/8.0/en/)
* [PCRE2](https://www.pcre.org/)
* [porter2_stemmer](https://github.com/smassung/porter2_stemmer)  (included in `crawlserv/src/_extern/porter2_stemmer`)
* [pugixml](https://github.com/zeux/pugixml)
* [RapidJSON](https://github.com/Tencent/rapidjson) (included in `crawlserv/src/_extern/rapidjson`)
* tomoto, the underlying C++ API of [tomotopy](https://github.com/bab2min/tomotopy) (included in `crawlserv/src/_extern/tomotopy`)
* [tidy-html5](http://www.html-tidy.org/)
* [uriparser](https://github.com/uriparser/uriparser)
* [UTF8-CPP](http://utfcpp.sourceforge.net/) (included in `crawlserv/src/_extern/utf8`)
* [Wapiti](https://github.com/Jekub/Wapiti) (included, modified, in `crawlserv/src/_extern/wapiti`)
* [zlib](https://www.zlib.net/)

While Asio, `date.h`, `jsoncons`, Mongoose, `porter2_stemmer`, RapidJSON, `UTF8-CPP`, and Wapiti are included in the source code and compiled together with the server, all other libraries need to be externally present.

## Frontend

**NB!** The current frontend is a quick-and-dirty solution to test the full functionality of the server. Feel free to implement your own nice frontend solution in your favorite programming language – all you need is a read-only connection to the MySQL database and a HTTP connection for exchanging JSON with the command-and-control server. You may also want to use the provided JSON files in [`crawlserv_frontend/crawlserv/json`](crawlserv_frontend/crawlserv/json) as keeping them up-to-date will inform you about module-specific configuration changes and the implementation of new algorithms.

This frontend is a simple HTML/PHP and JavaScript application that has read-only access to the database on its own and can (under certain conditions) interact with the command-and-control server using the above listed commands when the user wants to perform actions that could change the content of the database.

It provides the following menu structure:

* **Server**: Authorize additional IPs or revoke authorization for all custom IPs, run custom commands and kill the server.
* **Websites**: Manage websites and their URL lists including the download of URL lists as text files. 
* **Queries**: Manage RegEx, XPath and JSONPointer queries saved in the database including the test of such queries on custom texts by the command-and-control server using designated worker threads to avoid interference with the main functionality of the server.
* **Crawlers**: Manage crawling configurations in simple or advanced mode.
* **Parsers**: Manage parsing configurations in simple or advanced mode.
* **Extractors**: Manage extracting configurations in simple or advanced mode.
* **Analyzers**: Manage analyzing configurations in simple or advanced mode.
* **Threads**: Currently active threads and their status. Start, pause and stop specific threads.
* **~~Search:~~** Search crawled content.
* **Content:** View crawled, parsed, extracted and ~~analyzed~~ content.
* **Import/Export:** Import and export URL lists and possibly other data.
* **~~Statistics:~~** Show specific statistics derived from the database.
* **Logs**: Show and delete log entries.

### Third-party Code

The frontend uses the following third-party JavaScript code (to be found in [`crawlserv_frontend/crawlserv/js/external`](crawlserv_frontend/crawlserv/js/external):

* [Array.prototype.equals](https://stackoverflow.com/a/14853974) by Rob Bednak
* [jQuery](https://jquery.com/)
* [jQuery Form](http://jquery.malsup.com/form/)
* [jQuery Redirect](https://github.com/mgalante/jquery.redirect/)
* [jQuery UI](https://jqueryui.com/)
* [Prism](https://prismjs.com/)
* [Tippy.js](https://atomiks.github.io/tippyjs/)

## Configuration

The server needs a configuration file as argument, the test configuration can be found at [`crawlserv/config`](crawlserv/config). Replace the values in this file with those for your own database and server settings. The password for granting the server (full) access to the database will be prompted when starting the server.

The frontend uses the [`config.php`](crawlserv_frontend/crawlserv/config.php) to gain read-only access to the database. For security reasons, the database account used by the frontend should only have `SELECT` privilege! See this file for details about the test configuration (including the database schema and the user name and password for read-only access to the test database). Replace those values with those for your own database.

The testing environment consists of one PC that runs all three components of the application which can only be accessed locally (using ``localhost``). Therefore, the (randomly created) password in [`config.php`](crawlserv_frontend/crawlserv/config.php) is irrelevant for usage outside the original test environment and needs to be replaced! In this (test) case, the command-and-control server uses port 8080 for interaction with the frontend while the web server running the frontend uses port 80 for interaction with the user (i.e. his\*her web browser). The MySQL database server uses (default) port 3306.

Please note, that the MySQL server used by crawlserv++ might need some adjustments. First of all, the default character set should be set to standard UTF-8 (`utf8mb4`). Second of all, when processing large data, the `max_allowed_packet` should be adjusted, and maybe even set to the maximum value of 1 GiB. See this example `mysql.cnf`:

```
[mysqld]
character-set-server = utf8mb4
max_allowed_packet = 1G
```

On the client side, crawlserv++ will set these values automatically.

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
* **`threads`**: Thread status.
* **`urllists`**: URL lists.
* **`versions`**: Versions of external libraries.
* **`websites`**: Websites.

If not already existing, these tables will be created on startup of the command-and-control server by executing the SQL commands in [`crawlserv/sql/init.sql`](crawlserv/sql/init.sql). See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (`a-z`, `A-Z`, `0-9`, `$`, `\_`) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* **`<namespace of website>_<namespace of URL list>`**: Content of the URL list.
* **`<namespace of website>_<namespace of URL list>_analyzed_<name of target table>`**: Analyzing results.
* **`<namespace of website>_<namespace of URL list>_analyzing_<name of target table>`**: Analyzing status.
* **`<namespace of website>_<namespace of URL list>_crawled`**: Crawled content.
* **`<namespace of website>_<namespace of URL list>_crawling`**: Crawling status.
* **`<namespace of website>_<namespace of URL list>_extracted_<name of target table>`**: Extracting results.
* **`<namespace of website>_<namespace of URL list>_extracting`**: Extracting status.
* **`<namespace of website>_<namespace of URL list>_parsed_<name of target table>`**: Parsing results.
* **`<namespace of website>_<namespace of URL list>_parsing`**: Parsing status.

See the source code of the `addUrlList(...)` function in [`Main::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Main_1_1Database.html) for details about the structure of the non-result tables. Most of the columns of the result tables are specified by the respective parsing, extracting and analyzing configurations. See the code of the `initTargetTable(...)` functions in [`Module::Parser::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Module_1_1Parser_1_1Database.html), [`Module::Extractor::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Module_1_1Extractor_1_1Database.html) and [`Module::Analyzer::Database`](https://codedocs.xyz/crawlserv/crawlservpp/classcrawlservpp_1_1Module_1_1Analyzer_1_1Database.html) accordingly.

## Platform

At the moment, this software has been developed for and tested on **Linux only**.

Developed with Eclipse 2020-03 (4.15.0), Eclipse CDT 9.11.0, Eclipse PDT 7.1.0 and Eclipse Web Tools Platform 0.6.100. Compiled and linked with GNU Make 4.2.1, cmake/ccmake 3.16.3, gcc 9.3.0. Tested with Apache/2.4.41 and MySQL 8.0.21 on Ubuntu 20.04.1 LTS [focal] and Ubuntu 21.10 [impish] (both 64-bit).

The frontend is optimized for current versions of Mozilla Firefox (e.g. v79.0), but should also run on Chromium (e.g. v84.0), and other modern browsers.
