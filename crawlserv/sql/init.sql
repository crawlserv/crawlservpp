-- create table for logs
CREATE TABLE IF NOT EXISTS crawlserv_log (id SERIAL, time DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, module TINYTEXT NOT NULL, entry TEXT, PRIMARY KEY(id)) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED, ENGINE=InnoDB

-- create tables for websites, URL lists, queries and configs
CREATE TABLE IF NOT EXISTS crawlserv_websites(id SERIAL, domain TEXT DEFAULT NULL, namespace TINYTEXT NOT NULL, name TEXT NOT NULL, dir TEXT DEFAULT NULL, PRIMARY KEY(id)) CHARACTER SET utf8mb4, ENGINE=InnoDB
CREATE TABLE IF NOT EXISTS crawlserv_urllists(id SERIAL, website BIGINT UNSIGNED NOT NULL, namespace TINYTEXT NOT NULL, name TEXT NOT NULL, case_sensitive BOOLEAN DEFAULT TRUE NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ENGINE=InnoDB
CREATE TABLE IF NOT EXISTS crawlserv_queries(id SERIAL, website BIGINT UNSIGNED DEFAULT NULL, name TEXT NOT NULL, query MEDIUMTEXT NOT NULL, type TINYTEXT NOT NULL, resultbool BOOLEAN DEFAULT false NOT NULL, resultsingle BOOLEAN DEFAULT false NOT NULL, resultmulti BOOLEAN DEFAULT false NOT NULL, resultsubsets BOOLEAN DEFAULT false NOT NULL, textonly BOOLEAN DEFAULT false NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ENGINE=InnoDB
CREATE TABLE IF NOT EXISTS crawlserv_configs(id SERIAL, website BIGINT UNSIGNED DEFAULT NULL, module TINYTEXT NOT NULL, name TEXT NOT NULL, config JSON NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ENGINE=InnoDB

-- create indexes for parsing, extracting and analyzing tables
CREATE TABLE IF NOT EXISTS crawlserv_parsedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4, ENGINE=InnoDB
CREATE TABLE IF NOT EXISTS crawlserv_extractedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4, ENGINE=InnoDB
CREATE TABLE IF NOT EXISTS crawlserv_analyzedtables(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, name TINYTEXT NOT NULL, updated DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4, ENGINE=InnoDB

-- create table for threads
CREATE TABLE IF NOT EXISTS crawlserv_threads(id SERIAL, module TINYTEXT NOT NULL, status TEXT, progress DECIMAL(6,5) DEFAULT 0 NOT NULL, paused BOOLEAN DEFAULT FALSE NOT NULL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, config BIGINT UNSIGNED NOT NULL, last BIGINT UNSIGNED DEFAULT 0 NOT NULL, processed BIGINT UNSIGNED DEFAULT 0 NOT NULL, runtime BIGINT UNSIGNED DEFAULT 0 NOT NULL, pausetime BIGINT UNSIGNED DEFAULT 0 NOT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(config) REFERENCES crawlserv_configs(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4, ENGINE=InnoDB

-- create table for text corpora
CREATE TABLE IF NOT EXISTS crawlserv_corpora(id SERIAL, website BIGINT UNSIGNED NOT NULL, urllist BIGINT UNSIGNED NOT NULL, source_type TINYINT UNSIGNED NOT NULL, source_table TINYTEXT NOT NULL, source_field TINYTEXT NOT NULL, created DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL, savepoint TEXT DEFAULT NULL, corpus LONGTEXT NOT NULL, datemap JSON DEFAULT NULL, articlemap JSON DEFAULT NULL, sentencemap JSON DEFAULT NULL, previous BIGINT UNSIGNED NULL UNIQUE, sources BIGINT UNSIGNED DEFAULT 0 NOT NULL, size BIGINT UNSIGNED DEFAULT NULL, length BIGINT UNSIGNED DEFAULT NULL, chunks BIGINT UNSIGNED NOT NULL, chunk_size BIGINT UNSIGNED DEFAULT NULL, chunk_length BIGINT UNSIGNED DEFAULT NULL, tokens BIGINT UNSIGNED DEFAULT NULL, PRIMARY KEY(id), FOREIGN KEY(website) REFERENCES crawlserv_websites(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(urllist) REFERENCES crawlserv_urllists(id) ON UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(previous) REFERENCES crawlserv_corpora(id) ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED, ENGINE=InnoDB

-- add default queries
INSERT IGNORE INTO crawlserv_queries(id, name, query, type, resultmulti, textonly) VALUES (1, 'Get Links from HTML', '/html/body//a/@href', 'xpath', TRUE, TRUE), (2, 'Get Links from RSS and Sitemap', '//link|//loc', 'xpath', TRUE, TRUE)
