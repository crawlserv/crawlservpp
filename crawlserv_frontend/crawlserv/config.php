<?php

/*
 * config.php
 * 
 * Configuration file for the crawlserv++ frontent
 *  allowing read-only database access (and access to the command-and-control server).
 * 
 */

$cc_host = "http://localhost:8080"; // URL to the host of the command-and-control server

$db_host = "localhost";             // host of the database
$db_user = "frontend";              // user name (for security reasons, this user should only have SELECT privileges!)
$db_password = "K*^Qe,qzu=5Vn+gj";  // user password (randomly created test password, to be replaced by your own!)
$db_name = "crawled";               // database schema

if(isset($db_init) && $db_init) {
    // connect to database
    $dbConnection = new mysqli($db_host, $db_user, $db_password, $db_name);
    
    // check for connection error
    if($dbConnection->connect_error)
        die("Connection to databank failed: " . $dbConnection->connect_error);
    
    // set charset to UTF-8
    $dbConnection->set_charset("utf8mb4");
}

if(isset($cc_init) && $cc_init) 
    // tranfer command-and-control server settings to JavaScript
    echo "<script>\n\nvar cc_host = \"$cc_host\";\n\n</script>\n";

?>
