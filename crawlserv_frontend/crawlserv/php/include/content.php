
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
 * content.php
 *
 * Content helper functions for the PHP/JavaScript frontend for crawlserv++.
 *
 */

// function to wrap long code lines
function codeWrapper($str, $limit, $indent_limit) {
    $out = "";
    
    foreach(preg_split("/((\r?\n)|(\r\n?))/", $str) as $line) {
        while(strlen($line) > $limit) {
            // get indent
            $indent = strspn($line, " \t");
            
            if($indent !== false) {
                if($indent < strlen($line)) {
                    if($indent > $indent_limit)
                        $indent = $indent_limit;
                        
                    $begin = substr($line, 0, $indent);
                }
                else
                    break;
            }
            else
                $begin = "";
                
            // find last space, tab or tag in line inside $limit
            $allowed = substr($line, 0, $limit);
            
            $space = strrpos($allowed, " ", $indent + 1);
            
            if($space === false)
                $space = 0;
                
            $tab = strrpos($allowed, "\t", $indent + 1);
            
            if($tab === false)
                $tab = 0;
                
            $tag = strrpos($allowed, "<", $indent + 1);
            
            if($tag === false)
                $tag = 0;
            else
                $tag--;
                
            $cut = max($space, $tab, $tag);
            
            if($cut > 0) {
                $out .= substr($line, 0, $cut + 1)."\n";
                
                $line = $begin.substr($line, $cut + 1);
            }
            else {
                // find last hyphen, comma or semicolon in line inside $limit
                $allowed = substr($line, 0, $limit);
                
                $hyphen = strrpos($allowed, "-", $indent + 1);
                
                if($space === false)
                    $space = 0;
                    
                $comma = strrpos($allowed, ",", $indent + 1);
                
                if($tab === false)
                    $tab = 0;
                    
                $semicolon = strrpos($allowed, ";", $indent + 1);
                
                if($tag === false)
                    $tag = 0;
                else
                    $tag--;
                    
                $cut = max($hyphen, $comma, $semicolon);
                
                if($cut > 0) {
                    $out .= substr($line, 0, $cut + 1)."\n";
                    
                    $line = $begin.substr($line, $cut + 1);
                }
                else {
                    // find first space, tab or tag outside $limit
                    $l = strlen($line);
                    
                $space = strpos($line, " ", $limit);
                
                if($space === false)
                    $space = $l;
                    
                $tab = strpos($line, "\t", $limit);
                
                if($tab === false)
                    $tab = $l;
                    
                $tag = strpos($line, "<", $limit);
                
                if($tag === false)
                    $tag = $l;
                    else
                        $tag--;
                        
                    $cut = min($space, $tab, $tag);
                    
                    if($cut < $l) {
                        $out .= substr($line, 0, $cut + 1)."\n";
                        
                        $line = $begin.substr($line, $cut + 1);
                    }
                    else {
                        // try to break at hyphen or comma
                        $hyphen = strpos($line, "-", $limit);
                        
                        if($hyphen === false)
                            $hyphen = $l;
                            
                        $comma = strpos($line, ",", $limit);
                        
                        if($comma === false)
                            $comma = $l;
                            
                        $cut = min($hyphen, $comma);
                        
                        if($cut < $l) {
                            $out .= substr($line, 0, $cut + 1)."\n";
                            
                            $line = $begin.substr($line, $cut + 1);
                        }
                        else {
                            $out .= $line."\n";
                            
                            $line = "";
                        }
                    }
                }
            }
        }
        
        $out .= $line."\n";
    }
    
    return $out."\n";
}

// function to check for JSON code
function isJSON($str) {
    if($str[0] == '{' || $str[0] == "[") {
        json_decode($str);
        
        return json_last_error() == JSON_ERROR_NONE;
    }
    
    return false;
}

?>