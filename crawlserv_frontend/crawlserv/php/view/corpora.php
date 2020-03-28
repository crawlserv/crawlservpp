<?php

/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * corpora.php
 *
 * Show corpus data of the currently selected URL and version.
 *
 */ 

require "php/include/corpus.php";

/*
 * CHECK REQUIREMENTS
 */

// check for MySQL 8.0.2 (or later) needed for JSON_TABLE() and $[last] keyword
$result = $dbConnection->query("SELECT VERSION() AS version");

if(!$result)
    die("ERROR: Could not check MySQL version");
    
$row = $result->fetch_assoc();

if(!$row)
    die("ERROR: Could not check MySQL version");
    
$mysql_version = $row["version"];

$result->close();

$success = true;

if($mysql_version < "8" || $mysql_version == "8.0.0" || $mysql_version == "8.0.1") {
    echo "<br><i>This feature needs at least <b>MySQL 8.0.2</b> on the database server.<br><br>\n";

    $success = false;
}

// check for UTF-8 support needed for mb_strlen() and mb_substr()
if(!extension_loaded("mbstring")) {
    echo "<br><i>This feature needs UTF-8 support on the PHP server.<br>\n";
    echo "The following extension is missing: <b>mbstring</b>.<br><br></i>\n";
    
    $success = false;
}

