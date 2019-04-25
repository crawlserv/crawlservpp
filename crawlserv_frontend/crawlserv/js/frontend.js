
// is page unloading?
var crawlserv_frontend_unloading = false;

// helpers
var helperRegEx = null;
var helperXPath = null;
var helperJSONPointer = null;

// fullscreen mode
var fullscreen = false;

/*
 * HELPER FUNCTIONS
 */

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
	
	$.ajax(
			{
				type: "POST",
				url: "http://localhost:8080",
				data: JSON.stringify(cmdArgs, null, 1),
				contentType: "application/json; charset=utf-8",
				dataType: "json",
				success: function(data) {
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

jQuery(function($) {

/*
 * GLOBAL EVENTS
 */
	
// DOCUMENT READY
	$(document).ready(function() {
		// check website inputs
		$("#website-domain").prop("disabled", $("#website-crossdomain").prop("checked"));
		$("#website-dir").prop("disabled", !$("#website-externaldir").prop("checked"));
		
		// save selected values
		$("#website-select").data("current", $("#website-select").val());
		$("#urllist-select").data("current", $("#urllist-select").val());
		$("#config-select").data("current", $("#config-select").val());
		$("#algo-cat-select").data("current", $("#algo-cat-select").val());
		$("#algo-select").data("current", $("#algo-select").val());
		
		// show redirection time if available
		if(redirected)
			$("#redirect-time").text(msToStr(+new Date() - localStorage["_crawlserv_leavetime"]));
		
		// check query type if necessary
		if($("#query-type-select")) {
			if($("#query-type-select").val() == "regex") {
				if(!helperXPath)
					helperXPath = $("#xpath-helper").detach();
				
				if(!helperJSONPointer)
					helperJSONPointer = $("#jsonpointer-helper").detach();
				
				$("#query-text-only").prop("checked", false);
				$("#query-text-only").prop("disabled", true);
				$("#query-text-only-label").addClass("check-label-disabled");
			}
			else if($("#query-type-select").val() == "xpath") {
				if(!helperRegEx)
					helperRegEx = $("#regex-helper").detach();
				
				if(!helperJSONPointer)
					helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			else if($("#query-type-select").val() == "jsonpointer") {
				if(!helperRegEx)
					helperRegEx = $("#regex-helper").detach();
				
				if(!helperXPath)
					helperXPath = $("#xpath-helper").detach();
				
				$("#query-text-only").prop("checked", false);
				$("#query-text-only").prop("disabled", true);
				$("#query-text-only-label").addClass("check-label-disabled");
			}
		}
		
		// set timer
		refreshData();
		
		// enable CORS
		$.support.cors = true;
	});
	
// DOCUMENT UNLOAD
	$(window).on('beforeunload', function() {
		// set status
		crawlserv_frontend_unloading = true;
		
		// set leave time
		localStorage["_crawlserv_leavetime"] = +new Date();
	});

// CHANGE EVENT: check date
	$(document).on("change", "input[type='date']", function() {
		if(isValidDate($(this).val()))
			return true;
		
		alert("Please enter an empty or a valid date in the format \'YYYY-MM-DD\'.");
		
		return false;
	});
	
// CLICK EVENT: navigation
	$(document).on("click", "a.post-redirect", function() {
		if(typeof $(this).data("mode") !== "undefined")
			reload({
				"m" : $(this).data("m"),
				"mode" : $(this).data("mode")
			});
		else
			reload({
				"m" : $(this).data("m")
			});
		
		return false;
	});

// CLICK EVENT: navigation with filter
	$(document).on("click", "a.post-redirect-filter", function() {
		reload({
			"m" : $(this).data("m"),
			"filter" : $(this).data("filter")
		});
		
		return false;
	});
	
// CLICK EVENT: run command without arguments
	$(document).on("click", "a.cmd", function() {
  		runCmd(
  				$(this).data("cmd"),
  				{},
  				hasData($(this), "reload"),
  				{ "m" : $(this).data("m") }
  		);
  		
		return false;
	});
	
// KEYUP EVENT: exit fullscreen on ESC
	$(document).on("keyup", function(e) {
		if(e.keyCode === 27
				&& $("#content-fullscreen")
				&& fullscreen) {
			$("#content-fullscreen").click();
			
			return false;
		}
		
		return true;
	});

/* 
 * EVENTS FOR SERVER
 */

// CLICK EVENT: add allowed IP
	$(document).on("click", "a.cmd-allow", function() {
	    runCmd(
	    		"allow",
	    		{ "ip" : document.getElementById("cmd-allow-form-ip").value },
	    		false,
	    		null
	    );
	    
		return false;
	});

// CLICK EVENT: add argument to custom command
	$(document).on("click", "a.cmd-custom-addarg", function() {
  		var argId = 0;
  		var argTotal = $("#cmd-args .cmd-custom-arg").length;
  		
  		if(argTotal)
  			argId = parseInt($("#cmd-args .cmd-custom-arg").last().data("n"), 10) + 1;
  		
  		$("#cmd-args").append(
  				"<div class=\"cmd-custom-arg entry-row\" data-n=\"" + argId + "\">" +
  				"<div class=\"entry-label\">Argument " +
  				"<div class=\"cmd-custom-arg-no\" style=\"display:inline-block\">" + (argTotal + 1) + "</div>:</div>" +
  				"<div class=\"entry-halfx-input\"><input type=\"text\" class=\"entry-halfx-input cmd-custom-arg-name\" /></div>" +
  				"<div class=\"entry-separator\">=</div>" +
  				"<div class=\"entry-halfx-input\"><input type=\"text\" class=\"entry-halfx-input cmd-custom-arg-value\" /></div>" +
  				"<a href=\"\" class=\"action-link cmd-custom-remarg\" data-n=\"" + argId + "\">" +
  				"<span class=\"remove-entry\">X</div></a></div>"
  		);
  		
  		$(".cmd-custom-arg-name").last().focus();
  		
  		return false;
  	});

// CLICK EVENT: remove argument from custom command
	$(document).on("click", "a.cmd-custom-remarg", function() {
  		var n = parseInt($(this).data("n"), 10);
  		var toRemove = null;
  		
  		$("#cmd-args .cmd-custom-arg").each(function(i, obj) {
  			var objn = parseInt($(obj).data("n"), 10);
  			if(n == objn) {
  				toRemove = obj;
  			}
  			else if(n < objn) {
  				var div = $(this).find("div.cmd-custom-arg-no");
  				div.text(parseInt(div.text(), 10) - 1);
  			}
  		});
  		
  		if(toRemove) toRemove.remove();
  		return false;
  	});

// CLICK EVENT: run custom command with custom arguments
	$(document).on("click", "a.cmd-custom", function() {
  		var args = {};
  		$("#cmd-args .cmd-custom-arg").each(function(i, obj) { 
  			var argname = $(this).find(".cmd-custom-arg-name").val();
  			var argvalue = $(this).find(".cmd-custom-arg-value").val();
  			
  			if(!isNaN(argvalue))
  				argvalue = +argvalue;
  			else if(argvalue == "true")
  				argvalue = true;
  			else if(argvalue == "false")
  				argvalue = false;
  			
  			if(argname && argname.length)
  				args[argname] = argvalue;
  		});
  		
		runCmd(document.getElementById("cmd-custom-form-cmd").value, args, false, null);
		return false;
	});

/*
 * EVENTS FOR LOGS
 */

// CLICK EVENT: navigate log entries
	$(document).on("click", "a.logs-nav", function() {
		reload({
			"m" : "logs",
			"filter" : $(this).data("filter"),
			"offset" : $(this).data("offset"),
			"limit" : $(this).data("limit")
		});
		
		return false;
	});

// CLICK EVENT: clear log entries
	$(document).on("click", "a.logs-clear", function() {
		runCmd(
				"clearlogs",
				{ "module" : $(this).data("filter") },
				true,
				{
					"m": "logs",
					"filter" : $(this).data("filter")
				}
		);
		
		return false;
	});

/*
/* EVENTS FOR WEBSITES
*/

// CHANGE EVENT: website selected
	$("#website-select").on("change", function() {
		$(this).blur();
		disableInputs();
		
		var id = parseInt($(this).val(), 10);
		var args = { "m" : $(this).data("m"), "website" : id };
		
		if($("#query-test-text"))
			args["test"] = $("#query-test-text").val();
		
		if(typeof $(this).data("tab") !== "undefined")
			args["tab"] = $(this).data("tab");
		
		if(typeof $(this).data("mode") !== "undefined")
			args["mode"] = $(this).data("mode");
		
		if(typeof $(this).data("scrolldown") !== "undefined")
			args["scrolldown"] = true;
		
		if(typeof config !== "undefined") {
			if(config.isConfChanged()) {
				if(confirm("Do you want to discard the changes to your current configuration?"))
					reload(args);
				else
					reenableInputs();
			}
			else
				reload(args);
		}
		else {
			// reload in "Threads" menu also
			reload(args);
		}
		return false;
	});
	
// CHANGE EVENT: URL list selected
	$("#urllist-select").on("change", function() {
		$(this).blur();
		
		if(typeof $(this).data("noreload") === "undefined") {
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var id = parseInt($(this).val(), 10);
			var args = {
					"m" : $(this).data("m"),
					"website" : website,
					"urllist" : id
			};
			
			if(typeof $(this).data("tab") !== "undefined")
				args["tab"] = $(this).data("tab");
			
			reload(args);
			
			return false;
		}
		
		return true;
	});
	
// CHANGE EVENT: cross-domain website toggled
	$("#website-crossdomain").on("change", function() {
		$("#website-domain").prop("disabled", $(this).prop("checked"));
	});
	
// CHANGE EVENT: external directory toggled
	$("#website-externaldir").on("change", function() {
		if($(this).prop("checked")) {
			$("#website-dir").prop("disabled", false); 
			$("#website-dir").val("");
		}
		else {
			$("#website-dir").prop("disabled", true);
			$("#website-dir").val("[default]");
		}
	});

// CLICK EVENT: add website
	$(document).on("click", "a.website-add", function() {
		runCmd(
				"addwebsite",
				{
					"name" : $("#website-name").val(),
					"namespace" : $("#website-namespace").val(),
					"domain" : $("#website-crossdomain").prop("checked") ? undefined : $("#website-domain").val(),
					"crossdomain": $("#website-crossdomain").prop("checked"),
					"dir" : $("#website-externaldir").prop("checked") ? $("#website-dir").val() : undefined
				},
				true,
				{ "m" : "websites" },
				"id",
				"website"
		);
		
		return false;
	});
	
// CLICK EVENT: duplicate website
	$(document).on("click", "a.website-duplicate", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"duplicatewebsite",
				{ "id" : id },
				true,
				{ "m" : "websites" },
				"id",
				"website"
		);
		
		return false;
	});
	
// CLICK EVENT: update website
	$(document).on("click", "a.website-update", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"updatewebsite",
				{
					"id" : id,
					"name" : $("#website-name").val(),
					"namespace" : $("#website-namespace").val(),
					"crossdomain": $("#website-crossdomain").prop("checked"),
					"domain" : $("#website-crossdomain").prop("checked") ? undefined : $("#website-domain").val()
				},
				true,
				{
					"m" : "websites",
					"website" : id
				}
		);
		
		return false;
	});

// CLICK EVENT: delete website
	$(document).on("click", "a.website-delete", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		if(id)
			runCmd(
					"deletewebsite",
					{ "id" : id },
					true,
					{ "m" : "websites" }
		);
		
		return false;
	});

// CLICK EVENT: add URL list
	$(document).on("click", "a.urllist-add", function() {
		var website = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"addurllist",
				{
					"website" : website,
					"name" : $("#urllist-name").val(),
					"namespace" : $("#urllist-namespace").val()
				},
				true,
				{
					"m" : "websites",
					"website" : website
				},
				"id",
				"urllist"
		);
		return false;
	});
	
