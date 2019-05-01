<?php

// render row with website selection
function rowWebsiteSelect($adddelete = false, $showglobal = false, $scrolldown = false) {
    global $m, $mode, $tab, $dbConnection, $website, $websiteName, $namespace, $domain, $dir;
    
    if($adddelete)
        $class = "entry-x-input";
    else
        $class = "entry-input";
    
    $html = "<div class=\"entry-row\">\n";
    
    $html .= "<div class=\"entry-label\">Website:</div>";
    $html .= "<div class=\"$class\">";
    
    $html .= "<select class=\"$class\" id=\"website-select\"";
    
    if(isset($m))
        $html .= " data-m=\"$m\"";
    
    if(isset($mode))
        $html .= " data-mode=\"$mode\"";
    
    if(isset($tab))
        $html .= " data-tab=\"$tab\"";
    
    if($scrolldown)
        $html .= " data-scrolldown";
    
    $html .= ">\n";
    
    if($showglobal) {
        $html .= "<option value=\"0\"";
    
        if(!$website)
            $html .= " selected";
        
        $html .= ">All websites [global]</option>\n";
    }

    $result = $dbConnection->query(
            "SELECT id, name, namespace, domain, dir".
            " FROM crawlserv_websites".
            " ORDER BY name"
    );

    if(!$result)
        die("ERROR: Could not websites from database.");
    
    $first = true;
    
    while($row = $result->fetch_assoc()) {
        $id = $row["id"];
        $name = $row["name"];
        
        if($first) {
            if(!isset($website))
                $website = $id;
                
            $first = false;
        }
        
        $html .= "<option value=\"".$id."\"";
        
        if($website == $id) {
            $html .= " selected";
            
            $websiteName = $name;
            $namespace = $row["namespace"];
            $domain = $row["domain"];
            $dir = $row["dir"];
        }
            
        $html .= ">".htmlspecialchars($name)."</option>\n";
    }
    
    $result->close();
    
    if($adddelete) {
        $html .= "option value=\"0\"";
        
        if(!$website)
            $html .= " selected";
        
        $html .= ">Add website</option>\n";
    }
    else if(!isset($website))
        $html .= "<option disabled>No website available</option>\n";
    
    $html .= "</select>\n";
    
    if($adddelete)
        $html .= "<a href=\"#\" class=\"actionlink website-delete\"><span class=\"remove-entry\">X</span></a>";
    
    $html .= "</div>\n";
    $html .= "</div>\n";
    
    return $html;
}

// render row with URL list selection
function rowUrlListSelect($adddelete = false, $scrolldown = false, $noreload = false) {
    global $m, $mode, $tab, $dbConnection, $website, $urllist, $urllistName, $urllistNamespace;
    
    if($adddelete)
        $class = "entry-x-input";
    else
        $class = "entry-input";
    
    $html = "<div class=\"entry-row\">\n";
    
    $html .= "<div class=\"entry-label\">URL list:</div><div class=\"$class\">\n";
    
    $html .= "<select class=\"$class\" id=\"urllist-select\"";
    
    if(isset($m))
        $html .= " data-m=\"$m\"";
        
    if(isset($mode))
        $html .= " data-mode=\"$mode\"";
            
    if(isset($tab))
        $html .= " data-tab=\"$tab\"";
                
    if($scrolldown)
        $html .= " data-scrolldown";
    
        
    if($noreload)
        $html .= " data-noreload";
                    
    $html .= ">\n";
    
    $result = $dbConnection->query(
            "SELECT id, name, namespace".
            " FROM crawlserv_urllists".
            " WHERE website=$website".
            " ORDER BY name"
    );
    
    if(!$result)
        die("ERROR: Could not get URL lists from database.");
        
    if($result) {
        $first = true;
        
        while($row = $result->fetch_assoc()) {
            $ulId = $row["id"];
            $ulName = $row["name"];
            $ulNamespace = $row["namespace"];
            
            if($first) {
                if(!isset($urllist))
                    $urllist = $ulId;
                    
                $first = false;
            }
            
            $html .= "<option value=\"$ulId\"";
            
            if($urllist == $ulId) {
                $urllistName = $ulName;
                $urllistNamespace = $ulNamespace;
                
                $html .= " selected";
            }
            
            $html .= ">".htmlspecialchars($ulName)."</option>\n";
        }
        
        $result->close();
    }
    
    if($adddelete) {
        $html .= "<option value=\"0\"";
        
        if(!$urllist)
            $html .= " selected";
        
        $html .= ">Add URL list</option>\n";
    }
    else if(!isset($urllist))
        $html .= "<option disabled>No URL list available</option>\n";
        
    $html .= "</select>\n";
    
    if($adddelete)
        $html .= "<a href=\"#\" class=\"actionlink urllist-delete\" ><span class=\"remove-entry\">X</span></a>\n";
    
    $html .= "</div>\n";
    $html .= "</div>\n";
    
    return $html;
}

