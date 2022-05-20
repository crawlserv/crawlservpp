
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
 *  ---
 *  
 * frontend.js
 * 
 * Main JavaScript code for the PHP/JavaScript crawlserv++ frontend.
 * 
 * See https://github.com/crawlserv/crawlservpp/ for the latest version of the application.
 * 
 */

// @requires helpers.js !

// is page unloading?
var crawlserv_frontend_unloading = false;

// detached helpers
var helperRegEx = null;
var helperXPath = null;
var helperJSONPointer = null;
var helperJSONPath = null;
var helperCombined = null;

// detached options
var optionUrlListName = null;
var optionUrlListNamespace = null;

// fullscreen mode
var fullscreen = false;

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
		if(redirected) {
			$("#redirect-time").text(msToStr(+new Date() - localStorage["_crawlserv_leavetime"]));
		}
		
		// prepare fullscreen if necessary
		if(!$(".fs-insert-after") && $("#content-next").length) {
			$("#content-next").addClass("fs-insert-after");
		}
		
		// check query type
		if($("#query-type-select").length) {
			if($("#query-type-select").val() == "regex") {
				if(!helperXPath) {
					helperXPath = $("#xpath-helper").detach();
				}
				
				if(!helperJSONPointer) {
					helperJSONPointer = $("#jsonpointer-helper").detach();
				}
				
				if(!helperJSONPath) {
					helperJSONPath = $("#jsonpath-helper").detach();
				}
				
				if(!helperCombined) {
					helperCombined = $("#combined-helper").detach();
				}
				
				$("#query-text-only").prop("checked", true);
				$("#query-text-only").prop("disabled", true);
				$("#query-text-only-label").addClass("check-label-disabled");
				
				$("#query-xml-warnings").prop("checked", false);
				$("#query-xml-warnings").prop("disabled", true);
				$("#query-xml-warnings-label").addClass("check-label-disabled");
				
				$("#query-fix-cdata").prop("checked", false);
				$("#query-fix-cdata").prop("disabled", true);
				$("#query-fix-cdata-label").addClass("check-label-disabled");
				
				$("#query-fix-comments").prop("checked", false);
				$("#query-fix-comments").prop("disabled", true);
				$("#query-fix-comments-label").addClass("check-label-disabled");
				
				$("#query-remove-xml").prop("checked", false);
				$("#query-remove-xml").prop("disabled", true);
				$("#query-remove-xml-label").addClass("check-label-disabled");
			}
			else if($("#query-type-select").val() == "xpath") {
				if(!helperRegEx) {
					helperRegEx = $("#regex-helper").detach();
				}
				
				if(!helperJSONPointer) {
					helperJSONPointer = $("#jsonpointer-helper").detach();
				}
				
				if(!helperJSONPath) {
					helperJSONPath = $("#jsonpath-helper").detach();
				}
				
				if(!helperCombined) {
					helperCombined = $("#combined-helper").detach();
				}
			}
			else if($("#query-type-select").val() == "jsonpointer") {
				if(!helperRegEx) {
					helperRegEx = $("#regex-helper").detach();
				}
				
				if(!helperXPath) {
					helperXPath = $("#xpath-helper").detach();
				}
				
				if(!helperJSONPath) {
					helperJSONPath = $("#jsonpath-helper").detach();
				}
				
				if(!helperCombined) {
					helperCombined = $("#combined-helper").detach();
				}
				
				$("#query-text-only").prop("checked", false);
				
				$("#query-xml-warnings").prop("checked", false);
				$("#query-xml-warnings").prop("disabled", true);
				$("#query-xml-warnings-label").addClass("check-label-disabled");
				
				$("#query-fix-cdata").prop("checked", false);
				$("#query-fix-cdata").prop("disabled", true);
				$("#query-fix-cdata-label").addClass("check-label-disabled");
				
				$("#query-fix-comments").prop("checked", false);
				$("#query-fix-comments").prop("disabled", true);
				$("#query-fix-comments-label").addClass("check-label-disabled");
				
				$("#query-remove-xml").prop("checked", false);
				$("#query-remove-xml").prop("disabled", true);
				$("#query-remove-xml-label").addClass("check-label-disabled");
			}
			else if($("#query-type-select").val() == "jsonpath") {
				if(!helperRegEx) {
					helperRegEx = $("#regex-helper").detach();
				}
				
				if(!helperXPath) {
					helperXPath = $("#xpath-helper").detach();
				}
				
				if(!helperJSONPointer) {
					helperJSONPointer = $("#jsonpointer-helper").detach();
				}
				
				if(!helperCombined) {
					helperCombined = $("#combined-helper").detach();
				}
				
				$("#query-text-only").prop("checked", false);
				
				$("#query-xml-warnings").prop("checked", false);
				$("#query-xml-warnings").prop("disabled", true);
				$("#query-xml-warnings-label").addClass("check-label-disabled");
				
				$("#query-fix-cdata").prop("checked", false);
				$("#query-fix-cdata").prop("disabled", true);
				$("#query-fix-cdata-label").addClass("check-label-disabled");
				
				$("#query-fix-comments").prop("checked", false);
				$("#query-fix-comments").prop("disabled", true);
				$("#query-fix-comments-label").addClass("check-label-disabled");
				
				$("#query-remove-xml").prop("checked", false);
				$("#query-remove-xml").prop("disabled", true);
				$("#query-remove-xml-label").addClass("check-label-disabled");
			}
			else if($("#query-type-select").val() == "xpathjsonpointer") {
				if(!helperRegEx) {
					helperRegEx = $("#regex-helper").detach();
				}
				
				if(!helperJSONPointer) {
					helperJSONPointer = $("#jsonpointer-helper").detach();
				}
				
				if(!helperJSONPath) {
					helperJSONPath = $("#jsonpath-helper").detach();
				}
				
				$("#query-text-only").prop("checked", false);
			}
			else if($("#query-type-select").val() == "xpathjsonpath") {
				if(!helperRegEx) {
					helperRegEx = $("#regex-helper").detach();
				}
				
				if(!helperJSONPointer) {
					helperJSONPointer = $("#jsonpointer-helper").detach();
				}
				
				if(!helperJSONPath) {
					helperJSONPath = $("#jsonpath-helper").detach();
				}
				
				$("#query-text-only").prop("checked", false);
			}
		}
		
		// check query date test
		if($("#query-datetime").length && $("#query-datetime").is(":checked")) {
			// add locales to selection if necessary
			if($("#query-datetime-locale option").length == 1) {
				for(const locale of db_locales) {
					$("#query-datetime-locale").append(
							"\n<option value=\"" + locale + "\">" + locale + "</option>"
					);
				}
			}
			
			// show date-related options
			$("#query-datetime-format-label").show();
			$("#query-datetime-format").show();
			$("#query-datetime-locale-label").show();
			$("#query-datetime-locale").show();
		}
		
	    // enable tippy for corpus control characters
	    tippy(
	    		"span.content-corpus-control",
	    		{ delay : 0, duration: 0, arrow : true, placement: "left-start", size : "small" }
	    );
		
		// check import/export inputs
		disableImportExport();
		
		if($("#urllist-target").val() != 0) {
			if(!optionUrlListName) {
				optionUrlListName = $("#urllist-name-div").detach();
			}
			
			if(!optionUrlListNamespace) {
				optionUrlListNamespace = $("#urllist-namespace-div").detach();
			}
		}
		
		if($("#compression-select").length) {
			$("#website-select").data("compression", $("#compression-select").val());
		}
		
		// prepare form for file upload
		$("#file-form").ajaxForm();
		
		// activate jQuery datepicker and enable only the dates in the current corpus
		if(typeof corpusDates != "undefined" && corpusDates.length > 0) {
			$("#corpus-date").datepicker({
				changeMonth: true,
				changeYear: true,
				constrainInput: true,
				dateFormat: 'yy-mm-dd',
				minDate: corpusDates[0],
				maxDate: corpusDates[corpusDates.length - 1],
				beforeShowDay: function(date) {
						var sdate = $.datepicker.formatDate("yy-mm-dd", date);
						
						if($.inArray(sdate, corpusDates) != -1) {
							return [true];
						}
						
						return [false];
				},
				onSelect: function(dateText) {
					var website = parseInt($("#website-select").val(), 10);
					var urllist = parseInt($("#urllist-select").val(), 10);
					var corpus = parseInt($("#corpus-select").val(), 10);
					
					reload({
						"m" : $(this).data("m"),
						"tab" : $(this).data("tab"),
						"website" : website,
						"urllist" : urllist,
						"corpus": corpus,
						"corpus_date": dateText
					});
				}
			});
		}
		
		// set timer
		refreshData();
		
		// enable CORS
		$.support.cors = true;
	});
	
	// DOCUMENT UNLOAD
	$(window).on("beforeunload", function() {
		// set status
		crawlserv_frontend_unloading = true;
		
		// set leave time
		localStorage["_crawlserv_leavetime"] = +new Date();
	});
	
	// DOCUMENT KEYUP EVENT: exit fullscreen on ESC
	$(document).on("keyup", function(e) {
		if(
				e.keyCode === 27
				&& $("#content-fullscreen").length
				&& fullscreen
		) {
			$("#content-fullscreen").click();
			
			return false;
		}
		
		return true;
	});
	
	// CLICK EVENT: navigation
	$(document).on("click", "a.post-redirect", function() {		
		var args = {
				"m" : $(this).data("m")
		};
		
		if(global_website > 0) {
			args.website = global_website;
		}
		else if($("#website-select").length && $("#website-select").value > 0) {
			args.website = $("#website-select").value;
		}
		else if(typeof $(this).data("website") !== "undefined" && $(this).data("website") > 0) {
			args.website = $(this).data("website");
		}
		
		if(typeof $(this).data("mode") !== "undefined") {
			args.mode = $(this).data("mode");
		}
		
		reload(args);
		
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
  				{
  					"m" : $(this).data("m") 
  				}
  		);
  		
		return false;
	});
	
	// KEYDOWN EVENT: trigger event on CTRL+ENTER (text fields)
	$(document).on(
			"keydown",
			"input.trigger[type='text'], input.trigger[type='number'], input.trigger[type='date'], textarea.trigger",
			function(event) {
				if((event.keyCode == 10 || event.keyCode == 13) && event.ctrlKey) {
					if(hasData($(this), "trigger")) {
						$("#" + $(this).data("trigger")).trigger("click");
					}
				}
			}
	);
	
	// CHANGE EVENT: check date
	$("input[type='date']").on("change", function() {
		if(isValidDate($(this).val())) {
			return true;
		}
		
		alert("Please enter an empty or a valid date in the format \'YYYY-MM-DD\'.");
		
		return false;
	});

	/*
	 * EVENTS FOR SERVER
	 */
	
	// CLICK EVENT: add allowed IP
	$("#cmd-allow").on("click", function() {
	    runCmd(
	    		"allow",
	    		{
	    			"ip" : document.getElementById("cmd-allow-form-ip").value
	    		},
	    		false,
	    		null
	    );
	    
		return false;
	});

	// CLICK EVENT: add argument to custom command
	$("#cmd-custom-addarg").on("click", function() {
  		var argId = 0;
  		var argTotal = $("#cmd-args .cmd-custom-arg").length;
  		
  		if(argTotal > 0) {
  			argId = parseInt($("#cmd-args .cmd-custom-arg").last().data("n"), 10) + 1;
  		}
  		
  		$("#cmd-args").append(
  				"<div class=\"cmd-custom-arg entry-row\" data-n=\"" + argId + "\">" +
  				"<div class=\"entry-label\">Argument " +
  				"<div class=\"cmd-custom-arg-no\" style=\"display:inline-block\">" + (argTotal + 1) + "</div>:</div>" +
  				"<div class=\"entry-halfx-input\"><input type=\"text\" class=\"entry-halfx-input cmd-custom-arg-name\" /></div>" +
  				"<div class=\"entry-separator\">=</div>" +
  				"<div class=\"entry-halfx-input\"><input type=\"text\" class=\"entry-halfx-input cmd-custom-arg-value\" /></div>" +
  				"<a href=\"#\" class=\"action-link cmd-custom-remarg\" data-n=\"" + argId + "\">" +
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
  		
  		if(toRemove) {
  			toRemove.remove();
  		}
  		
  		return false;
  	});

	// CLICK EVENT: run custom command with custom arguments
	$("#cmd-custom").on("click", function() {
  		var args = {};
  		$("#cmd-args .cmd-custom-arg").each(function(i, obj) {
  			var argname = $(this).find(".cmd-custom-arg-name").val();
  			var argvalue = $(this).find(".cmd-custom-arg-value").val();
  			
  			if(!isNaN(argvalue)) {
  				argvalue = +argvalue;
  			}
  			else if(argvalue == "true") {
  				argvalue = true;
  			}
  			else if(argvalue == "false") {
  				argvalue = false;
  			}
  			
  			if(argname && argname.length) {
  				args[argname] = argvalue;
  			}
  		});
  		
		runCmd(document.getElementById("cmd-custom-form-cmd").value, args, false, null);
		
		return false;
	});

	/*
	 * EVENTS FOR LOGS
	 */
	
	// CLICK EVENT: navigate log entries
	$("a.logs-nav").on("click", function() {
		reload({
			"m" : "logs",
			"filter" : $(this).data("filter"),
			"offset" : $(this).data("offset"),
			"limit" : $(this).data("limit")
		});
		
		return false;
	});

	// CLICK EVENT: clear log entries
	$("a.logs-clear").on("click", function() {
		runCmd(
				"clearlogs",
				{
					"module" : $(this).data("filter")
				},
				true,
				{
					"m": "logs",
					"filter" : $(this).data("filter")
				}
		);
		
		return false;
	});
	
	// DOUBLE/MIDDLE CLICK EVENT: copy log entry
	$("select.content-list > option").on("dblclick auxclick", function(event) {
		if(event.type == "auxclick" && event.which != 2) {
			// only middle mouse button
			return true;
		}
		
		let value = $(this).val();
		
		if(navigator.clipboard) {		
			navigator.clipboard.writeText(value).then(
					function() {
						alert("This log entry has been written to the clipboard:\n\n" + value);
					},
					function() {
						console.log("Could not write to clipboard.");
						
						alert(value);
					}
			);
		}
		else {
			// insecure connection -> show as alert only
			alert(value);
		}
	});

	/*
	 * EVENTS FOR WEBSITES
	 */
	
	// CHANGE EVENT: website selected
	$("#website-select").on("change", function() {
		$(this).blur();
		
		disableInputs();
		
		var id = parseInt($(this).val(), 10);
		var args = {
				"m" : $(this).data("m"), 
				"website" : id
		};
		
		if($("#query-test-text").length) {
			args["test"] = $("#query-test-text").val();
		}
		
		if(!$("#query-new-tab").is(":checked")) {
			args["test-nonewtab"] = true;
		}
		
		if(typeof $(this).data("tab") !== "undefined") {
			args["tab"] = $(this).data("tab");
		}
		
		if(typeof $(this).data("mode") !== "undefined") {
			args["mode"] = $(this).data("mode");
		}
		
		if(typeof $(this).data("module") !== "undefined") {
			args["module"] = $(this).data("module");
		}
		
		if(typeof $(this).data("action") !== "undefined") {
			args["action"] = $(this).data("action");
		}
		
		if(typeof $(this).data("datatype") !== "undefined") {
			args["datatype"] = $(this).data("datatype");
		}
		
		if(typeof $(this).data("filetype") !== "undefined") {
			args["filetype"] = $(this).data("filetype");
		}
		
		if(typeof $(this).data("compression") !== "undefined") {
			args["compression"] = $(this).data("compression");
		}
		
		if(typeof $(this).data("scrolldown") !== "undefined") {
			args["scrolldown"] = true;
		}
		
		if(typeof config !== "undefined") {
			if(config.isConfChanged()) {
				if(confirm("Do you want to discard the changes to your current configuration?")) {
					reload(args);
				}
				else {
					reenableInputs();
				}
			}
			else {
				reload(args);
			}
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
			
			if(typeof $(this).data("tab") !== "undefined") {
				args["tab"] = $(this).data("tab");
			}
			
			if($("#urls-delete-query").length) {
				args["query"] = parseInt($("#urls-delete-query").val(), 10);
			}
			
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
	$("#website-add").on("click", function() {
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
				{
					"m" : "websites"
				},
				"id",
				"website"
		);
		
		return false;
	});
	
	// CLICK EVENT: duplicate website
	$("#website-duplicate").on("click", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"duplicatewebsite",
				{
					"id" : id,
					"queries": queries
				},
				true,
				{
					"m" : "websites"
				},
				"id",
				"website"
		);
		
		return false;
	});
	
	// CLICK EVENT: update website
	$("#website-update").on("click", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		runCmd(
				"updatewebsite",
				{
					"id" : id,
					"name" : $("#website-name").val(),
					"namespace" : $("#website-namespace").val(),
					"crossdomain": $("#website-crossdomain").prop("checked"),
					"domain" : $("#website-crossdomain").prop("checked") ? undefined : $("#website-domain").val(),
					"dir" : $("#website-externaldir").prop("checked") ? $("#website-dir").val() : undefined
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
	$("#website-delete").on("click", function() {
		var id = parseInt($("#website-select").val(), 10);
		
		if(id) {
			runCmd(
					"deletewebsite",
					{
						"id" : id
					},
					true,
					{
						"m" : "websites"
					}
			);
		}
		
		return false;
	});

	// CLICK EVENT: add URL list
	$("#urllist-add").on("click", function() {
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
	$("#urllist-update").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
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
					"urllist" : id,
					"query" : query
				}
		);
		
		return false;
	});

	// CLICK EVENT: delete URL list
	$("#urllist-delete").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
		if(id) {
			runCmd(
					"deleteurllist",
					{
						"id" : id
					},
					true,
					{
						"m" : "websites",
						"website" : website,
						"query" : query
					}
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: download URL list
	$("#urllist-download").on("click", function() {
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
					if(!window.open("download/", "crawlserv_download", false)) {
						console.error(
								"Could not open a new window, it has probably been blocked by a popup blocker."
						);
						
						alert(
								"Could not open a new window, it has probably been blocked by a popup blocker."		
						);
					}
				}
		);
		
		return false;
	});
	
	// CLICK EVENT: reset parsing status
	$("#urllist-reset-parsing").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
		if(urllist > 0) {
			runCmd(
					"resetparsingstatus",
					{
						"urllist" : urllist
					},
					true,
					{
						"m" : "websites",
						"website" : website,
						"urllist" : urllist,
						"query" : query
					}
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: reset extracting status
	$("#urllist-reset-extracting").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
		if(urllist > 0) {
			runCmd(
					"resetextractingstatus",
					{
						"urllist" : urllist
					},
					true,
					{
						"m" : "websites",
						"website" : website,
						"urllist" : urllist,
						"query" : query
					}
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: reset analyzing status
	$("#urllist-reset-analyzing").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
		if(urllist > 0) {
			runCmd(
					"resetanalyzingstatus", 
					{
						"urllist" : urllist
					},
					true,
					{
						"m" : "websites",
						"website" : website,
						"urllist" : urllist,
						"query" : query
					}
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: delete URLs from URL list
	$("#urls-delete").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var query = parseInt($("#urls-delete-query").val(), 10);
		
		if(urllist > 0) {
			runCmd(
					"deleteurls", 
					{
						"urllist" : urllist,
						"query" : query
					},
					true,
					{
						"m" : "websites",
						"website" : website,
						"urllist" : urllist,
						"query" : query
					}
			);
		}
		
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
					"test" : $("#query-test-text").val(),
					"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
				}
		);
		
		return false;
	});
	
	// CHANGE EVENT: query type selected
	$("#query-type-select").on("change", function() {
		if($(this).val() == "regex") {
			$("#query-text-only").prop("checked", true);
			$("#query-text-only").prop("disabled", true);
			$("#query-text-only-label").addClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("checked", false);
			$("#query-xml-warnings").prop("disabled", true);
			$("#query-xml-warnings-label").addClass("check-label-disabled");
			
			$("#query-fix-cdata").prop("checked", false);
			$("#query-fix-cdata").prop("disabled", true);
			$("#query-fix-cdata-label").addClass("check-label-disabled");
			
			$("#query-fix-comments").prop("checked", false);
			$("#query-fix-comments").prop("disabled", true);
			$("#query-fix-comments-label").addClass("check-label-disabled");
			
			$("#query-remove-xml").prop("checked", false);
			$("#query-remove-xml").prop("disabled", true);
			$("#query-remove-xml-label").addClass("check-label-disabled");
			
			if(helperRegEx) {
				helperRegEx.insertAfter("#query-properties");
				
				helperRegEx = null;
			}
			
			if(!helperXPath) {
				helperXPath = $("#xpath-helper").detach();
			}
			
			if(!helperJSONPointer) {
				helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			
			if(!helperJSONPath) {
				helperJSONPath = $("#jsonpath-helper").detach();
			}
			
			if(!helperCombined) {
				helperCombined = $("#combined-helper").detach();
			}
		}
		else if($(this).val() == "xpath") {
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only").prop("checked", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("disabled", false);
			$("#query-xml-warnings-label").removeClass("check-label-disabled");
			
			$("#query-fix-cdata").prop("checked", true);
			$("#query-fix-cdata").prop("disabled", false);
			$("#query-fix-cdata-label").removeClass("check-label-disabled");
			
			$("#query-fix-comments").prop("checked", true);
			$("#query-fix-comments").prop("disabled", false);
			$("#query-fix-comments-label").removeClass("check-label-disabled");
			
			$("#query-remove-xml").prop("checked", true);
			$("#query-remove-xml").prop("disabled", false);
			$("#query-remove-xml-label").removeClass("check-label-disabled");
			
			if(helperXPath) {
				helperXPath.insertAfter("#query-properties");
				
				helperXPath = null;
			}
			
			if(!helperRegEx) {
				helperRegEx = $("#regex-helper").detach();
			}
			
			if(!helperJSONPointer) {
				helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			
			if(!helperJSONPath) {
				helperJSONPath = $("#jsonpath-helper").detach();
			}
			
			if(!helperCombined) {
				helperCombined = $("#combined-helper").detach();
			}
		}
		else if($(this).val() == "jsonpointer") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("checked", false);
			$("#query-xml-warnings").prop("disabled", true);
			$("#query-xml-warnings-label").addClass("check-label-disabled");
			
			$("#query-fix-cdata").prop("checked", false);
			$("#query-fix-cdata").prop("disabled", true);
			$("#query-fix-cdata-label").addClass("check-label-disabled");
			
			$("#query-fix-comments").prop("checked", false);
			$("#query-fix-comments").prop("disabled", true);
			$("#query-fix-comments-label").addClass("check-label-disabled");
			
			$("#query-remove-xml").prop("checked", false);
			$("#query-remove-xml").prop("disabled", true);
			$("#query-remove-xml-label").addClass("check-label-disabled");
			
			if(helperJSONPointer) {
				helperJSONPointer.insertAfter("#query-properties");
				
				helperJSONPointer = null;
			}
			
			if(!helperRegEx) {
				helperRegEx = $("#regex-helper").detach();
			}
			
			if(!helperXPath) {
				helperXPath = $("#xpath-helper").detach();
			}
			
			if(!helperJSONPath) {
				helperJSONPath = $("#jsonpath-helper").detach();
			}
			
			if(!helperCombined) {
				helperCombined = $("#combined-helper").detach();
			}
		}
		else if($(this).val() == "jsonpath") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("checked", false);
			$("#query-xml-warnings").prop("disabled", true);
			$("#query-xml-warnings-label").addClass("check-label-disabled");
			
			$("#query-fix-cdata").prop("checked", false);
			$("#query-fix-cdata").prop("disabled", true);
			$("#query-fix-cdata-label").addClass("check-label-disabled");
			
			$("#query-fix-comments").prop("checked", false);
			$("#query-fix-comments").prop("disabled", true);
			$("#query-fix-comments-label").addClass("check-label-disabled");
			
			$("#query-remove-xml").prop("checked", false);
			$("#query-remove-xml").prop("disabled", true);
			$("#query-remove-xml-label").addClass("check-label-disabled");
			
			if(helperJSONPath) {
				helperJSONPath.insertAfter("#query-properties");
				
				helperJSONPath = null;
			}
			
			if(!helperRegEx) {
				helperRegEx = $("#regex-helper").detach();
			}
			
			if(!helperXPath) {
				helperXPath = $("#xpath-helper").detach();
			}
			
			if(!helperJSONPointer) {
				helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			
			if(!helperCombined) {
				helperCombined = $("#combined-helper").detach();
			}
		}
		else if($(this).val() == "xpathjsonpointer") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("disabled", false);
			$("#query-xml-warnings-label").removeClass("check-label-disabled");
			
			$("#query-fix-cdata").prop("checked", true);
			$("#query-fix-cdata").prop("disabled", false);
			$("#query-fix-cdata-label").removeClass("check-label-disabled");
			
			$("#query-fix-comments").prop("checked", true);
			$("#query-fix-comments").prop("disabled", false);
			$("#query-fix-comments-label").removeClass("check-label-disabled");
			
			$("#query-remove-xml").prop("checked", true);
			$("#query-remove-xml").prop("disabled", false);
			$("#query-remove-xml-label").removeClass("check-label-disabled");
			
			if(helperXPath) {
				helperXPath.insertAfter("#query-properties");
				
				helperXPath = null;
			}
			
			if(helperCombined) {
				helperCombined.insertAfter("#xpath-helper");
				
				helperCombined = null;
			}
			
			if(!helperRegEx) {
				helperRegEx = $("#regex-helper").detach();
			}
			
			if(!helperJSONPointer) {
				helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			
			if(!helperJSONPath) {
				helperJSONPath = $("#jsonpath-helper").detach();
			}
		}
		else if($(this).val() == "xpathjsonpath") {
			$("#query-text-only").prop("checked", false);
			$("#query-text-only").prop("disabled", false);
			$("#query-text-only-label").removeClass("check-label-disabled");
			
			$("#query-xml-warnings").prop("disabled", false);
			$("#query-xml-warnings-label").removeClass("check-label-disabled");
			
			if(helperXPath) {
				helperXPath.insertAfter("#query-properties");
				
				helperXPath = null;
			}
			
			if(helperCombined) {
				helperCombined.insertAfter("#xpath-helper");
				
				helperCombined = null;
			}
			
			if(!helperRegEx) {
				helperRegEx = $("#regex-helper").detach();
			}
			
			if(!helperJSONPointer) {
				helperJSONPointer = $("#jsonpointer-helper").detach();
			}
			
			if(!helperJSONPath) {
				helperJSONPath = $("#jsonpath-helper").detach();
			}
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
	$("#xpath-generate").on("click", function() {
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
		
		if(
				$("#query-text").val().length
				&& $("#query-text").val().charAt(0) != '\n'
				&& !confirm("Do you want to override the existing query?")
		) {
			return false;
		}
		
		query = "//"
				+ $("#xpath-element").val()
				+ "[contains(concat(' ', normalize-space(@"
				+ $("#xpath-property").val()
				+ "), ' '), ' "
				+ $("#xpath-value").val() + " ')]";
		
		if(property) {
			query += "/@" + $("#xpath-result-property").val();
		}
		
		var lines = $("#query-text").val().split('\n');
		
		if(lines.length > 1) {
			lines.forEach(function(currentValue, index) {
				if(index > 0) {
					query += "\n" + currentValue;
				}
			});
		}
		
		$("#query-text").val(query);
		
		return false;
	});
	
	// CLICK EVENT: add query
	$("#query-add").on("click", function() {
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
					"resultsubsets" : $("#query-result-subsets").is(":checked"),
					"textonly" : $("#query-text-only").is(":checked") 
				},
				true,
				{
					"m" : "queries",
					"website" : website,
					"test" : $("#query-test-text").val(),
					"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
				},
				"id",
				"query"
		);
		
		return false;
	});
	
	// CLICK EVENT: duplicate query
	$("#query-duplicate").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#query-select").val(), 10);
		
		if(id > 0) {
			runCmd(
					"duplicatequery",
					{
						"id" : id
					},
					true,
					{
						"m" : "queries",
						"website" :  website,
						"test" : $("#query-test-text").val(),
						"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
					},
					"id",
					"query"
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: show options to move query to another website
	$("#query-move-toggle").on("click", function() {
		$("#query-move-div").toggle();
		
		if($("#query-move-div").is(":hidden")) {
			$("#query-move-toggle").html("Move query &dArr;");
		}
		else {
			$("#query-move-toggle").html("Move query &uArr;");
		}
	});

	// CLICK EVENT: move query to another website
	$("#query-move").on("click", function() {
		var id = parseInt($("#query-select").val(), 10);
		var to = parseInt($("#move-to").val(), 10);
		
		runCmd(
				"movequery",
				{
					"id" : id,
					"to" : to
				},
				true,
				{
					"m" : "queries",
					"website" : to,
					"query" : id,
					"test" : $("#query-test-text").val(),
					"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
				}
		);
	});
	
	// CLICK EVENT: update query
	$("#query-update").on("click", function() {
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
					"resultsubsets" : $("#query-result-subsets").is(":checked"),
					"textonly" : $("#query-text-only").is(":checked")
				},
				true,
				{
					"m" : "queries",
					"website" : website,
					"query" : id,
					"test" : $("#query-test-text").val(),
					"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
				}
		);
		
		return false;
	});
	
	// CLICK EVENT: delete query
	$("#query-delete").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#query-select").val(), 10);
		
		if(id) {
			runCmd(
					"deletequery",
					{
						"id" : id
					},
					true,
					{
						"m" : "queries",
						"website" : website,
						"test" :  $("#query-test-text").val(),
						"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
					}
			);
		}
		
		return false;
	});
	
	// CLICK EVENT: test query (or hide test result)
	$("#query-test").on("click", function() {
		if($("#query-test-text").is(":visible")) {
			$("#query-test-label").text("Test result:");
			$("#query-test-text").hide();
			$("#query-test-result").show();
			$("#query-test-result").val("Testing query...");
			$("#query-test").text("Back to text");
			
			var queryText = $("#query-text").val();
			
			// remove whitespaces in the end of the query text
			while(
					queryText.length > 0 
					&& "\f\n\r\t\v\u00A0\u2028\u2029".indexOf(queryText[queryText.length - 1]) > -1
			) {
				queryText = queryText.slice(0, -1);
			}
			
			var timerStart = +new Date();
			var args = {
							"cmd" : "testquery",
							"query" : queryText,
							"type" : $("#query-type-select").val(),
							"resultbool": $("#query-result-bool").is(":checked"),
							"resultsingle" : $("#query-result-single").is(":checked"),
							"resultmulti" : $("#query-result-multi").is(":checked"),
							"resultsubsets" : $("#query-result-subsets").is(":checked"),
							"textonly" : $("#query-text-only").is(":checked"),
							"text" : $("#query-test-text").val(),
							"xmlwarnings" : $("#query-xml-warnings").is(":checked"),
							"fixcdata" : $("#query-fix-cdata").is(":checked"),
							"fixcomments" : $("#query-fix-comments").is(":checked"),
							"removexml" : $("#query-remove-xml").is(":checked"),
							"datetimeformat" : $("#query-datetime").is(":checked") ? $("#query-datetime-format").val() : "",
							"datetimelocale" : $("#query-datetime").is(":checked") ? $("#query-datetime-locale").val() : ""
						};
			
			$.ajax({
				type: "POST",
				url: cc_host,
				data: JSON.stringify(args, null, 1),
				contentType: "application/json; charset=utf-8",
				dataType: "json",
				success: function(data) {
					var timerEnd = +new Date();
					var resultText = "crawlserv++ responded (" + msToStr(timerEnd - timerStart) + ")\n";
					
					if(data["fail"]) {
						resultText += "ERROR: " + data["text"] + "\nDEBUG:\n" + data["debug"];
					}
					else {
						resultText += data["text"];
					}
						
					$("#query-test-result").val(resultText);
					
					if($("#query-new-tab").is(":checked")) {
						// open result in new tab
						newTab = window.open("", "crawlserv_query", false);
						
						if(newTab) {
							newTab.document.open();
						
							newTab.document.writeln("<!DOCTYPE html>");
							newTab.document.writeln("<html lang=\"en\">");
							newTab.document.writeln(" <head>");
							newTab.document.writeln("  <meta charset=\"utf-8\">");
							newTab.document.writeln("  <link rel=\"stylesheet\" type=\"text/css\" href=\"css/queryresult.css\">");
							newTab.document.writeln("  <title>crawlserv++ frontend: Query Test Result</title>");
							newTab.document.writeln(" </head>");
							newTab.document.writeln(" <body>");
							newTab.document.writeln("  <h1>crawlserv++ Query Test Result</h1>");
							newTab.document.writeln("  <pre id=\"result-target\"></pre>");
							newTab.document.writeln(" </body>");
							newTab.document.writeln("</html>");
							
							newTab.document.close();
							
							newTab.document.getElementById("result-target").appendChild(
									newTab.document.createTextNode(resultText)
							);
							
							newTab.focus();
						}
						else {
							console.error(
									"Could not open a new window, it has probably been blocked by a popup blocker."
							);
							
							alert(
									"Could not open a new window, it has probably been blocked by a popup blocker."
							);
						}
					}
				}
			}).fail(function() {
				$("#query-test-result").val("ERROR: Could not connect to server.");
			});
		}
		else if($("#query-test-result").is(":visible")) {
			$("#query-test-label").text("Test text:");
			$("#query-test-result").hide();
			$("#query-test-text").show();
			$("#query-test").text("Test query");
		}
		
		return false;
	});
	
	// CHANGE EVENT: "test date/time" toggled
	$("#query-datetime").on("change", function() {
		if($(this).is(":checked")) {
			// add locales to selection if necessary
			if($("#query-datetime-locale option").length == 1) {
				for(const locale of db_locales) {
					$("#query-datetime-locale").append(
							"\n<option value=\"" + locale + "\">" + locale + "</option>"
					);
				}
			}
			
			// show date-related options
			$("#query-datetime-format-label").show();
			$("#query-datetime-format").show();
			$("#query-datetime-locale-label").show();
			$("#query-datetime-locale").show();
		}
		else {
			// hide date-related options
			$("#query-datetime-format-label").hide();
			$("#query-datetime-format").hide();
			$("#query-datetime-locale-label").hide();
			$("#query-datetime-locale").hide();
		}
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
				if(confirm("Do you want to discard the changes to your current configuration?")) {
					reload({
						"m" : $(this).data("m"),
						"website" : website,
						"config" : id,
						"mode" : $(this).data("mode")
					});
				}
				else {
					reenableInputs();
				}
			}
			else {
				reload({
					"m" : $(this).data("m"),
					"website" : website,
					"config" : id,
					"mode" : $(this).data("mode")
				});
			}
			
			return false;
		}
		
		return true;
	});

	// CHANGE EVENT: algorithm category selected (for ANALYZERS only)
	$("#algo-cat-select").on("change", function() {
		$(this).blur();
		
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
			else {
				reenableInputs();
			}
		}
		else {
			reload({
				"m" :
				"analyzers",
				"website" : website,
				"config" : config_id,
				"mode" : $(this).data("mode"),
				"algo_cat": id,
				"algo_changed": id != algo.id
			});
		}
		
		return false;
	});
	
	// CHANGE EVENT: algorithm selected (for ANALYZERS only)
	$("#algo-select").on("change", function() {
		$(this).blur();
		
		disableInputs();
		
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
			else {
				reenableInputs();
			}
		}
		else {
			reload({
				"m" : "analyzers",
				"website" : website,
				"config" : config_id,
				"mode" : $(this).data("mode"),
				"algo_id": id
			});
		}
		
		return false;
	});

	// CLICK EVENT: change mode
	$("a.post-redirect-mode").on("click", function() {
		disableInputs();
		
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
		
		$("#container").scrollTop(
				$(this).offset().top
				- $("#container").offset().top
				+ $("#container").scrollTop()
				- $(this).outerHeight()
				- 2.5
		);
		
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
	$("a.config-add").on("click", function() {
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
	$("#config-duplicate").on("click", function() {
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
	$("a.config-update").on("click", function() {
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
	$("#config-delete").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var id = parseInt($("#config-select").val(), 10);
		
		if(id > 0) {
			runCmd(
					"deleteconfig",
					{
						"id" : id
					},
					true,
					{
						"m" : $(this).data("m"),
						"website" : website,
						"test" :  $("#query-test-text").val(),
						"test-nonewtab" : $("#query-new-tab").is(":checked") ? null : true
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
				{
					"id" : id
				},
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
				{
					"id" : id
				},
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
				{
					"id" : id
				},
				false
		);
		
		return false;
	})

	// CLICK EVENT: start thread
	$("#thread-start").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var module = $("#module-select").val();
		var config = parseInt($("#config-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		
		runCmd(
				"start" + module,
				{
					"website" : website,
					"config" : config,
					"urllist" : urllist
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
			var target = parseInt(
					prompt(
							"! WARNING ! Time travel can have unintended consequences.\n\nEnter ID to jump to:",
							last
					),
					10
			);
			
			if(target > 0) {
				runCmd(
						"warp",
						{
							"thread" : id,
							"target": target
						},
						false
				);
			}
		}
	})
	
	/*
	 * EVENTS for CONTENT
	 */
		
	// CLICK EVENT: change tab
	$("a.post-redirect-tab").on("click", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = 0;
		
		if($("#content-url").length) {
			url = parseInt($("#content-url").val(), 10);
		}
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url
		});
		
		return false;
	});
	
	// FOCUS EVENT: focus on URL ID or URL text selects it
	$("#content-url, #content-url-text").on("focus", function() {
		$(this).select();
		
		return true;
	});
	
	// CLICK EVENT: go to specific content
	$("#content-goto").on("click", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : "crawled",
			"website" : website,
			"urllist" : urllist,
			"url": url,
			"version": $(this).data("version")
		});
		
		return false;
	});
	
	// CLICK EVENT: previous URL
	$("#content-last").on("click", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		
		var args = {
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url,
			"last": true
		}
		
		if(
				$(this).data("tab") != "crawled" 
				&& $("#content-version").length > 0
		) {
			args.version = parseInt($("#content-version").val(), 10);
		}
		
		reload(args);
		
		return false;
	});
	
	// CLICK EVENT: next URL
	$("#content-next").on("click", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist =  parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
	
		var args = {
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url + 1
		};
		
		if(
				$(this).data("tab") != "crawled" 
				&& $("#content-version").length > 0
		) {
			args.version = parseInt($("#content-version").val(), 10);
		}
		
		reload(args);
		
		return false;
	});
	
	// CLICK EVENT: go to dataset
	$("button.content-dataset-to").on("click", function() {
		disableInputs();
		
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var url = parseInt($("#content-url").val(), 10);
		var version = parseInt($("#content-version").val(), 10);
		
		var args = {
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"url": url,
			"version": version,
			"offset": $(this).data("to")
		};
		
		reload(args);
		
		return false;
	});

	// CHANGE EVENT: input URL or dataset ID (numbers only)
	$("#content-url, #content-dataset").on("input keydown keyup mousedown mouseup select contextmenu drop", function() {
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
	$("#content-url").on("keypress", function(e) {
		if(e.which == 13) {
			if($(this).val().length == 0) {
				return true;
			}
			
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var urllist = parseInt($("#urllist-select").val(), 10);
			var url = parseInt($(this).val(), 10);
			
			var args = {
				"m" : $(this).data("m"),
				"tab" : $(this).data("tab"),
				"website" : website,
				"urllist" : urllist,
				"url": url
			};
			
			if(
					$(this).data("tab") != "crawled" 
					&& $("#content-version").length > 0
			) {
				args.version = parseInt($("#content-version").val(), 10);
			}
			
			reload(args);
			
			return false;
		}
		
		return true;
	});
	
	// KEYPRESS EVENT: input dataset ID (ENTER)
	$("#content-dataset").on("keypress", function(e) {
		if(e.which == 13) {
			if($(this).val().length == 0) {
				return true;
			}
			
			var value = parseInt($(this).val(), 10);
			
			if(value <= 0) {
				$(this).val(1);
				
				value = 1;
			}
			else if(value >= $(this).data("max")) {
				$(this).val($(this).data("max"));
				
				value = $(this).data("max");
			}
			
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var urllist = parseInt($("#urllist-select").val(), 10);
			var url = parseInt($("#content-url").val(), 10);
			var version = parseInt($("#content-version").val(), 10);
			
			var args = {
				"m" : $(this).data("m"),
				"tab" : $(this).data("tab"),
				"website" : website,
				"urllist" : urllist,
				"url": url,
				"version": version,
				"offset": value - 1
			};
			
			reload(args);
			
			return false;
		}
		
		return true;
	});
	
	// KEYPRESS EVENT: input URL text (ENTER)
	$("#content-url-text").on("keypress", function(e) {
		if(e.which == 13) {
			if(!$(this).val().length) {
				return true;
			}
			
			disableInputs();
			
			var website = parseInt($("#website-select").val(), 10);
			var urllist = parseInt($("#urllist-select").val(), 10);
			
			var args = {
				"m" : $(this).data("m"),
				"tab" : $(this).data("tab"),
				"website" : website,
				"urllist" : urllist,
				"urltext": $(this).val()
			};
			
			if(
					$(this).data("tab") != "crawled" 
					&& $("#content-version").length > 0
			) {
				args.version = parseInt($("#content-version").val(), 10);
			}
			
			reload(args);
			
			return false;
		}
		
		return true;
	});
	
	// CLICK EVENT: fullscreen
	$("#content-fullscreen").on("click", function() {
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
				"width" : "calc(100% - 5px)",
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
	$("#content-version").on("change", function() {
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
	$("#content-download").on("click", function() {		
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
			if(!window.open("download/", "crawlserv_download", false)) {
				console.error(
						"Could not open a new window, it has probably been blocked by a popup blocker."
				);
				alert(
						"Could not open a new window, it has probably been blocked by a popup blocker."
				);
			}
		});
		
		return false;
	});
	
	// CLICK EVENT: search for parsed ID
	$("#content-search-parsed").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var version = parseInt($("#content-version").val(), 10);
		var parsedId = $("#content-parsed-id").val();
		var url = parseInt($("#content-url").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"version": version,
			"parsed_id": parsedId,
			"url": url /* as fallback */
		});
	});
	
	// CHANGE EVENT: reset formatting of updated corpus position input field
	$("#corpus-position.updated, #corpus-length.updated").on("keyup keypress change", function() {
		$(this).removeClass("updated");
	});
	
	// CLICK EVENT: go to corpus position
	$("#content-go-position").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var corpus = parseInt($("#corpus-select").val(), 10);
		var corpusPos = parseInt($("#corpus-position").val(), 10);
		var corpusLen = parseInt($("#corpus-length").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"corpus": corpus,
			"corpus_pos": corpusPos,
			"corpus_len": corpusLen
		});
		
		return false;
	});
	
	// CLICK EVENT: go to article number
	$("#content-go-article-num").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var corpus = parseInt($("#corpus-select").val(), 10);
		var article = parseInt($("#corpus-article-num").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"corpus": corpus,
			"corpus_article_num": article
		});
	});
	
	// CLICK EVENT: go to article ID
	$("#content-go-article-id").on("click", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = parseInt($("#urllist-select").val(), 10);
		var corpus = parseInt($("#corpus-select").val(), 10);
		var article = parseInt($("#corpus-article-id").val(), 10);
		
		reload({
			"m" : $(this).data("m"),
			"tab" : $(this).data("tab"),
			"website" : website,
			"urllist" : urllist,
			"corpus": corpus,
			"corpus_article_id": article
		});
	});
	
	/*
	 * EVENTS for IMPORT/EXPORT [DATA]
	 */
		
	// CHANGE EVENT: action selected
	$("input[name='action']").on("change", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = getFirstUrlList();
		var dataType = $("#data-type-select").val();
		var fileType = $("#file-type-select").val();
		
		disableInputs();
		
		reload({
			"m" : $(this).data("m"),
			"website" : website,
			"urllist" : urllist,
			"action" : $(this).val(),
			"datatype": dataType,
			"filetype": fileType
		});
		
		return false;
	});
	
	// CHANGE EVENT: data type selected
	$("#data-type-select").on("change", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = getFirstUrlList();
		var action = $("input[name='action']:checked").val();
		var fileType = $("#file-type-select").val();
		
		disableInputs();
		
		reload({
			"m" : $(this).data("m"),
			"website" : website,
			"urllist" : urllist,
			"action" : action,
			"datatype": $(this).val(),
			"filetype": fileType
		});
		
		return false;
	});
	
	// CHANGE EVENT: file type selected
	$("#file-type-select").on("change", function() {
		var website = parseInt($("#website-select").val(), 10);
		var urllist = getFirstUrlList();
		var action = $("input[name='action']:checked").val();
		var dataType = $("#data-type-select").val();
		
		disableInputs();
		
		reload({
			"m" : $(this).data("m"),
			"website" : website,
			"urllist" : urllist,
			"action" : action,
			"datatype": dataType,
			"filetype": $(this).val()
		});
		
		return false;
	});
	
	// CHANGE EVENT: compression selected
	$("#compression-select").on("change", function() {
		$("#website-select").data("compression", $(this).val());
	});
	
	// CHANGE EVENT: target URL list selected
	$("#urllist-target").on("change", function() {		
		if($(this).val() == 0) {
			// show inputs for name and namespace if not shown yet
			if(optionUrlListName) {
				optionUrlListName.insertAfter("#urllist-target-div");
				
				optionUrlListName = null;
			}
			
			if(optionUrlListNamespace) {
				optionUrlListNamespace.insertAfter("#urllist-name-div");
				
				optionUrlListNamespace = null;
			}
		}
		else {
			// hide inputs for name and namespace if not hidden yet
			if(!optionUrlListName) {
				optionUrlListName = $("#urllist-name-div").detach();
			}
			
			if(!optionUrlListNamespace) {
				optionUrlListNamespace = $("#urllist-namespace-div").detach();
			}
		}
	});

	// CHANGE EVENT: write first-line header toggled
	$("#write-firstline-header").on("change", function() {
		$("#firstline-header").prop("disabled", !$(this).prop("checked"));
	});
	
	// CLICK EVENT: perform selected data action (import, export or merge)
	$("#perform-action").on("click", function() {
		// save pointer to button
		let t = this;
		
		// check connection to server
		$("#hidden-div").load(cc_host, function(response, status, xhr) {
			if(status == "error") {
				alert("No connection to the server.");
			}
			else {
				// get type of action
				var action = $(t).data("action");
				
				// set arguments and properties
				var args = {};
				var props = {};
				
				// (datatype, compression, compression path)
				args["datatype"] = $("#data-type-select").val();
				
				if(action != "merge") {
					args["filetype"] = $("#file-type-select").val();
					args["compression"] = $("#compression-select").val();
					
					if(action == "export") {
						// (file ending for download)
						props["ending"] = "";
						
						if(args["filetype"] == "text") {
							props["ending"] += ".txt";
						}
						else if(args["filetype"] == "spreadsheet") {
							props["ending"] += ".ods";
						}
						
						if(args["compression"] == "gzip") {
							props["ending"] += ".gz";
						}
						else if(args["compression"] == "zlib") {
							props["ending"] += ".zlib";
						}
						else if(args["compression"] == "zip") {
							props["ending"] += ".zip";
						}
					}
				}
				
				// (website, source URL list, target URL list, source table)
				if($("#website-select").length) {
					args["website"] = parseInt($("#website-select").val(), 10);
					props["website-namespace"] = $("#website-select").find(":selected").data("namespace");
				}
				
				if($("#urllist-select").length) {
					args["urllist"] = parseInt($("#urllist-select").val(), 10);
					props["urllist-namespace"] = $("#urllist-select").find(":selected").data("namespace");
				}
				
				if($("#urllist-source").length) {
					args["urllist-source"] = parseInt($("#urllist-source").val(), 10);
					props["urllist-namespace"] = $("#urllist-source").find(":selected").data("namespace");
				}
				
				if($("#urllist-target").length) {
					args["urllist-target"] = parseInt($("#urllist-target").val(), 10);
				}
				
				if($("#table-select").length) {
					args["source"] = parseInt($("#table-select").val(), 10);
					props["table"] = $("#table-select").find(":selected").data("table");
				}
				
				// (URL list name and namespace)
				if($("#urllist-name").length) {
					args["urllist-name"] = $("#urllist-name").val();
				}
				
				if($("#urllist-namespace").length) {
					args["urllist-namespace"] = $("#urllist-namespace").val();
					props["urllist-namespace"] = args["urllist-namespace"];
				}
				
				// (header options for URL lists)
				if($("#is-firstline-header").length) {
					args["is-firstline-header"] = $("#is-firstline-header").prop("checked");
				}
				
				if($("#write-firstline-header").length) {
					args["write-firstline-header"] = $("#write-firstline-header").prop("checked");
				}
				
				if($("#firstline-header").length) {
					args["firstline-header"] = $("#firstline-header").val();
				}
				
				// (header option for data tables)
				if($("#column-names").length) {
					args["column-names"] = $("#column-names").prop("checked");
				}
				
				if(action == "import") {
					// check whether file has been selected
					if($("#file-select").val().length) {
						// preserve caption of button
						var caption = $(t).val();
						
						// disable button while uploading file
						$(t).prop("disabled", true);
						$(t).val("Importing...");
						
						// submit form for file upload
						$("#file-form").ajaxSubmit({
							success: function(response) {
								// file upload successful: add filename to
								//  arguments
								args["filename"] = response;
								
								// run command
								runCmd("import", args, false, null, null, null, function() {
									// restore button and inputs
									$(t).prop("disabled", false);
									$(t).val(caption)
									
									reenableInputs();
									disableImportExport();
								});
							},
							failure: function(response) {
								// file upload failed
								alert("File upload failed: " + response);
								
								// restore button and inputs
								$(t).prop("disabled", false);
								$(t).val(caption);
								
								reenableInputs();
								disableImportExport();
							}
						});
						
						// disable inputs while waiting for response
						disableInputs();
					}
					else {
						alert("Please select a file to upload.");
					}
				}
				else if(action == "export") {
					// preserve caption of button
					var caption = $(t).val();
					
					// disable button and inputs while exporting
					$(t).prop("disabled", true);
					$(t).val("Exporting...");
					
					disableInputs();
					
					// run command for export
					args["cmd"] = "export";
					
					$.ajax({
							type: "POST",
							url: cc_host,
							data: JSON.stringify(args, null, 1),
							contentType: "application/json; charset=utf-8",
							dataType: "json",
							success: function(response, status, xhr) {
								if(response["fail"]) {
									alert(
											"crawlserv++ responded with error:\n\n"
											+ response["text"]
											+ "\n\ndebug: "
											+ response["debug"]
									);
								}
								else {
									// download resulting file
									var downloadAs =
										props["website-namespace"]
										+ "_"
										+ props["urllist-namespace"];
									
									if(props["table"]) {
										downloadAs += "_" + props["table"];
									}
									
									downloadAs += props["ending"];
									
									runCmd(
											"download",
											{
												"filename": response["text"],
												"as": downloadAs
											},
											false
									);
								}
							},
							failure: function(response) {
								alert("Error performing the export: " + response);
							},
							complete: function() {
								// restore button and inputs
								$(t).prop("disabled", false);
								$(t).val(caption)
								
								reenableInputs();
								disableImportExport();
							}
					});
				}
				else if(action == "merge") {
					// preserve caption of button
					var caption = $(t).val();
					
					// disable button and inputs while exporting
					$(t).prop("disabled", true);
					$(t).val("Merging...");
					
					disableInputs();
					
					// perform merge
					runCmd("merge", args, false, null, null, null, function() {
						// restore button and inputs
						$(t).prop("disabled", false);
						$(t).val(caption)
						
						reenableInputs();
						disableImportExport();
					});
				}
				else if(action.length) {
					alert("Unknown action: \"" + action + "\".");
				}
				else {
					alert("No action specified.");
				}
			}
		});
		
		return true;
	});
	
	/*
	 * 
	 */
	
});