// CLICK EVENT: update URL list
	$(document).on("click", "a.urllist-update", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#urllist-select").val(), 10);
		
		runCmd(
				"updateurllist",
				{
					"id" : id,
					"name" : $("#urllist-name").val(),
					"namespace" : $("#urllist-namespace").val()
				},
				true,
				{
					"m" : "websites",
					"website" : website,
					"urllist" : id
				}
		);
		
		return false;
	});

// CLICK EVENT: delete URL list
	$(document).on("click", "a.urllist-delete", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#urllist-select").val(), 10);
		
		if(id)
			runCmd("deleteurllist",
					{ "id" : id },
					true,
					{
						"m" : "websites",
						"website" : website
					}
			);
		return false;
	});
	
// CLICK EVENT: download URL list
	$(document).on("click", "a.urllist-download", function() {
		var ulwebsite = $(this).data("website-namespace");
		var ulnamespace = $(this).data("namespace");
		var ulfilename = ulwebsite + "_" + ulnamespace + ".txt";
		
		$.post(
				"download/",
				{
					"type": "urllist",
					"namespace": ulnamespace,
					"website": ulwebsite,
					"filename": ulfilename
				},
				function() {
					window.open("download/");
				}
		);
		
		return false;
	});
	
// CLICK EVENT: reset parsing status
	$(document).on("click", "a.urllist-reset-parsing", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		
		if(urllist)
			runCmd(
					"resetparsingstatus",
					{ "urllist" : urllist },
					true,
					{
						"m" : "websites",
						"website" : website
					}
			);
		
		return false;
	});
	
