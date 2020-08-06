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
 * config.php
 * 
 * Configuration file for the PHP/JavaScript crawlserv++ frontend
 *  allowing read-only database access (and access to the command-and-control server).
 * 
 */

$cc_host = "http://".$_SERVER['SERVER_NAME'].":8080"; // URL to the host of the command-and-control server

$db_host = "localhost";             // host of the database
$db_user = "frontend";              // user name (for security reasons, this user should only have SELECT privileges!)
$db_password = "K*^Qe,qzu=5Vn+gj";  // user password (randomly created test password, to be replaced by your own!)
$db_name = "crawled";               // database schema

if(isset($db_init) && $db_init) {
    // connect to database
    try {
        $dbConnection = new mysqli($db_host, $db_user, $db_password, $db_name);
    }
    catch(Exception $e) {
        die("<br>Connection to databank failed: " . $e->getMessage());
    }
    
    // check for connection error
    if($dbConnection->connect_error) {
        die("<br>Connection to databank failed: " . $dbConnection->connect_error);
    }
    
    // set charset to UTF-8
    $dbConnection->set_charset("utf8mb4");
}

if(isset($cc_init) && $cc_init) {
    // tranfer command-and-control server settings to JavaScript
    echo "<script>\n\nvar cc_host = \"$cc_host\";\n\n</script>\n";
}

?>