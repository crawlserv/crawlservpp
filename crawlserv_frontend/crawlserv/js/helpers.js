/**
 * Helper functions for crawlserv++ frontend
 */

/*
 * GLOBAL
 */

// handle JSON errors
function handleJsonError(txt, jqxhr, status, thrown) {
	var error = txt + "\n";
	
	if(typeof status !== "undefined")
		error += status + ": ";
			
	if(typeof thrown !== "undefined")
		error += thrown + " ";
	
	if(typeof jqxhr !== "undefined") {
		if(typeof jqxhr.responseJSON !== "undefined") {
			if(typeof jqxhr.responeJSON.message !== "undefined")
				error += "[" + jqxhr.responseJSON.message + "]";
			else
				error += "[" + jqxhr.responseJSON + "]";
		}
		else
			error += jqxhr;
	}
		
	
	throw new Error(error);
}

// convert milliseconds to string
function msToStr(ms) {
	var result = "";
	
	if(!ms) return "<1ms";
	
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
	if(ms > 0) result += ms + "ms ";
	return result.substr(0, result.length - 1);
}

/*
 * FRONTEND
 */

// check whether a date is valid
function isValidDate(dateString) {
	if(!dateString.length) return true; // empty string allowed (no date specified)
	var regEx = /^\d{4}-\d{2}-\d{2}$/;
	if(!dateString.match(regEx)) return false;  // invalid format
	var d = new Date(dateString);
	if(Number.isNaN(d.getTime())) return false; // invalid date
	return d.toISOString().slice(0,10) === dateString;
}

// check whether a (positive) integer is valid [based on https://stackoverflow.com/a/10834843 by T.J. Crowder]
function isNormalInteger(str) {
	var n = Math.floor(Number(str));
    return n !== Infinity && String(n) === str && n >= 0;
}

// refresh data
function refreshData() {
	if(crawlserv_frontend_unloading) return;
	if($("#server-status").length > 0) {
		var timerStart = +new Date();
		$("#server-status").load("http://localhost:8080", function(response, status, xhr) {
			if(status == "error") {
				$("#server-status").html("<div class='no-server-status'>crawlserv not found</div>");
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
		$("#threads").load("php/_threads.php", function(response, status, xhr) {
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

// run command with arguments
function runCmd(cmd, cmdArgs, doReload, reloadArgs, getReloadArgFrom, saveReloadArgTo) {
	if(cmd === "")
		return;
	
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
				
				a.download = cmdArgs["filename"];
				
				a.style.display = "none";
				
				document.body.appendChild(a);
				
				a.click();
			}
		};
		
		xhttp.open("POST", "http://localhost:8080");
		
		xhttp.setRequestHeader("Content-Type", "application/json");
		
		xhttp.responseType = "blob";
		
		xhttp.send(data);
	}
	else
		// send other server commands via AJAX
		$.ajax(
				{
					type: "POST",
					url: "http://localhost:8080",
					data: data,
					contentType: "application/json; charset=utf-8",
					dataType: "json",
					success: function(data) {
						console.log(data);
						
						var timerEnd = +new Date();
						
						if(data["confirm"]) {
							if(confirm("crawlserv asks (" + msToStr(timerEnd - timerStart) + ")\n\n" + data["text"])) {
								cmdArgs["confirmed"] = true;
								
								timerStart = +new Date();
								
								$.ajax(
										{
											type: "POST",
											url: "http://localhost:8080",
											data: JSON.stringify(cmdArgs, null, 1),
											contentType: "application/json; charset=utf-8",
											dataType: "json", success: function(data) {
												timerEnd = +new Date();
												
												if(data["fail"]) {
													var errorStr = "crawlserv responded with error ("
														+ msToStr(timerEnd - timerStart) + ")";
													
													if(data["text"].length)
														errorStr += "\n\n" + data["text"];
													else
														errorStr += ".";
													
													if(data["debug"]) 
														errorStr += "\n\ndebug: " + data["debug"];
													
													alert(errorStr);
												}
												else {
													if(getReloadArgFrom && saveReloadArgTo)
														reloadArgs[saveReloadArgTo] = data[getReloadArgFrom];
													else if(data["text"].length)
														alert("crawlserv responded ("
																+ msToStr(timerEnd - timerStart)
																+ ")\n\n"
																+ data["text"]
														);
													if(doReload)
														reload(reloadArgs);
												}
											},
											failure: function(errMsg) { alert(errMsg); }
								});
							}
						}
						else if(data["fail"]) {
							var errorStr = "crawlserv responded with error (" + msToStr(timerEnd - timerStart) + ")";
							
							if(data["text"].length)
								errorStr += "\n\n" + data["text"];
							else
								errorStr += ".";
							
							if(data["debug"])
								errorStr += "\n\ndebug: " + data["debug"];
							
							alert(errorStr);
						}
						else {
							if(getReloadArgFrom && saveReloadArgTo)
								reloadArgs[saveReloadArgTo] = data[getReloadArgFrom];
							else if(data["text"].length) {
								alert("crawlserv responded (" + msToStr(timerEnd - timerStart) + ")\n\n" + data["text"]);
							}
							
							if(doReload)
								reload(reloadArgs);
						}
					},
					failure: function(errMsg) { alert(errMsg); }
				}
		);
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

//helper function to compare two configurations
function areConfigsEqual(config1, config2, logging = false) {
	// go through all configuration entries in the first configuration
	for(key1 in config1) {
		// find configuration entry in the second configuration
		var found = false;
		
		for(key2 in config2) {
			if(config1[key1].cat == config2[key2].cat) {
				if(config1[key1].name == config2[key2].name) {
					// configuration entry found: check for array
					if($.isArray(config1[key1].value)
							&& $.isArray(config2[key2].value)) {
						// compare array size
						var arrayLength = config1[key1].value.length;
						
						if(arrayLength != config2[key2].value.length) {
							if(logging)
								console.log(
										"areConfigsEqual(): Unequal number of elements in "
										+ config1[key1].cat + "." + config1[key1].name
								);
							return false;
						}
						
						// compare array items
						for(var i = 0; i < arrayLength; i++) {
							if(config1[key1].value[i] != config2[key2].value[i]) {
								if(logging)
									console.log(
											"areConfigsEqual(): " + config1[key1].cat + "." + config1[key1].name
											+ "[" + i + "]: " + config1[key1].value[i]
											+ " != " + config2[key2].value[i]
									);
								
								return false;
							}
						}
					}
					// compare value (ignore change of algorithm)
					else if(config1[key1].name != "_algo"
						&& config1[key1].value != config2[key2].value) {
						if(logging)
							console.log(
									"areConfigsEqual(): " + config1[key1].cat + "." + config1[key1].name +
									": " + config1[key1].value + " != " + config2[key2].value
						);
						
						return false;
					}
					
					found = true;
					
					break;
				}
			}
		}
		
		if(!found
				&& config1[key1].name != "_algo") {
			if(logging)
				console.log(
						"areConfigsEqual(): " + config1[key1].cat + "."	+ config1[key1].name +
						" [=\'" + config1[key1].value + "\'] not found"
				);
			
			return false;
		}
	}
	
	return true;
};