// CLICK EVENT: reset extracting status
	$(document).on("click", "a.urllist-reset-extracting", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		
		if(urllist)
			runCmd(
					"resetextractingstatus",
					{ "urllist" : urllist },
					true,
					{
						"m" : "websites",
						"website" : website
					}
			);
		
		return false;
	});
	
// CLICK EVENT: reset analyzing status
	$(document).on("click", "a.urllist-reset-analyzing", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		
		if(urllist)
			runCmd(
					"resetanalyzingstatus", 
					{ "urllist" : urllist },
					true,
					{
						"m" : "websites",
						"website" : website
					}
			);
		
		return false;
	});
	
/*
 * EVENTS for QUERIES
 */
	
// CHANGE EVENT: query selected
	$("#query-select").on("change", function() {
		$(this).blur();
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($(this).val(), 10);
		
		reload(
				{
					"m" : $(this).data("m"),
					"website" : website,
					"query" : id,
					"test" : $("#query-test-text").val()
				}
		);
		
		return false;
	});
	
// CHANGE EVENT: query type selected
	$("#query-type-select").on("change", function() {
		if($(this).val() == "regex") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", true);
			$("#query-text-only-label").addClass("check-label-disabled");
			
			helperRegEx.insertAfter("#query-properties");
			helperRegEx = null;
			
			if(!helperXPath)
				helperXPath = $("#xpath-helper").detach();
			
			if(!helperJSONPointer)
				helperJSONPointer = $("#jsonpointer-helper").detach();
		}
		else if($(this).val() == "xpath") {
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			helperXPath.insertAfter("#query-properties");
			helperXPath = null;
			
			if(!helperRegEx)
				helperRegEx = $("#regex-helper").detach();
			
			if(!helperJSONPointer)
				helperJSONPointer = $("#jsonpointer-helper").detach();
		}
		else if($(this).val() == "jsonpointer") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", true);
			$("#query-text-only-label").addClass("check-label-disabled");
			
			helperJSONPointer.insertAfter("#query-properties");
			helperJSONPointer = null;
			
			if(!helperRegEx)
				helperRegEx = $("#regex-helper").detach();
			
			if(!helperXPath)
				helperXPath = $("#xpath-helper").detach();
		}
		
		return true;
	});
	
