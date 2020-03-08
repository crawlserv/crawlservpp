
<?php

/*
 * 
 * ---
 * 
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * _helpers.php
 * 
 * PHP helper functions for the PHP/JavaScript frontend for crawlserv++.
 * 
 */

// render row with website selection
function rowWebsiteSelect(
    $adddelete = false,
    $showglobal = false,
    $scrolldown = false,
    $select_id = "website-select",
    $label = "Website",
    $filter = []
) {
    global  $m,
            $mode,
            $tab,
            $module,
            $action,
            $datatype,
            $filetype,
            $compression,
            $dbConnection,
            $website,
            $websiteName,
            $namespace,
            $domain,
            $dir;
    
    flush();
    
    if($adddelete)
        $class = "entry-x-input";
    else
        $class = "entry-input";
    
    $html = "<div class=\"entry-row\">\n";
    
    $html .= "<div class=\"entry-label\">$label:</div>";
    $html .= "<div class=\"$class\">";
    
    $html .= "<select class=\"$class\" id=\"$select_id\"";
    
    if(isset($m))
        $html .= " data-m=\"$m\"";
    
    if(isset($mode))
        $html .= " data-mode=\"$mode\"";
    
    if(isset($tab))
        $html .= " data-tab=\"$tab\"";
    
    if(isset($module))
        $html .= " data-module=\"$module\"";
    
    if(isset($action))
        $html .= " data-action=\"$action\"";
    
    if(isset($datatype))
        $html .= " data-datatype=\"$datatype\"";
    
    if(isset($filetype))
        $html .= " data-filetype=\"$filetype\"";
    
    if(isset($compression))
        $html .= " data-compression=\"$compression\"";
    
    if($scrolldown)
        $html .= " data-scrolldown";
    
    $html .= ">\n";
    
    if($showglobal && !in_array(0, $filter, true)) {
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
        
        if(in_array($id, $filter, true))
            continue;
        
        $name = $row["name"];
        $data_namespace = $row["namespace"];
        
        if($first) {
            if(!isset($website))
                $website = $id;
                
            $first = false;
        }
        
        $html .= "<option value=\"$id\" data-namespace=\"$data_namespace\"";
        
        if($website == $id) {
            $html .= " selected";
            
            $websiteName = $name;
            $namespace = $data_namespace;
            $domain = $row["domain"];
            $dir = $row["dir"];
        }
            
        $html .= ">".htmlspecialchars($name)."</option>\n";
    }
    
    $result->close();
    
    if($adddelete) {
        $html .= "<option value=\"0\"";
        
        if(!$website)
            $html .= " selected";
        
        $html .= ">Add website</option>\n";
    }
    else if(!isset($website))
        $html .= "<option disabled>No website available</option>\n";
    
    $html .= "</select>\n";
    
    if($adddelete)
        $html .= "<a id=\"website-delete\" href=\"#\" class=\"actionlink\"><span class=\"remove-entry\">X</span></a>";
    
    $html .= "</div>\n";
    $html .= "</div>\n";
    
    return $html;
}

// render row with URL list selection
function rowUrlListSelect($add = false, $delete = false, $scrolldown = false, $noreload = false, $id = "urllist-select") {
    global  $m,
            $mode,
            $tab,
            $dbConnection,
            $website,
            $urllist,
            $urllistName,
            $urllistNamespace;
    
    flush();
    
    if($delete)
        $class = "entry-x-input";
    else
        $class = "entry-input";
    
    $html = "<div id=\"$id-div\" class=\"entry-row\">\n";
    
    $html .= "<div class=\"entry-label\">URL list:</div><div class=\"$class\">\n";
    
    $html .= "<select class=\"$class\" id=\"$id\"";
    
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
            
            $html .= "<option value=\"$ulId\" data-namespace=\"$ulNamespace\"";
            
            if($urllist == $ulId) {
                $urllistName = $ulName;
                $urllistNamespace = $ulNamespace;
                
                $html .= " selected";
            }
            
            $html .= ">".htmlspecialchars($ulName)."</option>\n";
        }
        
        $result->close();
    }
    
    if($add) {
        $html .= "<option value=\"0\"";
        
        if(!$urllist)
            $html .= " selected";
        
        $html .= ">Add URL list</option>\n";
    }
    else if(!isset($urllist))
        $html .= "<option disabled>No URL list available</option>\n";
        
    $html .= "</select>\n";
    
    if($delete)
        $html .= "<a id=\"urllist-delete\" href=\"#\" class=\"actionlink\" ><span class=\"remove-entry\">X</span></a>\n";
    
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
        $html .= "<a id=\"config-delete\" href=\"#\" class=\"actionlink\" data-m=\"$m\">";
        $html .= "<span class=\"remove-entry\">X</span>";
        $html .= "</a>";
    }
    
    $html .= "</div>\n";
    $html .= "</div>\n";
    
    return $html;
}

// generate script for locales
function scriptLocales() {
    global $dbConnection;
    
    $script = "";
    
    $script .= "// load locales\n\n"; // JavaScript comment
    
    $script .= "var db_locales = [\n";
    
    $result = $dbConnection->query("SELECT name FROM crawlserv_locales ORDER BY name");
    
    if(!$result)
        die("ERROR: Could not get locales from database.");
        
    $locales = "";
        
    while($row = $result->fetch_assoc())
        $locales .= " \"".$row["name"]."\",";
            
    $locales = substr($locales, 0, -1);
    
    $script .= "$locales];\n\n";
    
    return $script;
}

// generate script for locales, queries and current configuration
function scriptModule() {
    global $website, $dbConnection, $config;
    
    $script = "";
    
    if($website) {
        $script .= scriptLocales();
        
        $script .= "// load queries and configuration\n\n"; // JavaScript comment
                
        $script .= "var db_queries = [\n";
                
        $result = $dbConnection->query(
                "SELECT id, name, type, resultbool, resultsingle, resultmulti, resultsubsets".
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
                    .", \"name\": "
                    .json_encode($row["name"])
                    .", \"type\": \""
                    .$row["type"]
                    ."\", \"resultbool\": "
                    .($row["resultbool"] ? "true" : "false")
                    .", \"resultsingle\": "
                    .($row["resultsingle"] ? "true" : "false")
                    .", \"resultmulti\": "
                    .($row["resultmulti"] ? "true" : "false")
                    .", \"resultsubsets\": "
                    .($row["resultsubsets"] ? "true" : "false")
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
            $script .= " []";
                                                    
        $script .= ";\n";
    }
    
    return $script;
}

// datetime to time elapsed string function based on https://stackoverflow.com/a/18602474 by Glavić
function time_elapsed_string($datetime) {
    $now = new DateTime;
    $ago = new DateTime($datetime);
    
    $diff = $now->diff($ago);
    
    $diff->w = floor($diff->d / 7);
    $diff->d -= $diff->w * 7;
    
    $string = array(
        'y' => 'y',
        'm' => 'mth',
        'w' => 'w',
        'd' => 'd',
        'h' => 'h',
        'i' => 'min',
        's' => 's',
    );
    
    foreach($string as $k => &$v) {
        if ($diff->$k)
            $v = $diff->$k . $v;
            else
                unset($string[$k]);
    }
    
    $string = array_slice($string, 0, 1);
    
    return $string ? implode(', ', $string) . ' ago' : 'just now';
}

?>