
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
 * helpers.js
 * 
 * JavaScript helper functions for the PHP/JavaScript crawlserv++ frontend.
 *  
 */

/*
 * GLOBAL
 */

// handle JSON errors
function handleJsonError(txt, jqxhr, status, thrown) {
	var error = txt + "\n";
	
	if(typeof status !== "undefined") {
		error += status + ": ";
	}
			
	if(typeof thrown !== "undefined") {
		error += thrown + " ";
	}
	
	if(typeof jqxhr !== "undefined") {			
		if(typeof jqxhr.responseJSON !== "undefined") {
			if(typeof jqxhr.responeJSON.message !== "undefined") {
				error += "[" + jqxhr.responseJSON.message + "]";
			}
			else {
				error += "[" + jqxhr.responseJSON + "]";
			}
		}
		else if(typeof jqxhr.status !== "undefined") {
			error += "[" + jqxhr.status + "]";
		}
		else {
			error += jqxhr;
		}
	}
	
	throw new Error(error);
}

// convert milliseconds to string
function msToStr(ms) {
	var result = "";
	
	if(!ms) {
		return "<1ms";
	}
	
	if(ms > 3600000) {
		result = Math.floor(ms / 3600000).toString() + "h ";
		
		ms = ms % 3600000;
	}
	
	if(ms > 60000) {
		result += Math.floor(ms / 60000).toString() + "min ";
		
		ms = ms % 60000;
	}
	
	if(ms > 1000) {
		result += Math.floor(ms/ 1000).toString() + "s ";
		
		ms = ms % 1000;
	}
	
	if(ms > 0) {
		result += ms + "ms ";
	}
	
	return result.substr(0, result.length - 1);
}

// join array, but use different separator for last element
function join(array, separator, lastSeparator) {
	switch(array.length) {
	case 0:
		return "";
		
	case 1:
		return array[0];
	}
	
	return array.slice(0, -1).join(separator) + lastSeparator + array.slice(-1);
}

// encode HTML entities
var entityMap = {
  '&': '&amp;',
  '<': '&lt;',
  '>': '&gt;',
  '"': '&quot;',
  "'": '&#39;',
  '/': '&#x2F;',
  '`': '&#x60;',
  '=': '&#x3D;'
};

