<?php

/*
 *  
 * ---
 * 
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
        if(!isset($_POST["website"]) || !isset($_POST["namespace"]))
            die("Download error.");
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        
        // get URLs
        $result = $dbConnection->query(
                "SELECT url".
                " FROM `crawlserv_".$website."_".$namespace."`"
        );
        
        if(!$result)
            die("Download error.");
        
        while($row = $result->fetch_assoc())
            $data .= $row["url"]."\n";
        
        $result->close();
    }
    else if($type == "content") {
        // get website and URL list namespaces + content version to download
        if(!isset($_POST["website"]) || !isset($_POST["namespace"]) || !isset($_POST["version"]))
            die("Download error.");
        
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        $version = $_POST["version"];
        
        // get content
        $result = $dbConnection->query(
            "SELECT content FROM `crawlserv_".$website."_".$namespace."_crawled`".
            " WHERE id=$version".
            " LIMIT 1"
        );
        
        if(!$result)
            die("Download error.");
        
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("Download error.");
        
        $data = $row["content"];
        
        $result->close();
    }
    else if($type == "parsed") {
        // get website and URL list namespaces, parsing table to download from, sub-URL and website name
        if(
            !isset($_POST["website"])
            || !isset($_POST["namespace"])
            || !isset($_POST["version"])
            || !isset($_POST["w_id"])
            || !isset($_POST["w_name"])
            || !isset($_POST["u_id"])
            || !isset($_POST["url"])
        )
            die("Download error.");
        
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
        
        if(!$result)
            die("Download error.");
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if(!$row)
            die("Download error.");
        
        $domain = $row["domain"];
        
        // get parsing table
        $result = $dbConnection->query("SELECT name FROM crawlserv_parsedtables WHERE id=$version LIMIT 1");
        
        if(!$result)
            die("Download error.");
        
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if(!$row)
            die("Download error.");
        
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
        
        if(!$result)
            die("Download error.");
        
        $columns = array();
        
        while($row = $result->fetch_assoc())
            if(strlen($row["name"]) > 7 && substr($row["name"], 0, 7) == "parsed_")
                $columns[] = $row["name"];
            
        $result->close();
        
        // get parsed data
        if(!count($columns))
            die("Download error.");
        
        $query = "SELECT ";
        
        foreach($columns as $column)
            $query .= "b.`".$column."`, ";

        $query = substr($query, 0, -2);
        
        $query .= " FROM `$ctable` AS a, `$ptable` AS b".
                  " WHERE a.id = b.content".
                  " AND a.url = '$u_id'".
                  " ORDER BY b.id DESC".
                  " LIMIT 1";
        
        $result = $dbConnection->query($query);
        
        if(!$result)
            die("Download error: ".$query);
        
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
                
                if(strlen($column) > 8 && substr($column, 0, 8) == "parsed__")
                    $data .= json_encode(
                            substr($column, 8),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).": ";
                else
                    $data .= json_encode(
                            substr($column, 7),
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).": ";
                
                if(!strlen(trim($row[$column])))
                    $data.= "{},";
                else if(isJSON($row[$column]))
                    $data .= $row[$column].",";
                else
                    $data .= json_encode(
                            $row[$column],
                            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
                    ).",";
            }
            
            $data = substr($data, 0, -1);
        }
        
        // conclude JSON file
        $data .= "\n  }\n";
        $data .= " }\n";
        $data .= "}\n";
    }
    else
        die("Download error.");
    
    $_SESSION["crawlserv_data"] = $data;
    $_SESSION["crawlserv_filename"] = $_POST["filename"];
    
    header("Content-Type: application/json;charset=UTF-8");
    
    die(json_encode(array('status' => 'OK')));
}

if(isset($_SESSION["crawlserv_data"]) && isset($_SESSION["crawlserv_filename"])) {
    header('Content-Type: text/plain; charset=utf-8');
    header("Content-Disposition: attachment; filename=".$_SESSION["crawlserv_filename"]);
    
    echo $_SESSION["crawlserv_data"];
    
    unset($_SESSION["crawlserv_data"]);
    unset($_SESSION["crawlserv_filename"]);
}
else
    die("Download error.");

?>