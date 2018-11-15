<?php
/*
 * config.php
 * 
 * Configuration file for the crawlserv++ frontent allowing read-only database access.
 * 
 */

$db_host = "localhost";             // database host
$db_user = "frontend";              // user name (for security reasons, this user should only have SELECT privileges!)
$db_password = "K*^Qe,qzu=5Vn+gj";  // user password (randomly created test password, to be replaced by your own!)
$db_name = "crawled";               // database scheme

// connect to database
$dbConnection = new mysqli($db_host, $db_user, $db_password, $db_name);

// check for connection error
if($dbConnection->connect_error) {
    die("Connection to databank failed: " . $dbConnection->connect_error);
}

// set charset to UTF-8
$dbConnection->set_charset("utf8mb4");

?>
