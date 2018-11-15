# crawlserv++
<b>crawlserv++</b> is an application for crawling websites and analyzing textual content on these websites.

<i><b>WARNING:</b> This application is under development. It is neither complete nor adequately documented yet.</i>

The architecture of <b>crawlserv++</b> consists of three main parts:

* The <b>crawlserv++ command-and-control server</b>, written in C++ (source code in <i>crawlserv/src</i>)
* A webserver hosting the <b>crawlserv++ frontend</b> written in HTML, PHP and JavaScript (source code in <i>crawlserv_frontend/crawlserv</i>)
* A mySQL <b>database</b> containing all data (i.e. thread status, configurations, logs, crawled content, parsed data as well as the results of all analyses)

## crawlserv++ command-and-control server

This server contains an embedded web server (implemented by using the [mongoose library](https://github.com/cesanta/mongoose)) for interaction with the <b>crawlserv++ frontend</b> by cross-origin resource sharing (CORS). Access can (and should) be restricted to whitelisted IPs only.

The server performs commands and sends back their results. Some commands need to be confirmed before actually performed and some commands can be restricted by the configuration file loaded by the server. The following commands are implemented (as of November 2018):
* <b>addconfig</b> (arguments: website, module, name, config): add a configuration to the database
* <b>addquery</b> (arguments: website, name, query, type, resultbool, resultsingle, resultmulti, textonly): add a RegEx or XPath query to the database
* <b>addurllist</b> (arguments: website, name, namespace): add a URL list to a website in the database
* <b>addwebsite</b> (arguments: name, namespace, domain): add a website to the database
* <b>allow</b> (argument: ip): allow access for the specified IP(s)
* <b>clearlog</b> (optional argument: module): clear the logs of a specified module or all logs if no module is specified
* <b>deleteconfig</b> (argument: id): delete a configuration from the database
* <b>deletequery</b> (argument: id): delete a RegEx or XPath query from the database
* <b>deleteurllist</b> (argument: id): delete a URL list (and all associated data) from the database
* <b>deletewebsite</b> (argument: id): delete a website (and all associated data) from the database
* <b>disallow</b>: revoke access from all except the initial IP(s) specified by the configuration file
* <b>duplicateconfig</b> (argument: id): duplicate the specified configuration
* <b>duplicatequery</b> (argument: id): duplicate the specified RegEx or XPath query
* <b>duplicatewebsite</b> (argument: id): duplicate the specified website
* <b>kill</b>: kill the server
* <b>log</b> (argument: entry): write a log entry by the frontend into the database
* ~~<b>pauseanalyzer</b>~~ (argument: id): pause a running analyzer by its id
* <b>pausecrawler</b> (argument: id): pause a running crawler by its id
* ~~<b>pauseextractor</b>~~ (argument: id): pause a running extractor by its id
* ~~<b>pauseparser</b>~~ (argument: id): pause a running parser by its id
* ~~<b>startanalyzer</b>~~ (arguments: website, urllist, config): start an analyzer using the specified website, URL list and configuration
* <b>startcrawler</b> (arguments: website, urllist, config): start a crawler using the specified website, URL list and configuration
* ~~<b>startextractor</b>~~ (arguments: website, urllist, config): start an extractor using the specified website, URL list and configuration
* ~~<b>startparser</b>~~ (arguments: website, urllist, config): start a parser using the specified website, URL list and configuration
* ~~<b>stopanalyzer</b>~~ (argument: id): stop a running analyzer by its id
* <b>stopcrawler</b> (argument: id): stop a running crawler by its id
* ~~<b>stopextractor</b>~~ (argument: id): stop a running extractor by its id
* ~~<b>stopparser</b>~~ (argument: id): stop a running parser by its id

The commands and their replies are using the JSON format. See the following examples:

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

<i><b>Response from server:</b> Success (otherwise `"failed":true"` would be sent).</i>

```json
{
 "text":"URL list deleted"
}
```

For more information on the server commands, check out the `Server` class of the server.

As you can already see from the commands, the server also manages threads for performing specific tasks. In theory, an indefinite number of parallel threads can be run, limited only by the hardware provided for the server. There are four different modules (i.e. types of threads) that are implemented by inheritance from the `Thread` template class:

* <b>crawler</b>: Crawling of a website (using custom URLs and following links, downloading the plain content to the database and optionally checking archives using the [Memento Protocol](http://mementoweb.org/)).
* ~~<b>parser</b>~~: Parsing data from the downloaded content with the help of user-defined RegEx and XPath queries.
* ~~<b>extractor</b>~~: Downloading additional data such as comments and social media content.
* ~~<b>analyzer</b>~~: Analyzing textual data using different methods and algorithms.

The server and each thread have their own connection to the database. These connections are handled by inheritance from the `Database` class. Additionally, thread connections to the database (`DatabaseThread` as child class fo `Database`) are wrapped through the `DatabaseModule` class to protect the threads from accidentally using the server connection to the database. See the corresponsing source code for details.

## crawlserv++ frontend

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

## database

The application uses one database scheme and all tables will be prefixed with `crawlserv_`. The following main tables are used:

* <b>log</b>: Log entries.
* <b>websites</b>: Websites.
* <b>urllists</b>: URL lists.
* <b>queries</b>: RegEx and XPath queries.
* <b>configs</b>: Crawling, parsing, extracting and analyzing configurations.
* <b>parsedtables</b>: Result tables for parsing.
* <b>extractedtables</b>: Result tables for extracting.
* <b>analyzedtables</b>: Result tables for analyzing.
* <b>threads</b>: Thread status.

The tables will be created on startup of the command-and-control-server by executing the SQL commands in <i>crawlserv/sql/init.sql</i>. See this file for details about the structure of these tables. The result tables specified in `crawlserv_parsedtables`, `crawlserv_extractedtables` and `crawlserv_analyzedtables` will be created by the different modules as needed (with the structure needed for the performance of the specified tasks).

For each website and each URL list a namespace of at least four allowed characters (a-z, A-Z, 0-9, $, \_) is used. These namespaces determine the names of the tables used for each URL list (also prefixed by `crawlserv_`):

* <b>\<namespace of website\>\_\<namespace of URL list\></b>: Content of the URL list.
* <b>\<namespace of website\>\_\<namespace of URL list\>\_crawled</b>: Crawled content.
* <b>\<namespace of website\>\_\<namespace of URL list\>\_links</b>: Linkage information (which URLs link to which other URLs).

## test configuration

The testing environment consists of one PC that runs all components of the application which can only be accessed locally (by using ``localhost``). The command-and-control server needs a configuration file as argument, the test configuration can be found at <i>crawlserv/config</i>. The frontend uses <i>crawlserv_frontend/crawlserv/php/config.php</i> to provide access to the database. See those files for details about the test configuration.
