-- create table for logs
CREATE TABLE IF NOT EXISTS crawlserv_log (id SERIAL, time DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, module TINYTEXT NOT NULL, entry TEXT, PRIMARY KEY(id)) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED

-- create tables for websites, URL lists, queries and configs
CREATE TABLE IF NOT EXISTS crawlserv_websites(id SERIAL, domain TEXT NOT NULL, namespace TINYTEXT NOT NULL, name TEXT NOT NULL, PRIMARY KEY(id)) CHARACTER SET utf8mb4
CREATE TABLE IF NOT EXISTS crawlserv_urllists(id SERIAL, website BIGINT UNSIGNED NOT NULL, namespace TINYTEXT NOT NULL, name TEXT NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
CREATE TABLE IF NOT EXISTS crawlserv_queries(id SERIAL, website BIGINT UNSIGNED DEFAULT NULL, name TEXT NOT NULL, query MEDIUMTEXT NOT NULL, type TINYTEXT NOT NULL, resultbool BOOLEAN DEFAULT false NOT NULL, resultsingle BOOLEAN DEFAULT false NOT NULL, resultmulti BOOLEAN DEFAULT false NOT NULL, textonly BOOLEAN DEFAULT false NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
CREATE TABLE IF NOT EXISTS crawlserv_configs(id SERIAL, website BIGINT UNSIGNED DEFAULT NULL, module TINYTEXT NOT NULL, name TEXT NOT NULL, config JSON NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci

-- create indexes for parsing, extracting and analyzing tables and their locks
CREATE TABLE IF NOT EXISTS crawlserv_parsedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE)
CREATE TABLE IF NOT EXISTS crawlserv_extractedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE)
CREATE TABLE IF NOT EXISTS crawlserv_analyzedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE)
CREATE TABLE IF NOT EXISTS crawlserv_targetlocks(id SERIAL, tabletype TINYTEXT NOT NULL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, locktime DATETIME DEFAULT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE)

-- create table for threads
CREATE TABLE IF NOT EXISTS crawlserv_threads(id SERIAL, module TINYTEXT NOT NULL, status TEXT, progress DECIMAL(6,5) DEFAULT 0 NOT NULL, paused BOOLEAN DEFAULT FALSE NOT NULL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, config BIGINT UNSIGNED NOT NULL, last BIGINT UNSIGNED DEFAULT 0 NOT NULL, runtime BIGINT UNSIGNED DEFAULT 0 NOT NULL, pausetime BIGINT UNSIGNED DEFAULT 0 NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(config) REFERENCES crawlserv_configs(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4

-- create table for text corpora
CREATE TABLE IF NOT EXISTS crawlserv_corpora(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, source_type TINYINT UNSIGNED NOT NULL, source_table TINYTEXT NOT NULL, source_field TINYTEXT NOT NULL, created DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL, corpus LONGTEXT NOT NULL, datemap JSON DEFAULT NULL, sources BIGINT UNSIGNED DEFAULT 0 NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED
