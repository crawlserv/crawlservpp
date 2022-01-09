<?php

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
 * download/index.php
 * 
 * Download content from the database as file.
 * 
 */

// function to check for JSON code
function isJSON($str) {
    if($str[0] == '{' || $str[0] == "[") {
        json_decode($str);
        
        return json_last_error() == JSON_ERROR_NONE;
    }
    
    return false;
}

// function to get content type from file name
function getContentType($filename) {
    $ending =  substr($filename, -4);
    
    switch($ending) {
        case ".css":
            return "text/css";
            
        case ".ods":
            return "application/vnd.oasis.opendocument.spreadsheet";
            
        case ".odt":
            return "application/vnd.oasis.opendocument.text";
            
        default:
            /* assume plain text by default */
            return "text/plain";
    }
}

// <DEBUG>

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);

error_reporting(E_ALL);

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

// </DEBUG>

// initialize database
$db_init = true;

require "../config.php";

// set memory limit to 1 GiB
ini_set('memory_limit', '1G');

// send headers
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");

session_start();

if(isset($_POST["type"]) && isset($_POST["filename"])) {
    $data = "";
    $type = $_POST["type"];
    
    if($type == "urllist") {
        // get website and URL list namespaces
        if(!isset($_POST["website"])) {
            die("Download error: No website specified.");
        }
        
        if(!isset($_POST["namespace"])) {
            die("Download error: No namespace specified.");
        }
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        
        // get URLs
        $result = $dbConnection->query(
                "SELECT url".
                " FROM `crawlserv_".$website."_".$namespace."`"
        );
        
        if(!$result) {
            die("Download error: Could not retrieve URL.");
        }
        
        while($row = $result->fetch_assoc()) {
            $data .= $row["url"]."\n";
        }
        
        $result->close();
    }
    else if($type == "content") {
        // get website and URL list namespaces + content version to download
        if(!isset($_POST["website"])) {
            die("Download error: No website specified.");
        }
        
        if(!isset($_POST["namespace"])) {
            die("Download error: No namespace specified.");
        }
        
        if(!isset($_POST["version"])) {
            die("Download error: No version specified.");
        }
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        $version = $_POST["version"];
        
        // get content
        $result = $dbConnection->query(
            "SELECT content FROM `crawlserv_".$website."_".$namespace."_crawled`".
            " WHERE id=$version".
            " LIMIT 1"
        );
        
        if(!$result) {
            die("Download error: Could not get content from database.");
        }
        
        $row = $result->fetch_assoc();
        
        if(!$row) {
            die("Download error: Could not get content from database (empty response).");
        }
        
        $data = $row["content"];
        
        $result->close();
    }
    else if($type == "parsed") {
        // get website and URL list namespaces, parsing table to download from, sub-URL and website name
        if(!isset($_POST["website"])) {
            die("Download error: No website specified.");
        }
        
        if(!isset($_POST["namespace"])) {
            die("Download error: No namespace specified.");
        }
        
        if(!isset($_POST["version"])) {
            die("Download error: No version specified.");
        }
        
        if(!isset($_POST["w_id"])) {
            die("Download error: No website ID specified.");
        }
        
        if(!isset($_POST["w_name"])) {
            die("Download error: No website name specified.");
        }
        
        if(!isset($_POST["u_id"])) {
            die("Download error: No URL ID specified.");
        }
        
        if(!isset($_POST["url"])) {
            die("Download error: No URL specified.");
        }            
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        $version = $_POST["version"];
        $w_id = $_POST["w_id"];
        $w_name = $_POST["w_name"];
        $u_id = $_POST["u_id"];
        $url = $_POST["url"];
        
        // get website domain
        $result = $dbConnection->query(
                "SELECT domain".
                " FROM crawlserv_websites".
                " WHERE id=$w_id".
                " LIMIT 1"
        );
        
        if(!$result) {
            die("Download error: Could not get domain of website from database.");
        }
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if(!$row) {
            die("Download error: Could not get domain of website from database (empty response).");
        }
        
        $domain = $row["domain"];
        
        // get parsing table
        $result = $dbConnection->query("SELECT name FROM crawlserv_parsedtables WHERE id=$version LIMIT 1");
        
        if(!$result) {
            die("Download error: Could not get parsing table from database.");
        }
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if(!$row) {
            die("Download error: Could not get parsing table from database (empty response.");
        }
        
        // create table names
        $ctable = "crawlserv_".$website."_".$namespace."_crawled";
        $ptable = "crawlserv_".$website."_".$namespace."_parsed_".$row["name"];
        
        // get column names
        $result = $dbConnection->query(
                "SELECT COLUMN_NAME AS name".
                " FROM INFORMATION_SCHEMA.COLUMNS".
                " WHERE TABLE_SCHEMA = 'crawled'".
                " AND TABLE_NAME = N'$ptable'"
        );
        
        if(!$result) {
            die("Download error: Could not get columns of parsing table from database.");
        }
        
        $columns = array();
        
        while($row = $result->fetch_assoc()) {
            if(strlen($row["name"]) > 7 && substr($row["name"], 0, 7) == "parsed_") {
                $columns[] = $row["name"];
            }
        }
            
        $result->close();
        
        // get parsed data
        if(count($columns) == 0) {
            die("Download error: No data columns in parsing table.");
        }
        
        $query = "SELECT ";
        
        foreach($columns as $column) {
            $query .= "b.`".$column."`, ";
        }

        $query = substr($query, 0, -2);
        
        $query .= " FROM `$ctable` AS a, `$ptable` AS b".
                  " WHERE a.id = b.content".
                  " AND a.url = $u_id".
                  " ORDER BY b.id DESC".
                  " LIMIT 1";
        
        $result = $dbConnection->query($query);
        
        if(!$result) {
            die("Download error: ".$query);
        }
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        // start JSON file
        $data = "{\n";
        $data .= " \"website\": {\n";
        $data .= "  \"name\": ".json_encode($w_name, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE).",\n";
        $data .= "  \"domain\": ".json_encode($domain, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE)."\n";
        $data .= " },\n";
        $data .= " \"page\": {\n";
        $data .= "  \"url\": ".json_encode($url, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE).",\n";
        $data .= "  \"parsed-data\": {";
        
        // go through all rows
        if($row) {
            foreach($columns as $column) {
                $data .= "\n   ";
                
                if(strlen($column) > 8 && substr($column, 0, 8) == "parsed__") {
                    $data .= json_encode(
                            substr($column, 8),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).": ";
                }
                else {
                    $data .= json_encode(
                            substr($column, 7),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).": ";
                }
                
                if(!strlen(trim($row[$column]))) {
                    $data.= "{},";
                }
                else if(isJSON($row[$column])) {
                    $data .= $row[$column].",";
                }
                else {
                    $data .= json_encode(
                            $row[$column],
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).",";
                }
            }
            
            // remove last comma
            $data = substr($data, 0, -1);
        }
        
        // conclude JSON file
        $data .= "\n  }\n";
        $data .= " }\n";
        $data .= "}\n";
    }
    else if($type == "extracted") {
        // get website and URL list namespaces, extracting table to download from, sub-URL and website name
        if(!isset($_POST["website"])) {
            die("Download error: No website specified.");
        }
        
        if(!isset($_POST["namespace"])) {
            die("Download error: No namespace specified.");
        }
        
        if(!isset($_POST["version"])) {
            die("Download error: No version specified.");
        }
        
        if(!isset($_POST["w_id"])) {
            die("Download error: No website ID specified.");
        }
        
        if(!isset($_POST["w_name"])) {
            die("Download error: No website name specified.");
        }
        
        if(!isset($_POST["u_id"])) {
            die("Download error: No URL ID specified.");
        }
        
        if(!isset($_POST["url"])) {
            die("Download error: No URL specified.");
        }
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        $version = $_POST["version"];
        $w_id = $_POST["w_id"];
        $w_name = $_POST["w_name"];
        $u_id = $_POST["u_id"];
        $url = $_POST["url"];
        
        // get website domain
        $result = $dbConnection->query(
            "SELECT domain".
            " FROM crawlserv_websites".
            " WHERE id=$w_id".
            " LIMIT 1"
            );
        
        if(!$result) {
            die("Download error: Could not get domain of website from database.");
        }
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if(!$row) {
            die("Download error: Could not get domain of website from database (empty response).");
        }
        
        $domain = $row["domain"];
        
        // get extracting table
        $result = $dbConnection->query("SELECT name FROM crawlserv_extractedtables WHERE id=$version LIMIT 1");
        
        if(!$result) {
            die("Download error: Could not get extracting table from database.");
        }
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        // create table names
        $ctable = "crawlserv_".$website."_".$namespace."_crawled";
        $etable = "crawlserv_".$website."_".$namespace."_extracted_".$row["name"];
        
        // get column names
        $result = $dbConnection->query(
            "SELECT COLUMN_NAME AS name".
            " FROM INFORMATION_SCHEMA.COLUMNS".
            " WHERE TABLE_SCHEMA = 'crawled'".
            " AND TABLE_NAME = N'$etable'"
            );
        
        if(!$result) {
            die("Download error: Could not get columns of extracting table from database.");
        }
        
        $columns = array();
        $linked = true;
        $haslink = false;
        
        while($row = $result->fetch_assoc()) {
            if(strlen($row["name"]) > 10 && substr($row["name"], 0, 10) == "extracted_") {
                $columns[] = $row["name"];
            }
            else if($row["name"] == "content") {
                $linked = false;
            }
            else if($row["name"] == "link") {
                $haslink = true;
            }
        }
        
        $result->close();
        
        if($linked) {
            die("Download error: No URL-specific data found – cannot download linked data on its own.");
        }
        
        // get reference to linked data
        if($haslink) {
            $rtable = "";
            
            $result = $dbConnection->query("SHOW CREATE TABLE `$etable`");
            
            if(!$result) {
                die("Download error: Could not get references of extracting table from database.");
            }
            
            $row = $result->fetch_array();
            
            $result->close();
            
            if(!$row) {
                die("Download error: Could not get references of extracting table from database (empty response).");
            }
            
            $tableInfo = $row[1];
            
            $len = strlen($tableInfo);
            $pos = 0;
            
            while($pos < $len) {
                $pos = strpos($tableInfo, "FOREIGN KEY (`", $pos);
                
                if($pos === FALSE) {
                    break;
                }
                
                $pos += 14;
                
                $end = strpos($tableInfo, "`)", $pos);
                
                if($end === FALSE) {
                    die("Download error: Unexpected end of table information when getting constraints.");
                }
                
                if(substr($tableInfo, $pos, $end - $pos) != "link") {
                    continue;
                }
                
                $pos = $end + 2;
                
                break;
            }
            
            if($pos !== FALSE && $pos < $len) {
                if(substr($tableInfo, $pos, 13) != " REFERENCES `") {
                    die("Download error: Could not identify referenced table when getting constraints.");
                }
                
                $pos += 13;
                
                $end = strpos($tableInfo, "`", $pos);
                
                if($end === FALSE) {
                    die("Download error: Unexpected end of table information when getting referenced table.");
                }
                
                $rtable = substr($tableInfo, $pos, $end - $pos);
                
                $haslink = strlen($rtable) > 0;
            }
        }
        
        // get extracted data
        if(count($columns) == 0) {
            die("Download error: No data columns in extracting table.");
        }
        
        $query = "SELECT ";
        
        foreach($columns as $column) {
            $query .= "b.`".$column."`, ";
        }
        
        if($haslink) {
            $query .= "b.link";
        }
        else {
            // remove last comma
            $query = substr($query, 0, -2);
        }
        
        $query .= " FROM `$ctable` AS a, `$etable` AS b".
            " WHERE a.id = b.content".
            " AND a.url = $u_id".
            " ORDER BY b.id";
        
        $result = $dbConnection->query($query);
        
        if(!$result) {
            die("Download error: ".$query);
        }
        
        // start JSON file
        $data = "{\n";
        $data .= " \"website\": {\n";
        $data .= "  \"name\": ".json_encode($w_name, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE).",\n";
        $data .= "  \"domain\": ".json_encode($domain, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE)."\n";
        $data .= " },\n";
        $data .= " \"page\": {\n";
        $data .= "  \"url\": ".json_encode($url, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE).",\n";
        $data .= "  \"extracted-data\": [";
        
        // go through all rows
        if($result->num_rows > 0) {
            while($row = $result->fetch_assoc()) {
                $data .= "\n   {";
                
                foreach($columns as $column) {
                    $data .= "\n    ";
                    
                    if(strlen($column) > 11 && substr($column, 0, 11) == "extracted__") {
                        $data .= json_encode(
                            substr($column, 11),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                        ).": ";
                    }
                    else {
                        $data .= json_encode(
                            substr($column, 10),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                        ).": ";
                    }
                    
                    if(!strlen(trim($row[$column]))) {
                        $data.= "{},";
                    }
                    else if(isJSON($row[$column])) {
                        $data .= $row[$column].",";
                    }
                    else {
                        $data .= json_encode(
                            $row[$column],
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                            ).",";
                    }
                }
                
                if($haslink) {
                    $data .= "\n    \"linked-data\": {";
                    
                    $result2 = $dbConnection->query("SELECT * FROM `$rtable` WHERE id=".$row["link"]);
                    
                    if(!$result2) {
                        die("Download error: Could not get linked data from database.");
                    }
                    
                    $row2 = $result2->fetch_assoc();
                    
                    if($row2) {
                        $founddata = false;
                        
                        foreach($row2 as $key => $value) {
                            if(
                                strlen($key) > 10
                                && substr($key, 0, 10) == "extracted_"
                            ) {
                                $data .= "\n     ";
                                
                                if(
                                    strlen($key) > 11
                                    && $key[10] == "_"
                                ) {
                                    $cname = substr($key, 11);
                                }
                                else {
                                    $cname = substr($key, 10);
                                }
                                
                                $data .= json_encode(
                                    $cname,
                                    JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                                ).": ";
                                
                                if(!strlen(trim($value))) {
                                    $data.= "{},";
                                }
                                else if(isJSON($value)) {
                                    $data .= $value.",";
                                }
                                else {
                                    $data .= json_encode(
                                        $value,
                                        JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                                        ).",";
                                }
                                
                                $founddata = true;
                            }
                        }
                        
                        if($founddata) {
                            // remove last comma
                            $data = substr($data, 0, -1);
                            
                            $data .= "\n    ";
                        }
                    }
                    
                    $data .= "}";
                }
                else {
                    // remove last comma
                    $data = substr($data, 0, -1);
                }
                
                $data .= "\n   },";
            }
            
            // remove last comma
            $data = substr($data, 0, -1);
        }
        
        // conclude JSON file
        $data .= "\n  ]\n";
        $data .= " }\n";
        $data .= "}\n";
    }
    else {
        die("Download error: '$type' is an invalid data source.");
    }
    
    $_SESSION["crawlserv_data"] = $data;
    $_SESSION["crawlserv_filename"] = $_POST["filename"];
    
    header("Content-Type: application/json;charset=UTF-8");
    
    die(json_encode(array('status' => 'OK')));
}

if(isset($_SESSION["crawlserv_data"]) && isset($_SESSION["crawlserv_filename"])) {
    header('Content-Type: '.getContentType($_SESSION["crawlserv_filename"]).'; charset=utf-8');    
    header('Content-Disposition: attachment; filename="'.$_SESSION["crawlserv_filename"].'"');
    
    echo $_SESSION["crawlserv_data"];
    
    unset($_SESSION["crawlserv_data"]);
    unset($_SESSION["crawlserv_filename"]);
}
else {
    die("Download error.");
}

?>