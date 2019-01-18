<?php
// DEBUG
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);
// END DEBUG

// get navigation
if(isset($_POST["m"])) $m = $_POST["m"];
else $m = "server";
if($m != "server") require "config.php";

header('Content-Type: text/html; charset=utf-8');
?>
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>crawlserv++ frontend</title>
<link rel="stylesheet" type="text/css" href="css/style.css">
<script src="js/external/array.equals.js"></script>
<script src="js/external/jquery-3.3.1.min.js"></script>
<script src="js/external/jquery.redirect.js"></script>
<script src="js/external/tippy.all.min.js"></script>
<script src="js/style.js"></script>
<script src="js/frontend.js"></script>
<script src="js/algo.js"></script>
<script src="js/config.js"></script>
<script>
var redirected = <?php echo isset($_POST["redirected"]) ? "true" : "false";?>;
</script>
</head>
<body>
<div id="container">
<div id="main">
<h1>crawlserv++ frontend</h1>
<div id="navigation">
<div id="menu">
<div class="menu-row"><div class="menu-item <?php if($m == "server") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="server">Server</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "websites") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="websites">Websites</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "queries") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="queries">Queries</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "crawlers") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="crawlers">Crawlers</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "parsers") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="parsers">Parsers</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "extractors") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="extractors">Extractors</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "analyzers") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="analyzers">Analyzers</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "threads") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="threads">Threads</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "search") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="search">Search</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "content") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="content">Content</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "data") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="data">Import/Export</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "statistics") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="statistics">Statistics</a></div></div>
<div class="menu-row"><div class="menu-item <?php if($m == "logs") echo "menu-item-current"; ?>"><a href="" class="post-redirect menu-item-link" data-m="logs">Logs</a></div></div>
</div>
</div>
<div id="content">
<?php require "php/$m.php"; ?>
</div>
</div>
</div>
<div id="redirect-time"></div>
</body>
</html>
