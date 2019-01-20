<?php

require "../config.php";

session_start();

if(isset($_POST["type"]) && isset($_POST["filename"])) {
    $data = "";
    $type = $_POST["type"];
    
    if($type == "urllist") {
        // get website and URL list namespaces
        if(!isset($_POST["website"]) || !isset($_POST["namespace"])) die("Download error.");
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        
        // get URLs
        $result = $dbConnection->query("SELECT url FROM `crawlserv_".$website."_".$namespace."`");
        if(!$result) die("Download error.");
        while($row = $result->fetch_assoc()) {
            $data .= $row["url"]."\n";
        }
        $result->close();
    }
    else if($type == "content") {
        // get website and URL list namespaces + content version to download
        if(!isset($_POST["website"]) || !isset($_POST["namespace"]) || !isset($_POST["version"])) die("Download error.");
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        $version = $_POST["version"];
        
        // get content
        $result = $dbConnection->query("SELECT content FROM `crawlserv_".$website."_".$namespace."_crawled` WHERE id=$version LIMIT 1");
        if(!$result) die("Download error.");
        $row = $result->fetch_assoc();
        if(!$row) die("Download error.");
        $data = $row["content"];
        $result->close();
    }
    else die("Download error.");
    
    $_SESSION["crawlserv_data"] = $data;
    $_SESSION["crawlserv_filename"] = $_POST["filename"];
    Header("Content-Type: application/json;charset=UTF-8");
    die(json_encode(array('status' => 'OK')));
}

if(isset($_SESSION["crawlserv_data"]) && isset($_SESSION["crawlserv_filename"])) {
    header('Content-Type: text/plain; charset=utf-8');
    header("Content-Disposition: attachment; filename=".$_SESSION["crawlserv_filename"]);
    echo $_SESSION["crawlserv_data"];
    unset($_SESSION["crawlserv_data"]);
    unset($_SESSION["crawlserv_filename"]);
}
else die("Download error.");

?>