if($success) {
    /*
     * GET CORPORA FOR SELECTED WEBSITE AND URL LIST
     */
    
    // get corpus data
    $result = $dbConnection->query(
            "SELECT
              id,
              source_type,
              source_table,
              source_field,
              created
             FROM
              `crawlserv_corpora`
             WHERE
              website = $website
              AND urllist = $urllist
              AND previous IS NULL
              ORDER BY id"
    );
    
    if(!$result)
        die("ERROR: Could not get corpora for URL list from database.");
        
    if($result->num_rows) {
        // show corpus selection
        $first = true;
        
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Corpus:</div>\n";
        echo "<div class=\"entry-input\">\n";
        echo "<select id=\"corpus-select\">\n";
        
        if(isset($_POST["corpus"]))
            $corpus = $_POST["corpus"];
            
        while($row = $result->fetch_assoc()) {
            // get corpus source type
            switch($row["source_type"]) {
            case 0:
                // parsed data
                $source = "parsed";
                
                break;
                
            case 1:
                // extracted data
                $source = "extracted";
                
                break;
                
            case 2:
                // analyzed data
                $source = "analyzed";
                
                break;
                
            case 3:
                // raw crawled data
                $source = "crawled";
                
                break;
                
            default:
                die("ERROR: Unknwon corpus source type:".$row["source_type"].".");
            }
            
            // add to selection
            echo "<option value=\"".$row["id"]."\"";
            
            if($first && !isset($_POST["corpus"])) {
                // select first corpus if none was already selected
                echo " selected";
                
                $corpus = $row["id"];
                $first = false;
            }
            else if($row["id"] == $corpus)
                echo " selected";
                
            echo ">\n";
            
            echo $source."_".$row["source_table"].".".$row["source_field"]." (".$row["created"].")\n";
            
            echo "</option>\n";
        }
        
        echo "</select>\n";
        
        $result->close();
        
        /*
         * GET INFORMATION ABOUT THE SELECTED CORPUS
         */
        
        // get corpus information
        $result = $dbConnection->query(
                "SELECT
                  id,
                  source_type,
                  source_table,
                  source_field,
                  length,
                  size,
                  chunks,
                  chunk_length,
                  chunk_size
                 FROM `crawlserv_corpora`
                 WHERE
                  id = $corpus
                 LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not get corpus information.");
            
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("ERROR: Could not get corpus information.");
            
        $corpus_where = "website = $website
                            AND urllist = $urllist
                            AND source_type = ".$row['source_type']."
                            AND source_table = '".$row['source_table']."'
                            AND source_field = '".$row['source_field']."'";
        $corpus_total = $row["length"];
        $corpus_size = $row["size"];
        $corpus_chunks = $row["chunks"];
        $chunk_id = $row["id"];
        $chunk_length = $row["chunk_length"];
        $chunk_size = $row["chunk_size"];
        
        $result->close();
        
        echo "<span id=\"content-corpus-size\">".format_bytes($corpus_size)."</span>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        
        /*
         * PICK POSITION AND LENGTH OF SELECTION ACCORDING TO PARAMETERS RECEIVED VIA POST REQUEST IF AVAILABLE
         */
        
        $chunk_n = 0;      /* search from first chunk */
        $corpus_pos = 0;    /* start at beginning by default */
        $corpus_len = 1024; /* select 1024 characters by default */
        $byte_offset = 0;
        $char_offset = 0;
        $trimmed_pos = false;
        $trimmed_len = false;
        $trimmed_article = false;
        $outofmem = false;
        $markers_before = [];
        $markers = [];
        $markers_after = [];
        
        $use_pos = isset($_POST["corpus_pos"]) && is_numeric($_POST["corpus_pos"]) && $_POST["corpus_pos"] >= 0;
        $pos_set = false;
        $use_len = isset($_POST["corpus_len"]) && is_numeric($_POST["corpus_len"]) && $_POST["corpus_len"] >= 0;
        $len_set = false;
        
        if($source == "parsed") {
            // use date to pick position and length of selection
            if(
                isset($_POST["corpus_date"])
                && $_POST["corpus_date"]
            ) {
                // search chunks for date
                $date = $_POST["corpus_date"];
                
                $result = $dbConnection->query(
                        "SELECT id, chunk_size,
                         JSON_EXTRACT(
                                 datemap,
                                 CONCAT(
                                        SUBSTR(
                                                path,
                                                2,
                                                CHAR_LENGTH(path) - 4
                                        ),
                                        '.p'
                                )
                        ) AS p,
                        JSON_EXTRACT(
                                datemap,
                                CONCAT(
                                        SUBSTR(
                                                path,
                                                2,
                                                CHAR_LENGTH(path) - 4
                                        ),
                                        '.l'
                                )
                         ) AS l
                         FROM (
                                SELECT id, chunk_size, datemap,
                                JSON_SEARCH(
                                        datemap,
                                        'one',
                                        '$date',
                                        NULL,
                                        '$[*].v'
                                ) AS path
                                FROM `crawlserv_corpora`
                                WHERE $corpus_where
                        ) AS tmp"
                );
                    
                if(!$result)
                    die("ERROR: Could not search for dates in the chunks of the corpus");
                        
                // find first occurence of date
                $offset = 0;
                
                while($row = $result->fetch_assoc()) {
                    if(!is_null($row["p"])) {
                        $corpus_pos = $offset + $row["p"];
                        $use_pos = true;
                        $pos_set = true;
                        
                        break;
                    }
                    
                    $offset += $row["chunk_size"];
                }
                
                if($pos_set) {
                    // find length of date
                    $corpus_len = byte_to_character_length($row["p"], $row["l"], $row["id"]);
                    
                    $use_len = true;
                    $len_set = true;
                    
                    while($row = $result->fetch_assoc()) {
                        if(is_null($row["p"]))
                            break;
                            
                        $corpus_len += byte_to_character_length($row["p"], $row["l"], $row["id"]);
                    }
                }
                
                $result->close();
            }
            // use article ID to pick position and length of selection
            else if(
                    isset($_POST["corpus_article_id"])
                    && $_POST["corpus_article_id"]
            ) {
                // search chunks for article ID
                $article_id = $_POST["corpus_article_id"];
                
                $result = $dbConnection->query(
                        "SELECT chunk_size,
                         JSON_EXTRACT(
                                 articlemap,
                                 CONCAT(
                                        SUBSTR(
                                                path,
                                                2,
                                                CHAR_LENGTH(path) - 4
                                        ),
                                        '.p'
                                )
                        ) AS p
                        FROM (
                                SELECT chunk_size, articlemap, JSON_SEARCH(
                                        articlemap,
                                        'one',
                                        '$article_id%',
                                        NULL,
                                        '$[*].v'
                                ) AS path
                                FROM `crawlserv_corpora`
                                WHERE $corpus_where
                        ) AS tmp"
                );
                        
                if(!$result)
                    die("ERROR: Could not search for article IDs in the chunks of the corpus");
                            
                // find first occurence of article ID
                $offset = 0;
                
                while($row = $result->fetch_assoc()) {
                    if(!is_null($row["p"])) {
                        $corpus_pos = $offset + $row["p"];
                        $use_pos = true;
                        $pos_set = true;
                        
                        break;
                    }
                    
                    $offset += $row["chunk_size"];
                }
                
                $result->close();
            }
           
            $article_offset = 0;
            $chunk_sql = "";
            
            // use article number to pick position and length of selection
            if(
                    isset($_POST["corpus_article_num"])
                    && is_numeric($_POST["corpus_article_num"])
            ) {
                $article_num = $_POST["corpus_article_num"];
                $use_pos = false;
                $use_len = false;
                
                // search for chunk with article inside
                $result = $dbConnection->query(
                        "SELECT
                          id,
                          chunk_size,
                          chunk_length,
                          articlemap->>'$[0].v' AS first,
                          articlemap->>'$[last].v' AS last,
                          JSON_LENGTH(articlemap) AS articles
                         FROM `crawlserv_corpora`
                         WHERE $corpus_where
                         ORDER BY id;"
                );
                
                if(!$result)
                    die("ERROR: Could not extract article metadata");
                    
                $first = 1;
                $last_id = "";
                
                while($row = $result->fetch_assoc()) {
                    $chunk_id = $row["id"];
                    $chunk_size = $row["chunk_size"];
                    $chunk_length = $row["chunk_length"];
                    
                    if(strlen($last_id) && $row["first"] == $last_id)
                        --$article_offset;
                        
                    $last_id = $row["last"];
                    $last_article = $article_offset + $row["articles"];
                    
                    if($article_num >= $first && $article_num <= $last_article)
                        // found article in current chunk
                        break;
                    
                    if($chunk_n + 1 == $corpus_chunks) {
                        // reached end of last chunk: set article number to maximum
                        $article_num = $last_article;
                        
                        $trimmed_article = true;
                        
                        break;
                    }
                    
                    $chunk_previous = $chunk_id;
                    $byte_offset += $chunk_size;
                    $char_offset += $chunk_length;
                    
                    $article_offset = $last_article;
                    
                    ++$chunk_n;
                }
                
                $result->close();
                
                if(!$chunk_id)
                    die("ERROR: Did not find any chunk of the current corpus");
                    
                $chunk_sql = "AND id = $chunk_id";
            }
            // use manually set position for selection
            else if($use_pos) {
                if(!$pos_set)
                    $corpus_pos = $_POST["corpus_pos"];
                    
                // get chunk ID for corpus position
                $result = $dbConnection->query(
                        "SELECT
                          id,
                          chunk_size,
                          chunk_length,
                          `articlemap`->>'$[0].v' AS first,
                          `articlemap`->>'$[last].v' AS last,
                          JSON_LENGTH(`articlemap`) AS articles
                          FROM `crawlserv_corpora`
                          WHERE $corpus_where
                          ORDER BY id;"
                );
                
                if(!$result)
                    die("Could not get chunk sizes");
                    
                $last_id = "";
                
                while($row = $result->fetch_assoc()) {
                    $chunk_id = $row["id"];
                    $chunk_size = $row["chunk_size"];
                    $chunk_length = $row["chunk_length"];
                    
                    if(strlen($last_id) && $row["first"] == $last_id)
                        --$article_offset;
                        
                    $last_id = $row["last"];
                    
                    if($corpus_pos < $byte_offset + $chunk_size)
                        // found position in current chunk
                        break;
                        
                    if($chunk_n + 1 == $corpus_chunks) {
                        // reached end of last chunk: set corpus pos to end of corpus
                        $corpus_pos = $byte_offset + $chunk_size;
                        
                        $trimmed_pos = true;
                        
                        break;
                    }
                    
                    $chunk_previous = $chunk_id;
                    $byte_offset += $chunk_size;
                    $char_offset += $chunk_length;
                    
                    $article_offset += $row["articles"];
                    
                    ++$chunk_n;
                }
                
                $result->close();
                
                if(!$chunk_id)
                    die("ERROR: Did not find any chunk of the current corpus");
                    
                $chunk_sql = "AND id = $chunk_id";
                
                // get the number of the article at the specified position in the current chunk
                $rel_pos = $corpus_pos - $byte_offset;
                
                $result = $dbConnection->query(
                        "SELECT tmp.n
                        FROM `crawlserv_corpora` t,
                        JSON_TABLE(
                                t.articlemap,
                                '$[*]' COLUMNS(
                                        n FOR ORDINALITY,
                                        p BIGINT UNSIGNED PATH '$.p',
                                        l BIGINT UNSIGNED PATH '$.l'
                                )
                        ) AS tmp
                        WHERE t.id = $chunk_id
                        AND $rel_pos >= tmp.p
                        AND $rel_pos <= tmp.p + tmp.l
                        LIMIT 1"
                );
                    
                if(!$result)
                    die("ERROR: Could not get number of the article at #$corpus_pos");
                    
                $row = $result->fetch_assoc();
                
                if(!$row)
                    die("ERROR: Could not get number of the article at #$corpus_pos");
                    
                $article_num = $article_offset + $row["n"];
                    
                $result->close();
            }
            else {
                // use first article by default
                $article_num = 1;
                $chunk_sql = "ORDER BY id";
            }
            
            // get position, length and ID of (N + 1)th article in current chunk
            $N = $article_num - $article_offset - 1;
            
            $result = $dbConnection->query(
                    "SELECT `articlemap`->'$[$N]' AS json,
                    JSON_LENGTH(`articlemap`) AS n
                    FROM `crawlserv_corpora`
                    WHERE $corpus_where
                    $chunk_sql
                    LIMIT 1"
                );
            
            if(!$result)
                die("ERROR: Could not get article information");
                
            $row = $result->fetch_assoc();
            
            if(!$row)
                die("ERROR: Could not get article information");
                
            $decoded = json_decode($row["json"], true);
            
            $n = $row["n"];
            
            $result->close();
            
            if(json_last_error() != JSON_ERROR_NONE)
                die("ERROR: Could not get article information - ".json_last_error_msg());
                
            // set position and length if necessary, as well as article ID
            if($use_pos) {
                if(!$pos_set)
                    $corpus_pos = $_POST["corpus_pos"];
            }
            else
                $corpus_pos = $byte_offset + $decoded["p"];
                
            if($use_len) {
                if(!$len_set)
                    $corpus_len = $_POST["corpus_len"];
            }
            else {
                $corpus_len = byte_to_character_length(
                        $corpus_pos - $byte_offset,
                        $decoded["l"],
                        $chunk_id
                );
                
                if($n == $N + 1) {
                    // end of chunk: check next chunk(s) for more article length
                    $parent = $chunk_id;
                    $next = true;
                    
                    while($next) {                        
                        $result = $dbConnection->query(
                                "SELECT
                                  id,
                                  chunk_length,
                                  `articlemap`->'$[0].p' AS start,
                                  CHAR_LENGTH(
                                        CONVERT(
                                                SUBSTR(
                                                        BINARY corpus,
                                                        1,
                                                        `articlemap`->'$[0].l'
                                                ) USING utf8mb4
                                        )
                                  ) AS l
                                  FROM `crawlserv_corpora`
                                  WHERE `previous` = $parent
                                   AND `articlemap`->>'$[0].v' LIKE '".$decoded['v']."'
                                  LIMIT 1"
                        );
                        
                        if(!$result)
                            die("ERROR: Could not check next chunks for continuation of article");
                            
                        $row = $result->fetch_assoc();
                        
                        if($row) {
                            if($row["start"] > 0) {
                                $result->close();
                                
                                die(
                                        "ERROR: Corpus is compromised
                                        - continuation of article does not start at beginning of next chunk"
                                );
                            }
                            
                            if($row["l"] > $row["chunk_length"]) {
                                $result->close();
                                
                                die(
                                        "ERROR: Corpus is compromised
                                        - length of article exceeds length of chunk"
                                );
                            }
                            
                            $parent = $row["id"];
                            
                            $corpus_len += $row["l"];
                            
                            $next = $row["l"] == $row["chunk_length"];
                        }
                        else
                            $next = false; 
                        
                        $result->close();
                    }
                }
            }
            
            $article_id = $decoded["v"];
            
            /*
             * DETERMINE DATE AT THE CURRENT POSITION IN CORPUS
             */
            
            $rel_pos = $corpus_pos - $byte_offset;
            
            $result = $dbConnection->query(
                    "SELECT tmp.v
                      FROM `crawlserv_corpora` t,
                      JSON_TABLE(
                            t.datemap,
                            '$[*]' COLUMNS(
                                    p BIGINT UNSIGNED PATH '$.p',
                                    l BIGINT UNSIGNED PATH '$.l',
                                    v DATE PATH '$.v'
                            )
                    ) AS tmp
                    WHERE t.id = $chunk_id
                    AND $rel_pos >= tmp.p
                    AND $rel_pos <= tmp.p + tmp.l
                    LIMIT 1"
            );
            
            if(!$result)
                die("ERROR: Could not get date at current position");
                
            $row = $result->fetch_assoc();
            
            if($row)
                $date = $row["v"];
            else
                $date = ""; /* no date at current position */
                
            $result->close();
        }
        else {
            // use manually set position and length for selection
            if(isset($_POST["corpus_pos"]))
                $corpus_pos = $_POST["corpus_pos"];
            
            if(isset($_POST["corpus_len"]))
                $corpus_len = $_POST["corpus_len"];
        }
        
        /*
         * SELECT CHUNK AT CURRENT POSITION
         */
        
        // select chunk
        for(; $chunk_n < $corpus_chunks; ++$chunk_n) {
            if($corpus_pos - $byte_offset < $chunk_size)
                break;
                
            if($chunk_n + 1 == $corpus_chunks) {
                // reached end of last chunk -> set position to maximum
                $corpus_pos = $byte_offset + $chunk_size;
                $trimmed_pos = true;
                
                break;
            }
            
            $chunk_previous = $chunk_id;
            $byte_offset += $chunk_size;
            $char_offset += $chunk_length;
            
            $result = $dbConnection->query(
                    "SELECT
                      id,
                      chunk_length,
                      chunk_size
                    FROM `crawlserv_corpora`
                    WHERE $corpus_where
                     AND previous = $chunk_previous
                    LIMIT 1"
            );
            
            if(!$result)
                die("ERROR: Could not get information about chunk #".($chunk_n + 1)." of corpus");
                
            $row = $result->fetch_assoc();
            
            if(!$row)
                die("ERROR: Could not get information about chunk #".($chunk_n + 1). " of corpus");
                
            $chunk_id = $row["id"];
            $chunk_length = $row["chunk_length"];
            $chunk_size = $row["chunk_size"];
            
            $result->close();
        }
        
        // convert byte position to character position
        $byte_pos = $corpus_pos - $byte_offset;
        $rel_byte_pos = $byte_pos;
        
        $corpus_pos = byte_to_character_index($byte_pos, $chunk_id);
        $trimmed_pos = $trimmed_pos || $byte_pos < $rel_byte_pos;
        
        $byte_pos += $byte_offset;
        
        // trim length if necessary
        if($char_offset + $corpus_pos + $corpus_len > $corpus_total) {
            $corpus_len = $corpus_total - $corpus_pos - $char_offset;
            $trimmed_len = true;
        }
        
        $memory_limit = setting_to_bytes(ini_get('memory_limit'));
        
        if(($corpus_len + 1024) * 4 > $memory_limit) {
            $corpus_len = ($memory_limit - 1024) / 4;
            $trimmed_len = true;
            $outofmem = true;
        }
        
        /*
         * SHOW (UP TO) 512 CHARACTERS BEFORE THE CURRENT SELECTION
         */
        
        // show up to 512 characters before
        $corpus_before = 0;
        
        if($corpus_pos > 512) {
            $corpus_before = $corpus_pos - 512;
            $corpus_before_l = 512;
        }
        else {
            $corpus_before = 0;
            $corpus_before_l = $corpus_pos;
        }
        
        /*
         * SHOW INPUTS WITH PARAMETERS OF CURRENT SELECTION
         */
        
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Position:</div>\n";
        echo "<div class=\"entry-input\">\n";
        
        echo "<input id=\"corpus-position\" class=\"trigger";
        
        if($trimmed_pos)
            echo " updated";
            
        echo "\" type=\"number\" data-trigger=\"content-go-position\" value=\"$byte_pos\" />\n";
        
        echo "<span class=\"content-small\">(in bytes)</span>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Length:</div>\n";
        echo "<div class=\"entry-input\">\n";
        
        echo "<input id=\"corpus-length\" class=\"trigger";
        
        if($trimmed_len)
            echo " updated";
            
        echo "\" type=\"number\" data-trigger=\"content-go-position\" value=\"$corpus_len\" />\n";
        
        echo "<span class=\"content-small\">(in characters)</span>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        
        echo "<div class=\"action-link-box\">\n";
        echo "<div class=\"action-link\">\n";
        
        echo    "<a
                    href=\"#\"
                    id=\"content-go-position\"
                    class=\"action-link\"
                    data-m=\"$m\"
                    data-tab=\"$tab\"
                >Go</a>\n";
            
        echo "</div>\n";
        echo "</div>\n";
        
        echo "</div>\n";
        
        echo "<div class=\"content-block\">\n";
        
        if($source == "parsed") {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Article #:</div>\n";
            
            echo "<div class=\"entry-input\">\n";
            
            echo "<input id=\"corpus-article-num\" type=\"number\" class=\"trigger\"";
            
            if($trimmed_article)
                echo " updated";
                
            echo " data-trigger=\"content-go-article-num\" value=\"$article_num\"/>\n";
            
            echo    "<a
                        href=\"#\"
                        id=\"content-go-article-num\"
                        class=\"action-link-inline trigger\"
                        data-m=\"$m\"
                        data-tab=\"$tab\"
                    >Go</a>\n";
                
            echo "</div>\n";
            echo "</div>\n";
            
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Article ID:</div>\n";
            echo "<div class=\"entry-input\">\n";
            
            echo "<input id=\"corpus-article-id\" type=\"text\" class=\"trigger\"
                    data-trigger=\"content-go-article-id\" value=\"$article_id\"/>\n";
                
            echo    "<a
                        href=\"#\"
                        id=\"content-go-article-id\"
                        class=\"action-link-inline\"
                        data-m=\"$m\"
                        data-tab=\"$tab\"
                    >Go</a>\n";
                
            echo "</div>\n";
            echo "</div>\n";
            
            echo "</div>\n";
            
            echo "<div class=\"content-block\">\n";
            
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Date:</div>\n";
            echo "<div class=\"entry-input\">\n";
            
            echo "<input id=\"corpus-date\" type=\"text\" value=\"$date\"
                    data-m=\"$m\" data-tab=\"$tab\">\n";
                
            echo "</div>\n";
            echo "</div>\n";
        }
        
        echo "</div>\n";
        
        echo "<div id=\"content-corpus\" class=\"content-block\">\n";
        
        /*
         * GET TEXT FROM THE FIRST RELEVANT CHUNK FOR CURRENT SELECTION
         */
        
        $p = $corpus_before;
        $l = $corpus_before_l + $corpus_len + 512;
        
        $result = $dbConnection->query(
                "SELECT
                  SUBSTR(
                        corpus,
                        $p + 1,
                        $l
                  ) AS excerpt,
                  LENGTH(
                        SUBSTR(
                                corpus,
                                $p + 1,
                                $corpus_before_l
                        )
                  ) AS before_size,
                  chunk_size
                 FROM `crawlserv_corpora`
                 WHERE $corpus_where
                  AND id = $chunk_id
                 LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not get excerpt from corpus");
            
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("ERROR: Could not get excerpt from corpus");
            
        $corpus_excerpt = $row["excerpt"];
        $before_size = $row["before_size"];
        $chunk_size = $row["chunk_size"];
        
        $result->close();
        
        // get relevant articles and dates if necessary
        $before_pos = 0;
        $byte_end = 0;
        $byte_after = 0;
        
        if($source == "parsed") {
            $before_pos = $byte_pos - $before_size;
            $before_pos_relative = $before_pos - $byte_offset;
            $byte_end = $before_pos + strlen($corpus_excerpt);
            $byte_end_relative = $byte_end - $byte_offset;
            $byte_after = $rel_byte_pos + strlen(
                    mb_substr(
                            $corpus_excerpt,
                            $corpus_before_l,
                            $corpus_len,
                            "UTF-8"
                    )
            );
            
            // get relevant articles
            $result = $dbConnection->query(
                    "SELECT
                      tmp.p AS p,
                      tmp.l AS l,
                      tmp.v AS v
                     FROM `crawlserv_corpora` t,
                      JSON_TABLE(
                            t.articlemap,
                            '$[*]' COLUMNS(
                                    p BIGINT UNSIGNED PATH '$.p',
                                    l BIGINT UNSIGNED PATH '$.l',
                                    v TEXT PATH '$.v'
                            )
                      ) AS tmp
                     WHERE t.id = $chunk_id
                      AND (
                            $before_pos_relative BETWEEN tmp.p AND tmp.p + tmp.l
                            OR $byte_end BETWEEN tmp.p AND tmp.p + tmp.l
                            OR tmp.p BETWEEN $before_pos_relative AND $byte_end_relative
                      )"
            );
                    
            if(!$result)
                die("ERROR: Could not get articles");
                
            while($row = $result->fetch_assoc()) {
                if($row["p"] >= $before_pos_relative) {
                    if($row["p"] < $rel_byte_pos)
                        array_push(
                                $markers_before,
                                [
                                    $byte_offset + $row["p"],
                                    "art_beg",
                                    $row["v"]
                                ]
                        );
                    else if($row["p"] <= $byte_after)
                        array_push(
                                $markers,
                                [
                                    $byte_offset + $row["p"],
                                    "art_beg",
                                    $row["v"]
                                ]
                        );
                    else
                        array_push(
                                $markers_after,
                                [
                                    $byte_offset + $row["p"],
                                    "art_beg",
                                    $row["v"]
                                ]
                        );
                }
                
                if($row["p"] + $row["l"] <= $byte_end_relative) {
                    if($row["p"] + $row["l"] < $rel_byte_pos)
                        array_push(
                                $markers_before,
                                [
                                    $byte_offset + $row["p"] + $row["l"],
                                    "art_end",
                                    $row["v"]
                                ]
                        );
                    else if($row["p"] + $row["l"] <= $byte_after)
                        array_push(
                                $markers,
                                [
                                    $byte_offset + $row["p"] + $row["l"],
                                    "art_end",
                                    $row["v"]
                                ]
                        );
                    else
                        array_push(
                                $markers_after,
                                [
                                    $byte_offset + $row["p"] + $row["l"],
                                    "art_end",
                                    $row["v"]
                                ]
                        );
                }
            }
            
            $result->close();
            
            // get relevant dates
            $result = $dbConnection->query(
                    "SELECT
                      tmp.p AS p,
                      tmp.l AS l,
                      tmp.v AS v
                     FROM
                      `crawlserv_corpora` t,
                      JSON_TABLE(
                            t.datemap,
                            '$[*]' COLUMNS(
                                    p BIGINT UNSIGNED PATH '$.p',
                                    l BIGINT UNSIGNED PATH '$.l',
                                    v TEXT PATH '$.v'
                            )
                      ) AS tmp
                      WHERE t.id = $chunk_id
                       AND (
                            $before_pos_relative BETWEEN tmp.p AND tmp.p + tmp.l
                            OR $byte_end BETWEEN tmp.p AND tmp.p + tmp.l
                            OR tmp.p BETWEEN $before_pos_relative AND $byte_end_relative
                       )"
            );
            
            if(!$result)
                die("ERROR: Could not get dates");
                
            while($row = $result->fetch_assoc()) {
                if($row["p"] >= $before_pos_relative) {
                    if($row["p"] < $rel_byte_pos)
                        array_push(
                                $markers_before,
                                [
                                        $byte_offset + $row["p"],
                                        "dat_beg",
                                        $row["v"]
                                ]
                        );
                    else if($row["p"] <= $byte_after)
                        array_push(
                                $markers,
                                [
                                        $byte_offset + $row["p"],
                                        "dat_beg",
                                        $row["v"]
                                ]
                        );
                    else
                        array_push(
                                $markers_after,
                                [
                                        $byte_offset + $row["p"],
                                        "dat_beg",
                                        $row["v"]
                                ]
                        );
                }
                
                if($row["p"] + $row["l"] <= $byte_end_relative) {
                    if($row["p"] + $row["l"] < $rel_byte_pos)
                        array_push(
                                $markers_before,
                                [
                                        $byte_offset + $row["p"] + $row["l"],
                                        "dat_end",
                                        $row["v"]
                                ]
                        );
                    else if($row["p"] + $row["l"] <= $byte_after)
                        array_push(
                                $markers,
                                [
                                        $byte_offset + $row["p"] + $row["l"],
                                        "dat_end",
                                        $row["v"]
                                ]
                        );
                    else
                        array_push(
                                $markers_after,
                                [
                                        $byte_offset + $row["p"] + $row["l"],
                                        "dat_end",
                                        $row["v"]
                                ]
                        );
                }
            }
            
            $result->close();
        }
        
        /*
         * GET THE END OF THE PREVIOUS CHUNK IF NECESSARY
         */
        
        if($chunk_n && $corpus_before_l < 512) {
            $result = $dbConnection->query(
                    "SELECT
                      SUBSTR(
                            corpus,
                            CHAR_LENGTH(corpus) - "
                            .(511 - $corpus_before_l) /* + 1 because MySQL strings start at #1 */ ."
                      ) AS `before`,
                      chunk_size
                     FROM `crawlserv_corpora`
                     WHERE id = $chunk_previous
                     LIMIT 1"
            );
                        
            if(!$result)
                die("ERROR: Could not get end of previous chunk");
                            
            $row = $result->fetch_assoc();
            
            if(!$row)
                die("ERROR: Could not get end of previous chunk");
                
            $before = $row["before"];
            $before_bytes = strlen($before);
            $before_pos -= $before_bytes;
            $chunk_previous_size = $row["chunk_size"];
            
            $result->close();
            
            // get additional articles and dates if necessary
            if($source == "parsed") {
                $before_pos_relative = $chunk_previous_size - $before_bytes;
                
                // get additional articles
                $result = $dbConnection->query(
                        "SELECT
                          tmp.p AS p,
                          tmp.l AS l,
                          tmp.v AS v
                         FROM `crawlserv_corpora` t,
                          JSON_TABLE(
                                t.articlemap,
                                '$[*]' COLUMNS(
                                        p BIGINT UNSIGNED PATH '$.p',
                                        l BIGINT UNSIGNED PATH '$.l',
                                        v TEXT PATH '$.v'
                                )
                          ) AS tmp
                         WHERE t.id = $chunk_previous
                          AND tmp.p + tmp.l >= $before_pos_relative"
                );
                                
                if(!$result)
                    die("ERROR: Could not get additional articles from previous chunk");
                
                $added = false;
                
                while($row = $result->fetch_assoc()) {
                    if($row["p"] >= $before_pos_relative)
                        array_push(
                                $markers_before,
                                [
                                    $byte_offset - $chunk_previous_size + $row["p"],
                                    "art_beg",
                                    $row["v"]
                                ]
                    );

                    array_push(
                            $markers_before,
                            [
                                $byte_offset - $chunk_previous_size + $row["p"] + $row["l"],
                                "art_end",
                                $row["v"]
                            ]
                        );
                    
                    $added = true;
                }
                
                $result->close();
                
                // merge last added article if it is identical with the first article
                if($added) {
                    $last = count($markers_before) - 1;
                
                    if($last >= 0) {
                        if(
                                count($markers) > 0
                                && $markers[0][0] == $markers_before[$last][0]
                                && $markers[0][1] == "art_beg"
                                && $markers_before[$last][1] == "art_end"
                                && $markers[0][2] == $markers_before[$last][2]
                        ) {
                            array_splice($markers, 0, 1);
                            array_splice($markers_before, $last, 1);
                        }
                        else if(
                                count($markers) > 1
                                && $markers[1][0] == $markers_before[$last][0]
                                && $markers[1][1] == "art_beg"
                                && $markers_before[$last][1] == "art_end"
                                && $markers[1][2] == $markers_before[$last][2]
                        ) {
                            array_splice($markers, 1, 1);
                            array_splice($markers_before, $last, 1);
                        }
                        else if(
                                $last > 0
                                && $markers_before[0][0] == $markers_before[$last][0]
                                && $markers_before[0][1] == "art_beg"
                                && $markers_before[$last][1] == "art_end"
                                && $markers_before[0][2] == $markers_before[$last][2]
                        ) {
                            // remove last element first, otherwise its index will be invalid
                            array_splice($markers_before, $last, 1);
                            array_splice($markers_before, 0, 1);
                        }
                        else if(
                                $last > 1
                                && $markers_before[1][0] == $markers_before[$last][0]
                                && $markers_before[1][1] == "art_beg"
                                && $markers_before[$last][1] == "art_end"
                                && $markers_before[1][2] == $markers_before[$last][2]
                        ) {
                            // remove last element first, otherwise its index will be invalid
                            array_splice($markers_before, $last, 1);
                            array_splice($markers_before, 1, 1);
                        }
                    }
                }
                
                // get additional dates
                $result = $dbConnection->query(
                        "SELECT
                          tmp.p AS p,
                          tmp.l AS l,
                          tmp.v AS v
                         FROM `crawlserv_corpora` t,
                          JSON_TABLE(
                                t.datemap,
                                '$[*]' COLUMNS(
                                        p BIGINT UNSIGNED PATH '$.p',
                                        l BIGINT UNSIGNED PATH '$.l',
                                        v TEXT PATH '$.v'
                                )
                          ) AS tmp
                         WHERE t.id = $chunk_previous
                          AND tmp.p + tmp.l >= $before_pos_relative"
                );
                
                if(!$result)
                    die("ERROR: Could not get additional dates from previous chunk");
                
                $added = false;
                
                while($row = $result->fetch_assoc()) {
                    if($row["p"] >= $before_pos_relative)
                        array_push(
                                $markers_before,
                                [
                                    $byte_offset - $chunk_previous_size + $row["p"],
                                    "dat_beg",
                                    $row["v"]
                                ]
                        );
                        
                    array_push(
                            $markers_before,
                            [
                                    $byte_offset - $chunk_previous_size + $row["p"] + $row["l"],
                                    "dat_end",
                                    $row["v"]
                            ]
                    );
                    
                    $added = true;
                }
                
                $result->close();
                
                // merge last added date if it is identical with the first date
                if($added) {
                    $last = count($markers_before) - 1;
                    
                    if($last >= 0) {
                        if(
                                count($markers) > 0
                                && $markers[0][0] == $markers_before[$last][0]
                                && $markers[0][1] == "dat_beg"
                                && $markers_before[$last][1] == "dat_end"
                                && $markers[0][2] == $markers_before[$last][2]
                        ) {
                            array_splice($markers, 0, 1);
                            array_splice($markers_before, $last, 1);
                        }
                        else if(
                                count($markers) > 1
                                && $markers[1][0] == $markers_before[$last][0]
                                && $markers[1][1] == "dat_beg"
                                && $markers_before[$last][1] == "dat_end"
                                && $markers[1][2] == $markers_before[$last][2]
                        ) {
                            array_splice($markers, 1, 1);
                            array_splice($markers_before, $last, 1);
                        }
                        else if(
                                $last > 0
                                && $markers_before[0][0] == $markers_before[$last][0]
                                && $markers_before[0][1] == "dat_beg"
                                && $markers_before[$last][1] == "dat_end"
                                && $markers_before[0][2] == $markers_before[$last][2]
                        ) {
                            // remove last element first, otherwise its index will be invalid
                            array_splice($markers_before, $last, 1);
                            array_splice($markers_before, 0, 1);
                        }
                        else if(
                                $last > 1
                                && $markers_before[1][0] == $markers_before[$last][0]
                                && $markers_before[1][1] == "dat_beg"
                                && $markers_before[$last][1] == "dat_end"
                                && $markers_before[1][2] == $markers_before[$last][2]
                        ) {
                            // remove last element first, otherwise its index will be invalid
                            array_splice($markers_before, $last, 1);
                            array_splice($markers_before, 1, 1);
                        }
                    }
                }
            }
        }
        else
            $before = "";
        
        /*
         * GET THE FOLLOWING CHUNK(S) IF NECESSARY
         */
            
        // get more text belonging to the selection (and up to 512 characters beyond)
        //  from the next chunk(s) if necessary
        $more_text = "";
        $cutBack = 512;
        $byte_add = 0;
        
        if($p + $corpus_before_l + $corpus_len + 512 > $chunk_length) {
            for(++$chunk_n; $chunk_n < $corpus_chunks; ++$chunk_n) {
                $byte_add += $chunk_size;
                $rest = $p + $corpus_before_l + $corpus_len - $chunk_length;
                
                $chunk_previous = $chunk_id;
                
                $result = $dbConnection->query(
                        "SELECT
                          SUBSTR(
                                corpus,
                                1,
                                $rest + 512
                          ) AS more_text,
                          IF(
                                $rest > 0,
                                LENGTH(
                                        SUBSTR(
                                                corpus,
                                                1,
                                                $rest
                                        )
                                ),
                                0
                          ) AS byte_after,
                          id,
                          chunk_size,
                          chunk_length
                         FROM `crawlserv_corpora`
                         WHERE previous = $chunk_previous
                         LIMIT 1"
                );
                            
                if(!$result)
                    die("ERROR: Could not get information about chunk #".($chunk_n + 1)." of corpus");
                                
                $row = $result->fetch_assoc();
                
                if(!$row)
                    die("ERROR: Could not get information about chunk #".($chunk_n + 1). " of corpus");
                
                $byte_after = $byte_add + $row["byte_after"];
                $byte_end_relative = strlen($row["more_text"]);
                
                $more_text .= $row["more_text"];
                
                $chunk_id = $row["id"];
                $chunk_size = $row["chunk_size"];
                $chunk_length = $row["chunk_length"];
                
                $result->close();
                
                if($source == "parsed") {                    
                    // get additional articles
                    $result = $dbConnection->query(
                            "SELECT
                              tmp.p + $byte_add AS p,
                              tmp.l AS l,
                              tmp.v AS v
                             FROM `crawlserv_corpora` t,
                              JSON_TABLE(
                                    t.articlemap,
                                    '$[*]' COLUMNS(
                                            p BIGINT UNSIGNED PATH '$.p',
                                            l BIGINT UNSIGNED PATH '$.l',
                                            v TEXT PATH '$.v'
                                    )
                              ) AS tmp
                             WHERE t.id = $chunk_id
                              AND tmp.p < $byte_end_relative"
                    );
                                    
                    if(!$result)
                        die("ERROR: Could not get additional articles from consecutive chunk(s)");
                    
                    $first = true;
                                        
                    while($row = $result->fetch_assoc()) {
                        /*
                         * NOTE:    As part of the MySQL query above,
                         *          the bytes in the previous chunks
                         *          have already been added to $row["p"]!
                         */
                        
                        if(
                                $first
                                && count($markers) > 0
                                && $markers[count($markers) - 1][0] == $byte_offset + $row["p"]
                                && $markers[count($markers) - 1][1] == "art_end"
                                && $markers[count($markers) - 1][2] == $row["v"]
                        )   // merge with previous article by removing its end marker (last element of $markers)
                            array_splice($markers, count($markers) - 1, 1);
                        else if(
                                $first
                                && count($markers) > 1
                                && $markers[count($markers) - 2][0] == $byte_offset + $row["p"]
                                && $markers[count($markers) - 2][1] == "art_end"
                                && $markers[count($markers) - 2][2] == $row["v"]
                        )   // merge with previous article by removing its end marker (second last element of $markers)
                            array_splice($markers, count($markers) - 2, 1);
                        else if(
                                $first
                                && count($markers_after) > 0
                                && $markers_after[count($markers_after) - 1][0] == $byte_offset + $row["p"]
                                && $markers_after[count($markers_after) - 1][1] == "art_end"
                                && $markers_after[count($markers_after) - 1][2] == $row["v"]
                        )   // merge with previous article by removing its end marker (last element of $markers_after)
                            array_splice($markers_after, count($markers_after) - 1, 1);
                        else if(
                                $first
                                && count($markers_after) > 1
                                && $markers_after[count($markers_after) - 2][0] == $byte_offset + $row["p"]
                                && $markers_after[count($markers_after) - 2][1] == "art_end"
                                && $markers_after[count($markers_after) - 2][2] == $row["v"]
                        )   // merge with previous article by removing its end marker (second last element of $markers_after)
                            array_splice($markers_after, count($markers_after) - 2, 1);
                        else if($row["p"] <= $byte_after)
                            array_push(
                                    $markers,
                                    [
                                        $byte_offset + $row["p"],
                                        "art_beg",
                                        $row["v"]
                                    ]
                            );
                        else
                            array_push(
                                    $markers_after,
                                    [
                                        $byte_offset + $row["p"],
                                        "art_beg",
                                        $row["v"]
                                    ]
                                );
                            
                        if($row["p"] + $row["l"] <= $byte_end_relative + $byte_add) {
                            if($row["p"] + $row["l"] <= $byte_after)
                                array_push(
                                        $markers,
                                        [
                                            $byte_offset + $row["p"] + $row["l"],
                                            "art_end",
                                            $row["v"]
                                        ]
                                );
                            else
                                array_push(
                                        $markers_after,
                                        [
                                            $byte_offset + $row["p"] + $row["l"],
                                            "art_end",
                                            $row["v"]
                                        ]
                                    );
                        }
                        
                        $first = false;
                    }
                    
                    $result->close();
                    
                    // get additional dates
                    $result = $dbConnection->query(
                            "SELECT
                              tmp.p + $byte_add AS p,
                              tmp.l AS l,
                              tmp.v AS v
                             FROM `crawlserv_corpora` t,
                              JSON_TABLE(
                                    t.datemap,
                                    '$[*]' COLUMNS(
                                            p BIGINT UNSIGNED PATH '$.p',
                                            l BIGINT UNSIGNED PATH '$.l',
                                            v TEXT PATH '$.v'
                                    )
                              ) AS tmp
                             WHERE t.id = $chunk_id
                              AND tmp.p < $byte_end_relative"
                        );
                    
                    if(!$result)
                        die("ERROR: Could not get additional dates from consecutive chunk(s)");
                    
                    $first = true;
                        
                    while($row = $result->fetch_assoc()) {
                        /*
                         * NOTE:    As part of the MySQL query above,
                         *          the bytes in the previous chunks
                         *          have already been added to $row["p"]!
                         */
                        
                        if(
                                $first
                                && count($markers) > 0
                                && $markers[count($markers) - 1][0] == $byte_offset + $row["p"]
                                && $markers[count($markers) - 1][1] == "dat_end"
                                && $markers[count($markers) - 1][2] == $row["v"]
                        )   // merge with previous date by removing its end marker (last element of $markers)
                            array_splice($markers, count($markers) - 1, 1);
                        else if(
                                $first
                                && count($markers) > 1
                                && $markers[count($markers) - 2][0] == $byte_offset + $row["p"]
                                && $markers[count($markers) - 2][1] == "dat_end"
                                && $markers[count($markers) - 2][2] == $row["v"]
                        )   // merge with previous date by removing its end marker (second last element of $markers)
                            array_splice($markers, count($markers) - 2, 1);
                        else if(
                                $first
                                && count($markers_after) > 0
                                && $markers_after[count($markers_after) - 1][0] == $byte_offset + $row["p"]
                                && $markers_after[count($markers_after) - 1][1] == "dat_end"
                                && $markers_after[count($markers_after) - 1][2] == $row["v"]
                        )   // merge with previous date by removing its end marker (last element of $markers_after)
                            array_splice($markers_after, count($markers_after) - 1, 1);
                        else if(
                                $first
                                && count($markers_after) > 1
                                && $markers_after[count($markers_after) - 2][0] == $byte_offset + $row["p"]
                                && $markers_after[count($markers_after) - 2][1] == "dat_end"
                                && $markers_after[count($markers_after) - 2][2] == $row["v"]
                        )   // merge with previous date by removing its end marker (second last element of $markers_after)
                            array_splice($markers_after, count($markers_after) - 2, 1);
                        else if($row["p"] <= $byte_after)
                            array_push(
                                    $markers,
                                    [
                                            $byte_offset + $row["p"],
                                            "dat_beg",
                                            $row["v"]
                                    ]
                            );
                        else
                            array_push(
                                    $markers_after,
                                    [
                                            $byte_offset + $row["p"],
                                            "dat_beg",
                                            $row["v"]
                                    ]
                            );
                        
                        if($row["p"] + $row["l"] <= $byte_end_relative + $byte_add) {
                            if($row["p"] + $row["l"] <= $byte_after)
                                array_push(
                                        $markers,
                                        [
                                                $byte_offset + $row["p"] + $row["l"],
                                                "dat_end",
                                                $row["v"]
                                        ]
                                );
                            else
                                array_push(
                                        $markers_after,
                                        [
                                                $byte_offset + $row["p"] + $row["l"],
                                                "dat_end",
                                                $row["v"]
                                        ]
                                );
                        }
                        
                        $first = false;
                    }
                    
                    $result->close();
                }
            
                if($rest > $chunk_length)
                    $rest -= $chunk_length;
                else
                    break;
                    
                if($chunk_n + 1 == $corpus_chunks) {
                    if($rest <= 512) {
                        $cutBack = $rest;
                        
                        break;
                    }
                    
                    $rest -= 512;
                    
                    die("ERROR: Reached end of corpus with $rest characters remaining");
                }
            }
        }
        
        /*
         * CHECK FOR MEMORY ERROR
         */
        
        if($outofmem) {
            echo "<div id=\"content-corpus-warning\">\n";
            
            echo "<b>Note:</b> Selection reduced because of memory constraints. Set &quot;memory_limit&quot; accordingly.\n";
            
            echo "</div>\n";
        }
        
        /*
         * PRINT CORPUS SELECTION (AND SURROUNDING) INCLUDING MARKERS
         */

        echo "<span id=\"content-corpus-before\">";
        
        $txt = $before.mb_substr(
                $corpus_excerpt,
                0,
                $corpus_before_l,
                "UTF-8"
        );
        
        $char_pos = $char_offset + $corpus_pos - mb_strlen($txt, "UTF-8");
        
        print_with_markers(
                $before_pos,
                $txt,
                $markers_before,
                $char_pos
        );
        
        // free memory
        unset($txt);
        
        echo "</span>";
                
        echo "<span class=\"content-corpus-control\"
                data-tippy-content=\"Begin of selection<br>\n
                @ byte #$byte_pos<br>\n
                @ char #".($char_offset + $corpus_pos)."\"
                data-tippy-delay=\"0\"
                data-tippy-duration=\"0\"
                data-tippy-arrow=\"true\"
                data-tippy-placement=\"left-start\"
                data-tippy-size=\"small\">&#8614;</span>";
                
        echo "<span id=\"content-corpus-text\">";
                
        $text1 = mb_substr($corpus_excerpt, $corpus_before_l, $corpus_len, "UTF-8");
        $text2 = mb_substr($more_text, 0, mb_strlen($more_text, "UTF-8") - $cutBack, "UTF-8");
        
        print_with_markers(
                $byte_pos,
                $text1.$text2,
                $markers,
                $char_pos
        );
        
        $text_bytes = strlen($text1) + strlen($text2);
        
        if(!$text_bytes)
            echo "<span class=\"content-corpus-control\"
                    data-tippy-content=\"Empty selection<br>\n
                    @ byte #$byte_pos<br>\n
                    @ char #".($char_offset + $corpus_pos)."\"
                    data-tippy-delay=\"0\"
                    data-tippy-duration=\"0\"
                    data-tippy-arrow=\"true\"
                    data-tippy-placement=\"left-start\"
                    data-tippy-size=\"small\">&#8709;</span>";
                    
        echo "</span>";
        
        echo "<span class=\"content-corpus-control\"
                data-tippy-content=\"End of selection<br>\n
                    @ byte #".($byte_pos + $text_bytes)."<br>\n
                    @ char #".($char_offset + $corpus_pos + $corpus_len)."\"
                data-tippy-delay=\"0\"
                data-tippy-duration=\"0\"
                data-tippy-arrow=\"true\"
                data-tippy-placement=\"left-start\"
                data-tippy-size=\"small\">&#8612;</span>";
                    
        // free memory
        unset($text1);
        unset($text2);
        
        echo "<span id=\"content-corpus-after\">";
        
        print_with_markers(
                $byte_pos + $text_bytes,
                mb_substr($corpus_excerpt, $corpus_before_l + $corpus_len, NULL, "UTF-8")
                .mb_substr($more_text, mb_strlen($more_text, "UTF-8") - $cutBack, $cutBack, "UTF-8"),
                $markers_after,
                $char_pos
        );
        
        echo "</span>\n";
    }
    else {
        /*
         * NO CORPORA AVAILABLE
         */
        
        $result->close();
        
        // no corpora for the current URL list available
        //  -> check whether the website has any corpora
        $result = $dbConnection->query(
                "SELECT
                 EXISTS(
                        SELECT * 
                        FROM
                         `crawlserv_corpora` 
                        WHERE
                         website = $website
                ) AS `check`"
        );
        
        if(!$result)
            die("ERROR: Could not get corpora for website from database.");
            
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("ERROR: Could not get corpora for website from database.");
            
        if($row["check"])
            echo "<br><i>No corpora available for this URL list.<br><br></i>\n";
        else
            echo "<br><i>No corpora available for this website.<br><br></i>\n";
            
        $result->close();
    }
}
    
?>