// CHANGE EVENT: XPath helper result type selected
	$("input[name='xpath-result']").on("change", function() {
		if($(this).val() == "property") {
			$("#xpath-result-property").prop("disabled", false);
			$("#xpath-result-property").select();
			$("#xpath-generate").text("Generate query for HTML attribute");
		}
		else {
			$("#xpath-result-property").prop("disabled", true);
			$("#xpath-generate").text("Generate query for HTML tag");
		}
		return true;
	});
	
// CLICK EVENT: let XPath helper generate a query
	$(document).on("click", "a.xpath-generate", function() {
		var query = "";
		var property = $("input:radio[name='xpath-result']:checked").val() == "property";
		
		if(!$("#xpath-element").val().length) {
			alert("Please enter a tag name into the first input field.");
			$("#xpath-element").focus();
			return false;
		}
		
		if(!$("#xpath-property").val().length) {
			alert("Please enter an attribute name into the second input field.");
			$("#xpath-property").focus();
			return false;
		}
		
		if(!$("#xpath-value").val().length) {
			alert("Please enter an attribute value into the third input field.");
			$("#xpath-value").focus();
			return false;
		}
		
		if(property && !$("#xpath-result-property").val().length) {
			alert("Please enter an attribute name into the fourth input field.");
			$("#xpath-result-property").focus();
			return false;
		}
		
		if($("#query-text").val().length && !confirm("Do you want to override the existing query?"))
			return false;
		
		query = "//"
				+ $("#xpath-element").val()
				+ "[contains(concat(' ', normalize-space(@"
				+ $("#xpath-property").val()
				+ "), ' '), ' "
				+ $("#xpath-value").val() + " ')]";
		
		if(property)
			query += "/@" + $("#xpath-result-property").val();
		
		$("#query-text").val(query);
		
		return false;
	});
	
// CLICK EVENT: add query
	$(document).on("click", "a.query-add", function() {
		var website = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"addquery",
				{
					"website" : website,
					"name" : $("#query-name").val(),
					"query" : $("#query-text").val(),
					"type" : $("#query-type-select").val(),
					"resultbool": $("#query-result-bool").is(":checked"),
					"resultsingle" : $("#query-result-single").is(":checked"),
					"resultmulti" : $("#query-result-multi").is(":checked"),
					"textonly" : $("#query-text-only").is(":checked") 
				},
				true,
				{
					"m" : "queries",
					"website" : website,
					"test" : $("#query-test-text").val()
				},
				"id",
				"query"
		);
		
		return false;
	});
	
