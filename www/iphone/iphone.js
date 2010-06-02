var count = 0;
var termlist = {};
var JSON = JSON || {};
var inApp = false;

var callback = {};

callback.init = function() {
	if (!inApp) {
		callback.type = 'browser';
		document.getElementById("heading").style.display="";
	} else
		callback.type = 'iphone';

	var searchdiv = document.getElementById("searchdiv");
	if (this.type != 'iphone') {
		searchdiv.style.display = '';
		document.search.onsubmit = onFormSubmit;
	}
	else
		searchdiv.style.display = 'none';
};

String.prototype.replaceAll = function(stringToFind,stringToReplace) {
		var temp = this;
		var index = temp.indexOf(stringToFind);
		while(index != -1){
			temp = temp.replace(stringToFind,stringToReplace);
			index = temp.indexOf(stringToFind);
		}
		return temp;
    }

callback.send = function() 
{
	var args = [].splice.call(arguments,0);
	for (var i = 0; i < args.length; i++) {
		if (args[i])
			args[i] = args[i].replaceAll(':','_'); 
		else 
			alert("args was null: " + i);
	}
	var message = "myapp:" + args.join(":");
	if (this.type == 'iphone')
		document.location = message;
	else
		document.getElementById("log").innerHTML = message;
}

// implement JSON.stringify serialization
JSON.stringify = JSON.stringify || function(obj) {
	var t = typeof (obj);
	if (t != "object" || obj === null) {
		// simple data type
		if (t == "string")
			obj = '"' + obj + '"';
		return String(obj);
	} else {
		// recurse array or object
		var n, v, json = [], arr = (obj && obj.constructor == Array);
		for (n in obj) {
			v = obj[n];
			t = typeof (v);
			if (t == "string")
				v = '"' + v + '"';
			else if (t == "object" && v !== null)
				v = JSON.stringify(v);
			json.push((arr ? "" : '"' + n + '":') + String(v));
		}
		return (arr ? "[" : "{") + String(json) + (arr ? "]" : "}");
	}
};

function search(message) {
	document.search.query.value = message;
	onFormSubmitEventHandler();
	return false;
}

function loaded() {
	callback.init();
}

function onFormSubmit() {
	return search(document.search.query.value);
}