// render row with configuration selection (for specific module)
function rowConfigSelect($module, $adddelete = false, $noreload = false) {
    global $m, $mode, $dbConnection, $website, $config, $configName, $current;
    
    if($adddelete)
        $class = "entry-x-input";
    else
        $class = "entry-input";
    
    $html = "<div class=\"entry-row\">\n";
    
    $html .= "<div class=\"entry-label\">$module:</div><div class=\"$class\">\n";
    
    $html .= "<select id=\"config-select\" class=\"$class\"";
    
    if(isset($m))
        $html .= " data-m=\"$m\"";
    
    if(isset($mode))
        $html .= " data-mode=\"$mode\"";
    
    if($noreload)
        $html .= " data-noreload";
    
    $result = $dbConnection->query(
            "SELECT id, name".
            " FROM crawlserv_configs".
            " WHERE website=$website".
            " AND module='".strtolower($module)."'".
            " ORDER BY name"
    );
    
    $html .= ">\n";
    
    if(!$result)
        die("ERROR: Could not get ".strtolower($module)."s from database.");
        
    $first = true;
        
    while($row = $result->fetch_assoc()) {
        $id = $row["id"];
        $name = $row["name"];
        
        if($first) {
            if(!isset($config))
                $config = $id;
                
                $first = false;
        }
        
        $html .= "<option value=\"".$id."\"";
        
        if($id == $config)
            $html .= " selected";
            
        $html .= ">$name</option>\n";
    }
        
    $result->close();
    
    if(!isset($config))
        $config = 0;
            
    if($config) {
        $result = $dbConnection->query("SELECT name, config FROM crawlserv_configs WHERE id=$config LIMIT 1");
        
        if(!$result)
            die("ERROR: Could not get ".strtolower($module)." data.");
            
            $row = $result->fetch_assoc();
            
            $configName = $row["name"];
            
            if(!isset($current))
                $current = $row["config"];
                
                $result->close();
    }
    else if(!isset($current))
        $current = '[]';

    if($adddelete) {
        $html .= "<option value=\"0\"";
                
        if(!$config)
            $html .= " selected";
                    
        $html .= ">Add ".strtolower($module)."</option>\n";
    }
    else if(!isset($config))
        $html .= "<option disabled>No ".strtolower($module)." available</option>\n";
    
    $html .= "</select>\n";
    
    if($adddelete) {
        $html .= "<a href=\"#\" class=\"actionlink config-delete\" data-m=\"$m\">";
        $html .= "<span class=\"remove-entry\">X</span>";
        $html .= "</a>";
    }
    
    $html .= "</div>\n";
    $html .= "</div>\n";
    
    return $html;
}

// generate script for locales, queries and current configuration
function scriptModule() {
    global $website, $dbConnection, $config;
    
    $script = "";
    
    if($website) {
        $script .= "// load locales, queries and configuration\n\n"; // JavaScript comment
        $script .= "var db_locales = [\n";
        
        $result = $dbConnection->query("SELECT name FROM crawlserv_locales ORDER BY name");
        
        if(!$result)
            die("ERROR: Could not get locales from database.");
            
        $locales = "";
            
        while($row = $result->fetch_assoc())
            $locales .= " \"".$row["name"]."\",";
                
        $locales = substr($locales, 0, -1);
                
        $script .= "$locales];\n\n";
                
        $script .= "var db_queries = [\n";
                
        $result = $dbConnection->query(
                "SELECT id, name, type, resultbool, resultsingle, resultmulti".
                " FROM crawlserv_queries".
                " WHERE website=".$website.
                " OR website IS NULL".
                " ORDER BY name"
        );
                
        if(!$result)
            die("ERROR: Could not get queries from database.");
                    
        while($row = $result->fetch_assoc())
            $script .= " { \"id\": "
                    .$row["id"]
                    .", \"name\": \""
                    .$row["name"]
                    ."\", \"type\": \""
                    .$row["type"]
                    ."\", \"resultbool\": "
                    .($row["resultbool"] ? "true" : "false")
                    .", \"resultsingle\": "
                    .($row["resultsingle"] ? "true" : "false")
                    .", \"resultmulti\": "
                    .($row["resultmulti"] ? "true" : "false")
                    ." },\n";
                                                
        $result->close();
                                                
        $script .= "];\n";
                                                
        $script .= "var db_config = \n";
                                                
        if($config) {
            $result = $dbConnection->query(
                    "SELECT config".
                    " FROM crawlserv_configs".
                    " WHERE id=$config".
                    " LIMIT 1"
            );
            
            if(!$result)
                die("ERROR: Could not get current configuration from database");
                
            $row = $result->fetch_assoc();
                
            $script .= " ".$row["config"];
                
            $result->close();
        }
        else
            $script .= " []\n";
                                                    
        $script .= ";\n";
    }
    
    return $script;
}

?>