// CLICK EVENT: duplicate query
	$(document).on("click", "a.query-duplicate", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#query-select").val(), 10);
		
		if(id)
			runCmd(
					"duplicatequery",
					{ "id" : id },
					true,
					{
						"m" : "queries",
						"website" :  website,
						"test" : $("#query-test-text").val()
					},
					"id",
					"query"
			);
		
		return false;
	});
	
// CLICK EVENT: update query
	$(document).on("click", "a.query-update", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#query-select").val(), 10);
		
		runCmd(
				"updatequery",
				{
					"id" : id,
					"name" : $("#query-name").val(),
					"query" : $("#query-text").val(),
					"type" : $("#query-type-select").val(),
					"resultbool": $("#query-result-bool").is(":checked"),
					"resultsingle" : $("#query-result-single").is(":checked"),
					"resultmulti" : $("#query-result-multi").is(":checked"),
					"textonly" : $("#query-text-only").is(":checked")
				},
				true,
				{
					"m" : "queries",
					"website" : website,
					"query" : id,
					"test" : $("#query-test-text").val()
				}
		);
		
		return false;
	});
	
// CLICK EVENT: delete query
	$(document).on("click", "a.query-delete", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#query-select").val(), 10);
		
		if(id) {
			runCmd(
					"deletequery",
					{ "id" : id },
					true,
					{
						"m" : "queries",
						"website" : website,
						"test" :  $("#query-test-text").val()
					}
			);
		}
		
		return false;
	});
	
// CLICK EVENT: test query (or hide test result)
	$(document).on("click", "a.query-test", function() {
		if($("#query-test-text").is(":visible")) {
			$("#query-test-label").text("Test result:");
			$("#query-test-text").hide();
			$("#query-test-result").show();
			$("#query-test-result").val("Testing query...");
			$("a.query-test").text("Back to text");
			
			var timerStart = +new Date();
			var args = {
							"cmd" : "testquery",
							"query" : $("#query-text").val(),
							"type" : $("#query-type-select").val(),
							"resultbool": $("#query-result-bool").is(":checked"),
							"resultsingle" : $("#query-result-single").is(":checked"),
							"resultmulti" : $("#query-result-multi").is(":checked"),
							"textonly" : $("#query-text-only").is(":checked"),
							"text" : $("#query-test-text").val()
						};
			
			$.ajax({
				type: "POST",
				url: "http://localhost:8080",
				data: JSON.stringify(args, null, 1),
				contentType: "application/json; charset=utf-8",
				dataType: "json",
				success: function(data) {
					var timerEnd = +new Date();
					var resultText = "crawlserv responded (" + msToStr(timerEnd - timerStart) + ")\n";
					
					if(data["fail"])
						resultText += "ERROR: " + data["text"];
					else
						resultText += data["text"];
					
					$("#query-test-result").val(resultText);
				}
			}).fail(function() {
				$("#query-test-result").val("ERROR: Could not connect to server.");
			});
		}
		else if($("#query-test-result").is(":visible")) {
			$("#query-test-label").text("Test text:");
			$("#query-test-result").hide();
			$("#query-test-text").show();
			$("a.query-test").text("Test query");
		}
		
		return false;
	});

/*
 * EVENTS for CONFIGURATIONS (for CRAWLERS, PARSERS, EXTRACTORS and ANALYZERS)
 */

// CHANGE EVENT: configuration selected
	$("#config-select").on("change", function() {
		$(this).blur();
		
		if(typeof config !== "undefined") {
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var id = parseInt($(this).val(), 10);
			
			if(config.isConfChanged()) {
				if(confirm("Do you want to discard the changes to your current configuration?"))
					reload({
						"m" : $(this).data("m"),
						"website" : website,
						"config" : id,
						"mode" : $(this).data("mode")
					});
				else
					reenableInputs();
			}
			else
				reload({
					"m" : $(this).data("m"),
					"website" : website,
					"config" : id,
					"mode" : $(this).data("mode")
				});
			
			return false;
		}
		
		return true;
	});

