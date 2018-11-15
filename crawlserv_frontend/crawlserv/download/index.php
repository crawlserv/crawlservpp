<?php

require "../config.php";

session_start();

if(isset($_POST["type"]) && isset($_POST["filename"])) {
    $data = "";
    $type = $_POST["type"];
    
    if($type == "urllist") {
        // get website and URL list namespace
        if(!isset($_POST["website"]) || !isset($_POST["namespace"])) die("Download error.");
        $website = $_POST["website"];
        $namespace = $_POST["namespace"];
        
        // get URLs
        $result = $dbConnection->query("SELECT url FROM crawlserv_".$website."_".$namespace);
        if(!$result) die("Download error.");
        while($row = $result->fetch_assoc()) {
            $data .= $row["url"]."\n";
        }
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