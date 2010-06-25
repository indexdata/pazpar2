var count = 0;
var termlist = {};
var inApp = false;

var callback = {};

callback.init = function() {
	if (!inApp) {
		callback.type = 'browser';
	} else {
		callback.type = 'iphone';
	}
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