// CHANGE EVENT: algorithm category selected (for ANALYZERS only)
	$("#algo-cat-select").on("change", function() {
		$(this).blur()
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var config_id = parseInt($("#config-select").val(), 10);
		var id = parseInt($(this).val(), 10);
		
		if(config.isConfChanged() || config.hasAlgoSettings()) {
			if(confirm("Do you want to discard the settings of your algorithm?")) {
				reload({
					"m" : "analyzers",
					"website" : website,
					"config" : config_id,
					"mode" : $(this).data("mode"),
					"algo_cat" : id
				});
			}
			else
				reenableInputs();
		}
		else
			reload({
				"m" :
				"analyzers",
				"website" : website,
				"config" : config_id,
				"mode" : $(this).data("mode"),
				"algo_cat": id,
				"algo_changed": id != algo.id
			});
		
		return false;
	});
	
// CHANGE EVENT: algorithm selected (for ANALYZERS only)
	$("#algo-select").on("change", function() {
		$(this).blur()
		disableInputs()
		
		var website = parseInt($("#website-select").val(), 10);
		var config_id = parseInt($("#config-select").val(), 10);
		var algo_cat = parseInt($("#algo-cat-select").val(), 10);
		var id = parseInt($(this).val(), 10);
		
		if(config.isConfChanged() || config.hasAlgoSettings()) {
			if(confirm("Do you want to discard the settings of your algorithm?")) {
				reload({
					"m" : "analyzers",
					"website" : website,
					"config" : config_id, 
					"mode" : $(this).data("mode"),
					"algo_cat" : algo_cat,
					"algo_id" : id,
					"algo_changed": id != algo.id
				});
			}
			else
				reenableInputs();
		}
		else reload({
			"m" : "analyzers",
			"website" : website,
			"config" : config_id,
			"mode" : $(this).data("mode"),
			"algo_id": id
		});
		
		return false;
	});

// CLICK EVENT: change mode
	$(document).on("click", "a.post-redirect-mode", function() {
		$("#website-select").prop("disabled", true);
		$("#config-select").prop("disabled", true);
		$("#algo-select").prop("disabled", true);
		$("#algo-cat-select").prop("disabled", true);
		
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#config-select").val(), 10);
		var algo_id = parseInt($("#algo-select").val(), 10);
		var algo_cat = parseInt($("#algo-cat-select").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"mode" : $(this).data("mode"),
			"website" : website,
			"config" : id,
			"algo_id" : algo_id,
			"algo_cat" : algo_cat,
			"current" : JSON.stringify(config.updateConf()),
			"name" : $("#config-name").val()
		});
		
		return false;
	});

// CLICK EVENT: show segment body
	$(document).on("click", "div.segment-head-closed", function() {
		var block = $("div.segment-body-closed[data-block=\""
				+ $(this).data("block") + "\"]");
		block.toggleClass("segment-body-closed segment-body-open");
		
		$("span.segment-arrow[data-block=\"" + $(this).data("block") + "\"]").html("&dArr;");
		$(this).toggleClass("segment-head-closed segment-head-open");
		
		$("#container").scrollTop(block.offset().top + block.height())
		
		return true;
	});

// CLICK EVENT: hide segment body
	$(document).on("click", "div.segment-head-open", function() {
		var block = $("div.segment-body-open[data-block=\""
				+ $(this).data("block") + "\"]");
		block.toggleClass("segment-body-open segment-body-closed");
		
		$("span.segment-arrow[data-block=\"" + $(this).data("block") + "\"]").html("&rArr;");
		$(this).toggleClass("segment-head-open segment-head-closed");
		
		return true;
	});

// CLICK EVENT: add configuration
	$(document).on("click", "a.config-add", function() {
		var website = parseInt($("#website-select").val(), 10);
		var name = $("#config-name").val();
		var json = JSON.stringify(config.updateConf());
		
		runCmd(
				"addconfig",
				{
					"website" : website,
					"module" : $(this).data("module"),
					"name" : name,
					"config" : json
				},
				true,
				{
					"m" : $(this).data("m"),
					"website" : website,
					"mode" : $(this).data("mode")
				},
				"id",
				"config"
		);
		return false;
	})
	
// CLICK EVENT: duplicate configuration
	$(document).on("click", "a.config-duplicate", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#config-select").val(), 10);
		var json = JSON.stringify(config.updateConf());
		
		runCmd(
				"duplicateconfig",
				{
					"id" : id
				},
				true,
				{
					"m" : $(this).data("m"),
					"website" :  website,
					"mode" : $(this).data("mode"),
					"current" : json
				},
				"id",
				"config"
		);
		
		return false;
	});
	
