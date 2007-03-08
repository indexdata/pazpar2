/* prevent execution of more than once */
if(typeof window.pz2 == "undefined") {
window.undefined = window.undefined;

var pz2 = function(callbackArr, autoInit, keepAlive) {
    //for convenience
    myself = this;

    // at least one callback required
    if ( !callbackArr )
        throw new Error("Callback parameters array has to be suplied when instantiating a class");   
    
    // function callbacks
    myself.statCallback = callbackArr.onstat;
    myself.showCallback = callbackArr.onshow;
    myself.termlistCallback = callbackArr.onterm;
    myself.recordCallback = callbackArr.onrecord || null;

    // termlist keys
    myself.termKeys = callbackArr.termlist || "subject";
    
    myself.pz2String = "search.pz2";
    myself.sessionID = null;
    myself.initStatusOK = false;
    myself.pingStatusOK = false;
    myself.searchStatusOK = false;
    myself.keepAlive = 50000;

    if ( keepAlive < myself.keepAlive )
        myself.keepAlive = keepAlive;

    // for sorting
    myself.currentSort = "relevance";
    // where are we?
    myself.currentStart = 0;
    myself.currentNum = 20;

    //timers
    myself.statTime = 2000;
    myself.statTimer = null;
    myself.termTime = 1000;
    myself.termTimer = null;
    myself.showTime = 1000;
    myself.showTimer = null;

    // error handling
    $(document).ajaxError( 
    function (request, settings, exception) {
        if ( settings.responseXML && settings.responseXML.getElementsByTagName("error")[0].childNodes[0].nodeValue)
            throw new Error( settings.responseXML.getElementsByTagName("error")[0].childNodes[0].nodeValue);
    });

    if (autoInit !== false)
        myself.init(myself.keepAlive);
}
pz2.prototype = {
    init: function(keepAlive) {
        if ( keepAlive < myself.keepAlive ) myself.keepAlive = keepAlive;  
        $.get( myself.pz2String,
            { command: "init" },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    myself.initStatusOK = true;
                    myself.sessionID = data.getElementsByTagName("session")[0].childNodes[0].nodeValue;
                    setTimeout(myself.ping, myself.keepAlive);
                }
            }
        );
    },
    // no need to ping explicitly
    ping: function() {
        if( !myself.initStatusOK )
            return;
            // session is not initialized code here
        $.get( myself.pz2String,
            { command: "ping", session: myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    myself.pingStatusOK = true;
                    setTimeout("myself.ping()", myself.keepAlive);
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
        if( !myself.initStatusOK )
            return;
        $.get( myself.pz2String,
            { command: "search", session: myself.sessionID, query: query },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    myself.searchStatusOK = true;
                    //piggyback search
                    myself.show(0, num, sort)
                    if ( myself.statCallback )
                        myself.statTimer = setTimeout("myself.stat()", myself.statTime / 2);
                    if ( myself.termlistCallback )
                        myself.termTimer = setTimeout("myself.termlist()", myself.termTime / 2);
                }
                else
                    location = "?";
            }
        );
    },
    // callback, not to be called explicitly
    stat: function(test) {
        if( !myself.searchStatusOK )
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
                        myself.statTimer = setTimeout("myself.stat()", myself.statTime); 
                }
                else
                    myself.statTimer = setTimeout("myself.stat()", myself.statTime / 4)
                    //location = "?";
                    // some error handling
            }
        );
    },
    //callback not to be called explicitly
    show: function(start, num, sort) {
        clearTimeout(myself.showTimer);
        if( !myself.searchStatusOK )
            return;
        
        if( sort !== undefined )
            myself.currentSort = sort;
        if( start !== undefined )
            myself.currentStart = Number( start );
        if( num !== undefined )
            myself.currentNum = Number( num );
        
        $.get( myself.pz2String,
            { "command": "show", "session": myself.sessionID, "start": myself.currentStart, "num": myself.currentNum, "sort": myself.currentSort, "block": 1 },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    var activeClients = Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue );
                    var show = {
                    "activeclients": activeClients,
                    "merged": Number( data.getElementsByTagName("merged")[0].childNodes[0].nodeValue ),
                    "total": Number( data.getElementsByTagName("total")[0].childNodes[0].nodeValue ),
                    "start": Number( data.getElementsByTagName("start")[0].childNodes[0].nodeValue ),
                    "num": Number( data.getElementsByTagName("num")[0].childNodes[0].nodeValue ),
                    "hits": []
                    }

                    var hits = data.getElementsByTagName("hit");
                    var hit = new Array();
                    for (i = 0; i < hits.length; i++) {
                        show.hits[i] = new Array();
                        for ( j = 0; j < hits[i].childNodes.length; j++) {
                            if ( hits[i].childNodes[j].nodeType == Node.ELEMENT_NODE ) {
                                var nodeName = hits[i].childNodes[j].nodeName;
                                var nodeText = hits[i].childNodes[j].firstChild.nodeValue;
                                show.hits[i][nodeName] = nodeText;
                            }
                        }
                    }

                    //TODO include records
                    myself.showCallback(show);
                    if (activeClients > 0)
                        myself.showTimer = setTimeout("myself.show()", myself.showTime);
                }
                else
                    myself.showTimer = setTimeout("myself.show()", myself.showTime / 4);
                    // location = "?";
                    // some error handling
            }
        );
    },
    record: function(id) {
        if( !myself.searchStatusOK )
            return;
        $.get( myself.pz2String,
            { "command": "record", "session": myself.sessionID, "id": id },
            function(data) {
                var recordNode;
                var record = new Array();
                if ( recordNode = data.getElementsByTagName("record")[0] ) {
                    for ( i = 0; i < recordNode.childNodes.length; i++) {
                        if ( recordNode.childNodes[i].nodeType == Node.ELEMENT_NODE ) {
                            var nodeName = recordNode.childNodes[i].nodeName;
                            var nodeText = recordNode.childNodes[i].firstChild.nodeValue;
                            record[nodeName] = nodeText;
                        }
                    }
                    myself.recordCallback(record);
                }
                else
                    alert("");
                    //location = "?";
                    // some error handling
            }
        );
        
    },
    termlist: function() {
        if( !myself.searchStatusOK )
            return;
        $.get( myself.pz2String,
            { "command": "termlist", "session": myself.sessionID, "name": myself.termKeys },
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
                        myself.termTimer = setTimeout("myself.termlist()", myself.termTime); 
                }
                else
                    myself.termTimer = setTimeout("myself.termlist()", myself.termTime / 4); 
                    //location = "?";
                    // some error handling
            }
        );

    },
    bytarget: function() {
    }
};
}
