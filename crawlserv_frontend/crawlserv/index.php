
<!-- 

   ***
   
    Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version in addition to the terms of any
    licences already herein identified.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

// </DEBUG>

// get navigation
if(isset($_POST["m"]))
    $m = $_POST["m"];
else
    $m = "server";

// get optional attribute
if(isset($_POST["mode"]))
    $mode_attribute = ' data-mode="'.$_POST["mode"].'"';
else
    $mode_attribute = "";

// send header with content type and character encoding
header('Content-Type: text/html; charset=utf-8');

?>

<!doctype html>

<html lang="en">

<head>

<meta http-equiv="Content-Type" content="text/html; charset=utf-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>crawlserv++ frontend</title>

<link rel="stylesheet" type="text/css" href="css/external/prism.css">
<link rel="stylesheet" type="text/css" href="css/external/table.css">

<link rel="stylesheet" type="text/css" href="css/style.css">

<script src="js/external/array.equals.js"></script>
<script src="js/external/jquery-3.3.1.min.js"></script>
<script src="js/external/jquery.redirect.js"></script>
<script src="js/external/jquery.form.min.js"></script>
<script src="js/external/tippy.all.min.js"></script>
<script src="js/external/prism.js"></script>

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

if($m == "server")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="server"<?php

echo $mode_attribute;

?>>Server</a></div></div>

<!-- Websites -->

<div class="menu-row"><div class="menu-item <?php

if($m == "websites")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="websites"<?php

echo $mode_attribute;

?>>Websites</a></div></div>

<!-- Queries -->

<div class="menu-row"><div class="menu-item <?php

if($m == "queries")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="queries"<?php

echo $mode_attribute;

?>>Queries</a></div></div>

<!-- Crawlers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "crawlers")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="crawlers"<?php

echo $mode_attribute;

?>>Crawlers</a></div></div>

<!-- Parsers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "parsers")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="parsers"<?php

echo $mode_attribute;

?>>Parsers</a></div></div>

<!-- Extractors -->

<div class="menu-row"><div class="menu-item <?php

if($m == "extractors")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="extractors"<?php

echo $mode_attribute;

?>>Extractors</a></div></div>

<!-- Analyzers -->

<div class="menu-row"><div class="menu-item <?php

if($m == "analyzers")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="analyzers"<?php

echo $mode_attribute;

?>>Analyzers</a></div></div>

<!-- Threads -->

<div class="menu-row"><div class="menu-item <?php

if($m == "threads")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="threads"<?php

echo $mode_attribute;

?>>Threads</a></div></div>

<!-- Search -->

<div class="menu-row"><div class="menu-item <?php

if($m == "search")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="search"<?php

echo $mode_attribute;

?>>Search</a></div></div>

<!-- Content -->

<div class="menu-row"><div class="menu-item <?php

if($m == "content")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="content"<?php

echo $mode_attribute;

?>>Content</a></div></div>

<!-- Import/Export [Data] -->

<div class="menu-row"><div class="menu-item <?php

if($m == "data")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="data"<?php

echo $mode_attribute;

?>>Import/Export</a></div></div>

<!-- Statistics -->

<div class="menu-row"><div class="menu-item <?php

if($m == "statistics")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="statistics"<?php

echo $mode_attribute;

?>>Statistics</a></div></div>

<!-- Logs -->

<div class="menu-row"><div class="menu-item <?php

if($m == "logs")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="logs"<?php

echo $mode_attribute;

?>>Logs</a></div></div>

<!-- About -->

<div class="menu-row"><div class="menu-item <?php

if($m == "about")
    echo "menu-item-current";

?>"><a href="" class="post-redirect menu-item-link" data-m="about"<?php

echo $mode_attribute;

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