// CLICK EVENT: update configuration
	$(document).on("click", "a.config-update", function() {
		var id = parseInt($("#config-select").val(), 10);
		var website = parseInt($("#website-select").val(), 10);
		var name = $("#config-name").val();
		var json = JSON.stringify(config.updateConf());
		
		runCmd(
				"updateconfig",
				{
					"id" : id,
					"website" : website,
					"module" : $(this).data("module"),
					"name" : name,
					"config" : json
				},
				true,
				{
					"m" : $(this).data("m"),
					"website" : website,
					"mode" : $(this).data("mode"),
					"config" : id
				}
		);
		return false;
	})
	
// CLICK EVENT: delete configuration
	$(document).on("click", "a.config-delete", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#config-select").val(), 10);
		
		if(id) {
			runCmd(
					"deleteconfig",
					{
						"id" : id
					},
					true,
					{
						"m" : $(this).data("m"),
						"website" : website,
						"test" :  $("#query-test-text").val()
					}
			);
		}
		return false;
	});
	
// CLICK EVENT: add item to array
	$(document).on("click", "input.opt-array-add", function() {
		config.onAddElement($(this).data("cat"), $(this).data("id"));
		return true;
	});
	
// CLICK EVENT: remove item from array
	$(document).on("click", "input.opt-array-del", function() {
		config.onDelElement($(this).data("cat"), $(this).data("id"), $(this).data("item"));
		return true;
	});
	
// CLICK EVENT: remove placeholder on click
	$(document).on("click", "input.opt", function() {
		$(this).removeAttr("placeholder");
		return true;
	});

/*
 * EVENTS for THREADS
 */
	
// CHANGE EVENT: module selected
	$("#module-select").on("change", function() {
		var website = parseInt($("#website-select").val(), 10);
		
		disableInputs();
		
		reload({
			"m" : $(this).data("m"),
			"website" : website,
			"module": $(this).val(),
			"scrolldown": $(this).data("scrolldown") !== "undefined"
		});
		
		return false;
	});

// CLICK EVENT: pause thread
	$(document).on("click", "a.thread-pause", function() {
		var id = parseInt($(this).data("id"), 10);
		var module = $(this).data("module");
		
		runCmd(
				"pause" + module,
				{ "id" : id },
				false
		);
		
		return false;
	})
	
// CLICK EVENT: unpause thread
	$(document).on("click", "a.thread-unpause", function() {
		var id = parseInt($(this).data("id"), 10);
		var module = $(this).data("module"); 
		
		runCmd(
				"unpause" + module,
				{ "id" : id },
				false
		);
		
		return false;
	})
	
// CLICK EVENT: stop thread
	$(document).on("click", "a.thread-stop", function() {
		var id = parseInt($(this).data("id"), 10);
		var module = $(this).data("module");
		
		runCmd(
				"stop" + module,
				{ "id" : id },
				false
		);
		return false;
	})

// CLICK EVENT: start thread
	$(document).on("click", "a.thread-start", function() {
		var website = parseInt($("#website-select").val(), 10);
		var module = $("#module-select").val();
		var config = parseInt($("#config-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		
		runCmd(
				"start" + module,
				{
					"website" : website,
					"config": config,
					"urllist": urllist
				},
				false
		);
		
		return false;
	})
	
// CLICK EVENT: time travel (jump to an ID)
	$(document).on("click", "span.remaining", function() {
		var id = $(this).data("id");
		var module = $(this).data("module");
		var last = $(this).data("last");
		
		if(module != "analyzer") {
			var target = parseInt(prompt("! WARNING ! Time travel can have unintended consequences.\n\nEnter ID to jump to:", last), 10);
			
			if(target)
				runCmd(
						"warp",
						{
							"thread" : id,
							"target": target
						},
						false
				);
		}
	})
	
/*
 * EVENTS for CONTENT
 */
	
// CLICK EVENT: change tab
	$(document).on("click", "a.post-redirect-tab", function() {
		$("#website-select").prop("disabled", true);
		$("#urllist-select").prop("disabled", true);
		$("#content-last").prop("disabled", true);
		$("#content-url").prop("disabled", true);
		$("#content-url-text").prop("disabled", true);
		$("#content-next").prop("disabled", true);
		$("#content-version").prop("disabled", true);
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url
		});
		
		return false;
	});
	
// CLICK EVENT: last URL
	$(document).on("click", "#content-last", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url,
			"last": true
		});
		
		return false;
	});
	
// CLICK EVENT: next URL
	$(document).on("click", "#content-next", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url + 1
		});
		
		return false;
	});

