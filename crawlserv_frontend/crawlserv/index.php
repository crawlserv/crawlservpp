
<!-- 

   ***
   
    Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version in addition to the terms of any
    licences already herein identified.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
    
   ***
  
   index.php

   PHP/JavaScript frontend for crawlserv++.
    
   See https://github.com/crawlserv/crawlservpp/ for the latest version of the application.

 -->

<?php

// <DEBUG>

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);

error_reporting(E_ALL);

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

// </DEBUG>

// get navigation
if(isset($_POST["m"])) {
    $m = $_POST["m"];
}
else if(isset($_GET["m"])) {
    $m = $_GET["m"];
}
else {
    $m = "server";
}

// get optional attributes
$opt_attributes = "";

if(isset($_POST["website"])) {
    $opt_attributes .= ' data-website="'.$_POST["website"].'"';
}

if(isset($_POST["mode"])) {
    $opt_attributes .= ' data-mode="'.$_POST["mode"].'"';
}

// send header with content type and character encoding
header('Content-Type: text/html; charset=utf-8');

?>

<!doctype html>

<html lang="en">

<head>

<meta http-equiv="Content-Type" content="text/html; charset=utf-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>crawlserv++ frontend</title>

<link rel="stylesheet" type="text/css" href="css/external/jquery-ui.min.css">
<link rel="stylesheet" type="text/css" href="css/external/prism.css">
<link rel="stylesheet" type="text/css" href="css/external/table.css">

<link rel="stylesheet" type="text/css" href="css/style.css">

<script src="js/external/array.equals.js"></script>
<script src="js/external/jquery-3.3.1.min.js"></script>
<script src="js/external/jquery.redirect.js"></script>
<script src="js/external/jquery.form.min.js"></script>
<script src="js/external/jquery-ui.min.js"></script>
<script src="js/external/prism.js"></script>
<script src="js/external/tippy.all.min.js"></script>

<script src="js/style.js"></script>
<script src="js/helpers.js"></script>
<script src="js/frontend.js"></script>
<script src="js/algo.js"></script>
<script src="js/config.js"></script>

<script>

var redirected = <?php

echo isset($_POST["redirected"]) ? "true" : "false";

?>;

var scrolldown = <?php

echo isset($_POST["scrolldown"]) ? "true" : "false";

?>;

window.history.pushState("object or string", "Title", location.protocol + '//' + location.host + location.pathname);

</script>

</head>

<body>

<div id="hidden-div" style="visibility:hidden"></div>

<div id="container">
<div id="main">

<h1>crawlserv++ frontend</h1>

<div id="navigation">
<div id="menu">

<!-- Server -->

<div class="menu-row"><div class="menu-item <?php

if($m == "server") {
    echo "menu-item-current";
}

?>"><a href="?m=server" class="post-redirect menu-item-link" data-m="server"<?php

echo $opt_attributes;

?>>Server</a></div></div>

<!-- Websites -->

<div class="menu-row"><div class="menu-item <?php

if($m == "websites") {
    echo "menu-item-current";
}

?>"><a href="?m=websites" class="post-redirect menu-item-link" data-m="websites"<?php

echo $opt_attributes;

?>>Websites</a></div></div>

<!-- Queries -->

<div class="menu-row"><div class="menu-item <?php

if($m == "queries") {
    echo "menu-item-current";
}

?>"><a href="?m=queries" class="post-redirect menu-item-link" data-m="queries"<?php

echo $opt_attributes;

?>>Queries</a></div></div>

<!-- Crawlers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "crawlers") {
    echo "menu-item-current";
}

?>"><a href="?m=crawlers" class="post-redirect menu-item-link" data-m="crawlers"<?php

echo $opt_attributes;

?>>Crawlers</a></div></div>

<!-- Parsers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "parsers") {
    echo "menu-item-current";
}

?>"><a href="?m=parsers" class="post-redirect menu-item-link" data-m="parsers"<?php

echo $opt_attributes;

?>>Parsers</a></div></div>

<!-- Extractors -->

<div class="menu-row"><div class="menu-item <?php

if($m == "extractors") {
    echo "menu-item-current";
}

?>"><a href="?m=extractors" class="post-redirect menu-item-link" data-m="extractors"<?php

echo $opt_attributes;

?>>Extractors</a></div></div>

<!-- Analyzers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "analyzers") {
    echo "menu-item-current";
}

?>"><a href="?m=analyzers" class="post-redirect menu-item-link" data-m="analyzers"<?php

echo $opt_attributes;

?>>Analyzers</a></div></div>

<!-- Threads -->

<div class="menu-row"><div class="menu-item <?php

if($m == "threads") {
    echo "menu-item-current";
}

?>"><a href="?m=threads" class="post-redirect menu-item-link" data-m="threads"<?php

echo $opt_attributes;

?>>Threads</a></div></div>

<!-- Search -->

<div class="menu-row"><div class="menu-item <?php

if($m == "search") {
    echo "menu-item-current";
}

?>"><a href="?m=search" class="post-redirect menu-item-link" data-m="search"<?php

echo $opt_attributes;

?>>Search</a></div></div>

<!-- Content -->

<div class="menu-row"><div class="menu-item <?php

if($m == "content") {
    echo "menu-item-current";
}

?>"><a href="?m=content" class="post-redirect menu-item-link" data-m="content"<?php

echo $opt_attributes;

?>>Content</a></div></div>

<!-- Import/Export [Data] -->

<div class="menu-row"><div class="menu-item <?php

if($m == "data") {
    echo "menu-item-current";
}

?>"><a href="?m=data" class="post-redirect menu-item-link" data-m="data"<?php

echo $opt_attributes;

?>>Import/Export</a></div></div>

<!-- Statistics -->

<div class="menu-row"><div class="menu-item <?php

if($m == "statistics") {
    echo "menu-item-current";
}

?>"><a href="?m=statistics" class="post-redirect menu-item-link" data-m="statistics"<?php

echo $opt_attributes;

?>>Statistics</a></div></div>

<!-- Logs -->

<div class="menu-row"><div class="menu-item <?php

if($m == "logs") {
    echo "menu-item-current";
}

?>"><a href="?m=logs" class="post-redirect menu-item-link" data-m="logs"<?php

echo $opt_attributes;

?>>Logs</a></div></div>

<!-- About -->

<div class="menu-row"><div class="menu-item <?php

if($m == "about") {
    echo "menu-item-current";
}

?>"><a href="?m=about" class="post-redirect menu-item-link" data-m="about"<?php

echo $opt_attributes;

?>>About</a></div></div>

</div>
</div>

<div id="content">

<?php

require "php/$m.php";

?>

</div>

</div>
</div>

<div id="redirect-time"></div>

</body>

</html>
