/* prevent execution of more than once */
if(typeof window.pz2 == "undefined") {
window.undefined = window.undefined;

var pz2 = function(callbackArr, autoInit, keepAlive) {
    // 
    if ( callbackArr == null )
        return null;

    //for convenience
    myself = this;
    
    myself.pz2String = "search.pz2";
    myself.sessionID;
    myself.initStatus = 0;
    myself.pingStatus = 0;
    myself.keepAlive = 50000;

    if ( keepAlive < myself.keepAlive )
        myself.keepAlive = keepAlive;

    // for sorting
    myself.currentSort = "relevance";
    myself.sortKeywords = [ "relevance", "title", "author" ];
    
    // function callbacks
    myself.statCallback = callbackArr.onstat;
    myself.showCallback = callbackArr.onshow;
    myself.termlistCallback = callbackArr.onterm;

    //timers
    myself.statTime = 2000;
    myself.statTimer;
    myself.termTime = 1000;
    myself.termTimer;
    myself.showTime = 1000;
    myself.showTimer;

    if (autoInit == true)
        myself.init(myself.keepAlive);
}
pz2.prototype = {
    init: function(keepAlive) {
        if ( keepAlive < myself.keepAlive ) myself.keepAlive = keepAlive;  
        $.get( myself.pz2String,
            { command: "init" },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" )
                    myself.initStatus = 1;
                myself.sessionID = data.getElementsByTagName("session")[0].childNodes[0].nodeValue;
                setTimeout(myself.ping, myself.keepAlive);
            }
        );
    },
    // no need to ping explicitly
    ping: function() {
        if( !myself.initStatus )
            return;
            // session is not initialized code here
        $.get( myself.pz2String,
            { command: "ping", session: myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    myself.pingStatus = 1;
                    setTimeout(myself.ping, myself.keepAlive);
                }
                else
                    location = "?";
            }
        );
    },
    search: function(query, num, sort) {
        clearTimeout(myself.statTimer);
        clearTimeout(myself.showTimer);
        clearTimeout(myself.termTimer);
        if( !myself.initStatus )
            return;
        $.get( myself.pz2String,
            { command: "search", session: myself.sessionID, query: query },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    myself.searchStatus = 1;
                    //piggyback search
                    myself.show(0, num, sort)
                    if ( myself.statCallback )
                        myself.statTimer = setTimeout(myself.stat, myself.statTime / 2);
                    if ( myself.termlistCallback )
                        myself.termTimer = setTimeout(myself.termlist, myself.termTime / 2);
                }
                else
                    location = "?";
            }
        );
    },
    // callback, not to be called explicitly
    stat: function() {
        if( !myself.searchStatus )
            return;
        $.get( myself.pz2String,
            { command: "stat", session: myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("stat") ) {
                    var activeClients = Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue );
                    var stat = {
                    "activeclients": activeClients,
                    "hits": Number( data.getElementsByTagName("hits")[0].childNodes[0].nodeValue ),
                    "records": Number( data.getElementsByTagName("records")[0].childNodes[0].nodeValue ),
                    "clients": Number( data.getElementsByTagName("clients")[0].childNodes[0].nodeValue ),
                    "initializing": Number( data.getElementsByTagName("initializing")[0].childNodes[0].nodeValue ),
                    "searching": Number( data.getElementsByTagName("searching")[0].childNodes[0].nodeValue ),
                    "presenting": Number( data.getElementsByTagName("presenting")[0].childNodes[0].nodeValue ),
                    "idle": Number( data.getElementsByTagName("idle")[0].childNodes[0].nodeValue ),
                    "failed": Number( data.getElementsByTagName("failed")[0].childNodes[0].nodeValue ),
                    "error": Number( data.getElementsByTagName("error")[0].childNodes[0].nodeValue )
                    }
                    myself.statCallback(stat);
                    if (activeClients > 0)
                        myself.statTimer = setTimeout(myself.stat, myself.statTime); 
                }
                else
                    myself.statTimer = setTimeout(myself.stat, myself.statTime / 4)
                    //location = "?";
                    // some error handling
            }
        );
    },
    //callback not to be called explicitly
    show: function(start, num, sort) {
        clearTimeout(myself.showTimer);
        if( !myself.searchStatus )
            return;
        if ( myself.sortKeywords.some( function(element, index, arr) { if (element == sort) return true; else return false; } ) )
            myself.currentSort = sort;
        $.get( myself.pz2String,
            { "command": "show", "session": myself.sessionID, "start": start, "num": num, "sort": myself.currentSort, "block": 1 },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    var activeClients = Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue );
                    var show = {
                    "activeclients": activeClients,
                    "merged": Number( data.getElementsByTagName("merged")[0].childNodes[0].nodeValue ),
                    "total": Number( data.getElementsByTagName("total")[0].childNodes[0].nodeValue ),
                    "start": Number( data.getElementsByTagName("start")[0].childNodes[0].nodeValue ),
                    "num": Number( data.getElementsByTagName("num")[0].childNodes[0].nodeValue )
                    }
                    //TODO include records
                    myself.showCallback(show);
                    if (activeClients > 0)
                        myself.showTimer = setTimeout("myself.show(" + start + "," + num + "," + sort + ")", myself.showTime);
                }
                else
                    myself.showTimer = setTimeout("myself.show(" + start + "," + num + "," + sort + ")", myself.showTime / 4);
                    // location = "?";
                    // some error handling
            }
        );
    },
    record: function(id) {
    },
    termlist: function(name) {
        if( !myself.searchStatus )
            return;
        $.get( myself.pz2String,
            { "command": "termlist", "session": myself.sessionID, "name": "author" },
            function(data) {
                if ( data.getElementsByTagName("termlist") ) {
                    var termList = { "activeclients": Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue ) }
                    var termLists = data.getElementsByTagName("list");
                    //for each termlist
                    for (i = 0; i < termLists.length; i++) {
                	var listName = termLists[i].getAttribute('name');
                        termList[listName] = new Array();
                        var terms = termLists[i].getElementsByTagName('term');
                        //for each term in the list
                        for (j = 0; j < terms.length; j++) { 
                            var term = {
                                "name": terms[j].getElementsByTagName("name")[0].childNodes[0].nodeValue,
                                "freq": terms[j].getElementsByTagName("frequency")[0].childNodes[0].nodeValue,
                            }
                            termList[listName][j] = term;
                        }
                    }
                    myself.termlistCallback(termList);
                    if (termList["activeclients"] > 0)
                        myself.termTimer = setTimeout("myself.termlist(" + name + ")" , myself.termTime); 
                }
                else
                    myself.termTimer = setTimeout("myself.termlist(" + name + ")" , myself.termTime / 4); 
                    //location = "?";
                    // some error handling
            }
        );

    },
    bytarget: function() {
    }
};
}