// CHANGE EVENT: input URL ID (numbers only)
	$(document).on("input keydown keyup mousedown mouseup select contextmenu drop", "#content-url", function() {
		var input = $(this).val();
		
		if(!input.length || isNormalInteger(input)) {
	        this.oldValue = this.value;
	        this.oldSelectionStart = this.selectionStart;
	        this.oldSelectionEnd = this.selectionEnd;
		}
		else if(this.hasOwnProperty("oldValue")) {
	        this.value = this.oldValue;
	        this.setSelectionRange(this.oldSelectionStart, this.oldSelectionEnd);
		}
		
		return true;
	});
	
// KEYPRESS EVENT: input URL ID (ENTER)
	$(document).on("keypress", "#content-url", function(e) {
		if(e.which == 13) {
			if(!$(this).val().length)
				return true;
			
			disableInputs();
	
			var website = parseInt($("#website-select").val(), 10);
			var urllist = parseInt($("#urllist-select").val(), 10);
			var url = parseInt($(this).val(), 10);
			
			reload({
				"m" : $(this).data("m"),
				"tab" : $(this).data("tab"),
				"website" : website,
				"urllist" : urllist,
				"url": url
			});
			
			return false;
		}
		
		return true;
	});
	
// KEYPRESS EVENT: input URL text (ENTER)
	$(document).on("keypress", "#content-url-text", function(e) {
		if(e.which == 13) {
			if(!$(this).val().length)
				return true;
			
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var urllist = parseInt($("#urllist-select").val(), 10);
			var urltext = $(this).val();
			
			reload({
				"m" : $(this).data("m"),
				"tab" : $(this).data("tab"),
				"website" : website,
				"urllist" : urllist,
				"urltext": urltext
			});
			return false;
		}
		return true;
	});
	
// CLICK EVENT: fullscreen
	$(document).on("click", "#content-fullscreen", function() {
		if(fullscreen) {
			fullscreen = false;
			
			var temp = $(".fs-div").detach();
			
			temp.insertAfter($(".fs-insert-after"));
			
			$(".fs-content").css({
				"display" : this.oldDisplay,
				"width" : this.oldCodeWidth,
				"height" : this.oldCodeHeight
			});
			
			temp.css({
				"position" : this.oldPos,
				"width" : this.oldWidth,
				"height" : this.oldHeight,
				"margin-left": this.oldMargin
			});
			
			$(this).css({
				"position" : "absolute",
				"right" : this.oldBtnPos
			});
			
			$(this).html("&#9727;");
			$(this).prop("title", "Show Fullscreen");
		}
		else {
			fullscreen = true;
			
			this.oldPos = $(".fs-div").css("position");
			this.oldWidth = $(".fs-div").css("width");
			this.oldHeight = $(".fs-div").css("height");
			this.oldMargin = $(".fs-div").css("margin-left");
			this.oldCodeWidth = $(".fs-content").css("width");
			this.oldCodeHeight = $(".fs-content").css("height");
			this.oldBtnPos = $(this).css("right");
			
			var temp = $(".fs-div").detach();
			temp.appendTo("body");
			
			temp.css({
				"position" : "absolute",
				"width" : "100%",
				"height" : "100%",
				"margin-left" : "5px",
				"z-index": "2"
			});
			
			$(".fs-content").css({
				"width" : "100%",
				"height" : "auto"
			});
			
			$(this).css({"position" : "fixed", "right" : "30px"});
			$(this).html("&ultri;");
			$(this).prop("title", "Exit Fullscreen [ESC]");
		}
	});
	
// CHANGE EVENT: select content version
	$(document).on("change", "#content-version", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		var version = parseInt($(this).val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url,
			"version": version
		});
		
		return false;
	});
	
// CLICK EVENT: download content
	$(document).on("click", "#content-download", function() {
		var contentwebsiteid = parseInt($("#website-select").val(), 10);
		var contentwebsitename = $("#website-select option:selected").text();
		var contenturlid = parseInt($("#content-url").val(), 10);
		var contenturl = "/" + $("#content-url-text").val();
		var contentwebsite = $(this).data("website-namespace");
		var contentnamespace = $(this).data("namespace");
		var contentversion = $(this).data("version");
		var contenttype = $(this).data("type");
		var contentfilename = $(this).data("filename");
		
		var args = {
						"type": contenttype,
						"namespace": contentnamespace,
						"website": contentwebsite,
						"version": contentversion,
						"w_id": contentwebsiteid,
						"w_name" : contentwebsitename,
						"u_id": contenturlid,
						"url": contenturl,
						"filename": contentfilename
		};
		
		$.post("download/", args, function() {
			window.open("download/");
		});
		
		return false;
	});
	
/*
 * 
 */
	
});
