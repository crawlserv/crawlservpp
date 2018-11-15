<?php
$db_host = "localhost";
$db_user = "frontend";
$db_password = "K*^Qe,qzu=5Vn+gj";
$db_name = "crawled";

$dbConnection = new mysqli($db_host, $db_user, $db_password, $db_name);
if($dbConnection->connect_error) {
    die("Connection to databank failed: " . $dbConnection->connect_error);
}
$dbConnection->set_charset("utf8mb4");
?>