function htmlentities(string) {
	return String(string).replace(
			/[&<>"'`=\/]/g,
			function(s) {
					return entityMap[s];
			}
	);
}


/*
 * FRONTEND
 */

// check whether a date is valid
function isValidDate(dateString) {
	if(!dateString.length) {
		return true; // empty string allowed (no date specified)
	}
	
	var regEx = /^\d{4}-\d{2}-\d{2}$/;
	
	if(!dateString.match(regEx)) {
		return false;  // invalid format
	}
	
	var d = new Date(dateString);
	
	if(Number.isNaN(d.getTime())) {
		return false; // invalid date
	}
	
	return d.toISOString().slice(0,10) === dateString;
}

// check whether a (positive) integer is valid [based on https://stackoverflow.com/a/10834843 by T.J. Crowder]
function isNormalInteger(str) {
	var n = Math.floor(Number(str));
	
    return n !== Infinity && String(n) === str && n >= 0;
}

// refresh data
function refreshData() {
	if(crawlserv_frontend_unloading) {
		return;
	}
	
	if($("#server-status").length > 0) {
		var timerStart = +new Date();
		
		$("#server-status").load(cc_host, function(response, status, xhr) {
			if(status == "error") {
				$("#server-status").html("<div class='no-server-status'>crawlserv++ not found at " + cc_host + "</div>");
				
				$("#server-ping-value").text("");
				
				setTimeout(refreshData, 10000);
			}
			else {
				var timerEnd = +new Date();
				
				$("#server-ping").
					html("<span id='_ping1'>&bull;</span><span id='_ping2'>&bull;</span><span id='_ping3'>&bull;</span>");
				
				$("#_ping1").fadeIn(0).fadeOut(600);
				$("#_ping2").fadeIn(0).delay(200).fadeOut(500);
				$("#_ping3").fadeIn(0).delay(300).fadeOut(400, "linear", function() {
					$("#server-ping-value").text(msToStr(timerEnd - timerStart));
				});
				
				setTimeout(refreshData, 5000);
			}
		});
	}
	
	if($("#threads").length > 0) {
		$("#threads").load("php/include/threads.php", function(response, status, xhr) {
			if(status == "error") {
				$("#threads").html("<div class='no-server-status'>Failed to load threads.</div>");
				
				setTimeout(refreshData, 2500);
			}
			else {
				$("#threads-ping").
					html("<span id='_ping1'>&bull;</span><span id='_ping2'>&bull;</span><span id='_ping3'>&bull;</span>");
				
				$("#_ping1").fadeIn(0).fadeOut(600);
				$("#_ping2").fadeIn(0).delay(200).fadeOut(500);
				$("#_ping3").fadeIn(0).delay(300).fadeOut(400);
				
				setTimeout(refreshData, 700);
				
				if(scrolldown) {
					$("#container").scrollTop($("#scroll-to").offset().top);
					
					scrolldown = false;
				}
			}
		});
	}
}

// redirect
function reload(args) {
	console.log("Redirecting...");
	console.log(args);
	
	//args["_redirectstarttime"] = +new Date();
	args["redirected"] = true;
	
	$.redirect("", args);
}

// send command to server
function runCmd(cmd, cmdArgs, doReload, reloadArgs, getReloadArgFrom, saveReloadArgTo, whenDone) {
	if(cmd === "") {
		return;
	}
	
	// check connection to server
	$("#hidden-div").load(cc_host, function(response, status, xhr) {		
		if(status == "error") {
			alert("No connection to the server.");
		}
		else {
			cmdArgs["cmd"] = cmd;
			
			var timerStart = +new Date();
			var data = JSON.stringify(cmdArgs, null, 1);
		
			if(cmd == "download") {
				// send file download command via XMLHttpRequest
				xhttp = new XMLHttpRequest();
				
				xhttp.onreadystatechange = function() {
					var a;
					
					if(xhttp.readyState === 4 && xhttp.status === 200) {
						a = document.createElement("a");
						
						a.href = window.URL.createObjectURL(xhttp.response);
						
						if(cmdArgs["as"])
							a.download = cmdArgs["as"];
						
						a.style.display = "none";
						
						document.body.appendChild(a);
						
						a.click();
						
						if(whenDone) {
							whenDone();
						}
					}
				};
				
				xhttp.open("POST", cc_host);
				
				xhttp.setRequestHeader("Content-Type", "application/json");
				
				xhttp.responseType = "blob";
				
				xhttp.send(data);
			}
			else
				// send other server commands via AJAX
				$.ajax(
						{
							type: "POST",
							url: cc_host,
							data: data,
							contentType: "application/json; charset=utf-8",
							dataType: "json",
							success: function(data) {						
								var timerEnd = +new Date();
								
								if(data["confirm"]) {
									if(
											confirm(
													"crawlserv++ asks ("
													+ msToStr(timerEnd - timerStart)
													+ ")\n\n"
													+ data["text"]
											)
									) {
										cmdArgs["confirmed"] = true;
										
										timerStart = +new Date();
										
										$.ajax(
												{
													type: "POST",
													url: cc_host,
													data: JSON.stringify(cmdArgs, null, 1),
													contentType: "application/json; charset=utf-8",
													dataType: "json",
													success: function(data) {
														timerEnd = +new Date();
														
														if(data["fail"]) {
															var errorStr = "crawlserv++ responded with error ("
																+ msToStr(timerEnd - timerStart)
																+ ")";
															
															if(data["text"].length) {
																errorStr += "\n\n" + data["text"];
															}
															else {
																errorStr += ".";
															}
															
															if(data["debug"]) {
																errorStr += "\n\ndebug: " + data["debug"];
															}
															
															alert(errorStr);
														}
														else {
															if(getReloadArgFrom && saveReloadArgTo) {
																reloadArgs[saveReloadArgTo] = data[getReloadArgFrom];
															}
															else if(data["text"].length) {
																alert("crawlserv++ responded ("
																		+ msToStr(timerEnd - timerStart)
																		+ ")\n\n"
																		+ data["text"]
																);
															}
															
															if(doReload) {
																reload(reloadArgs);
															}
														}
														
														if(whenDone) {
															whenDone();
														}
													},
													failure: function(errMsg) {
														alert(errMsg);
														
														if(whenDone) {
															whenDone();
														}
													}
										});
									}
								}
								else if(data["fail"]) {
									var errorStr = "crawlserv++ responded with error ("
										+ msToStr(timerEnd - timerStart)
										+ ")";
									
									if(data["text"].length) {
										errorStr += "\n\n" + data["text"];
									}
									else {
										errorStr += ".";
									}
									
									if(data["debug"]) {
										errorStr += "\n\ndebug: " + data["debug"];
									}
									
									alert(errorStr);
									
									if(whenDone) {
										whenDone();
									}
								}
								else {
									if(getReloadArgFrom && saveReloadArgTo) {
										reloadArgs[saveReloadArgTo] = data[getReloadArgFrom];
									}
									else if(data["text"].length) {
										alert(
												"crawlserv++ responded ("
												+ msToStr(timerEnd - timerStart)
												+ ")\n\n"
												+ data["text"]
										);
									}
									
									if(doReload) {
										reload(reloadArgs);
									}
									
									if(whenDone) {
										whenDone();
									}
								}
							},
							failure: function(errMsg) {
								alert(errMsg);
								
								if(whenDone) {
									whenDone();
								}
							}
						}
				);
		}
	});
}

// check whether object has data attribute
function hasData(obj, key) {
	var attr = obj.attr("data-" + key);
	
	return (typeof attr !== typeof undefined && attr !== false);
}

// disable input on redirect
function disableInputs() {
	$("select").prop("disabled", true);
	$("input").prop("disabled", true);
	$("textarea").prop("disabled", true);
}

// restore values and enable input on cancelling redirect
function reenableInputs() {
	$("#website-select").val($("#website-select").data("current"));
	$("#urllist-select").val($("#urllist-select").data("current"));
	$("#config-select").val($("#config-select").data("current"));
	$("#algo-cat-select").val($("#algo-cat-select").data("current"));
	$("#algo-select").val($("#algo-select").data("current"));
	
	$("select").prop("disabled", false);
	$("input").prop("disabled", false);
	$("textarea").prop("disabled", false);
}

/*
 * CONFIG
 */

// are two JSON values are unequal (while value2 can be null)?
function areValuesUnequal(value1, value2) {
	if(value2 === null) {
		if(!isNaN(value1)) {
			return value1 != 0;
		}
		
		if(typeof value1 === "string" || value1 instanceof String) {
			return value1 != "";
		}
		
		if(typeof value1 === "boolean") {
			return value1 != false;
		}
		
		return value1 != null;
	}
	
	return value1 != value2;
}

// helper function to compare two configurations
function areConfigsEqual(config1, config2, logging = true) {
	// go through all configuration entries in the first configuration
	for(key1 in config1) {
		// find configuration entry in the second configuration
		var found = false;
		
		for(key2 in config2) {
			if(config1[key1].cat == config2[key2].cat) {
				if(config1[key1].name == config2[key2].name) {
					// configuration entry found: check for array
					if(
							$.isArray(config1[key1].value)
							&& $.isArray(config2[key2].value)
					) {
						// compare array size
						var arrayLength = config1[key1].value.length;
						
						if(arrayLength != config2[key2].value.length) {
							if(logging) {
								console.log(
										"areConfigsEqual(): Unequal number of elements in "
										+ config1[key1].cat + "." + config1[key1].name
								);
							}
							
							return false;
						}
						
						// compare array items
						for(var i = 0; i < arrayLength; i++) {
							if(areValuesUnequal(config1[key1].value[i], config2[key2].value[i])) {
								if(logging) {
									console.log(
											"areConfigsEqual(): " + config1[key1].cat + "." + config1[key1].name
											+ "[" + i + "]: " + config1[key1].value[i]
											+ " != " + config2[key2].value[i]
									);
								}
								
								return false;
							}
						}
					}
					// compare value (ignore change of algorithm)
					else if(
							config1[key1].name != "_algo"
							&& areValuesUnequal(config1[key1].value, config2[key2].value)
					) {
						if(logging) {
							console.log(
									"areConfigsEqual(): " + config1[key1].cat + "." + config1[key1].name +
									": " + config1[key1].value + " != " + config2[key2].value
							);
						}
						
						return false;
					}
					
					found = true;
					
					break;
				}
			}
		}
		
		if(!found && config1[key1].name != "_algo") {
			if(logging) {
				console.log(
						"areConfigsEqual(): " + config1[key1].cat + "."	+ config1[key1].name +
						" [=\'" + config1[key1].value + "\'] not found"
				);
			}
			
			return false;
		}
	}
	
	return true;
};

// helper function to enumerate all queries 
function enumQueries(result) {
	const modules = [ "crawler", "parser", "extractor", "analyzer" ];
	
	modules.forEach(function(module) {
		// add module to result
		result[module] = [];
		
		let moduleResult = result[module];
		
		// parse configuration data from JSON file for module
		console.log("> json/" + module + ".json");
		
		$.getJSON("json/" + module + ".json", function(data) {
			// get algo data
			var deferreds = [];
			var categories = [];
			var results = [];
			var includes = [];
			
			// get data for algorithms from external JSONs
			if(typeof algo !== 'undefined') {
				$.each(algo.config_cats, function(index, category) {
					if(category != "general") {
						categories.push(category);
						
						console.log("> json/algos/" + category + ".json");
						
						deferreds.push($.getJSON("json/algos/" + category + ".json", function(data) {
							results.push(data);
						}));
					}
				});
			}
			
			// get included data from external JSONs
			if(data.hasOwnProperty("#include")) {
				if($.isArray(data["#include"])) {
					$.each(data["#include"], function(index, include) {
						console.log("> json/" + include);
						
						deferreds.push($.getJSON("json/" + include, function(data) {
							includes.push(data);
						}));
					});
				}
				else {
					console.log("> json/" + data["#include"]);
					
					deferreds.push($.getJSON("json/" + data["#include"], function(data) {
						includes.push(data);
					}));
				}
				
				delete data["#include"];
			}
			
			$.when.apply($, deferreds).then(function() {
				// include algo data				
				$.each(results, function(index, algoData) {
					data[categories[index]] = algoData;
				});
				
				// merge with included JSON data
				$.each(includes, function(index, include) {
					$.extend(data, include);
				})
				
				$.each(data, function(cat, entry) {
					$.each(entry, function(name, properties) {
						if(properties.hasOwnProperty("type")) {
							if(
									(
											properties["type"] == "array"
											&& properties.hasOwnProperty("item-type")
											&& properties["item-type"] == "query"
									)
									|| properties["type"] == "query"
							) {
								moduleResult.push({
									"cat": cat,
									"name": name
								})
							}
						}
					});
				});
			});
		});
	});
}

/*
 * IMPORT/EXPORT [DATA]
 */

function disableImportExport() {
	$("#firstline-header").prop("disabled", !$("#write-firstline-header").prop("checked"));
}
