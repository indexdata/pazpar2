/* prevent execution of more than once */
if(typeof window.pz2 == "undefined") {
window.undefined = window.undefined;

var pz2 = function(callbackArr, autoInit, keepAlive) {
    //for convenience
    __myself = this;

    // at least one callback required
    if ( !callbackArr )
        throw new Error("Callback parameters array has to be suplied when instantiating a class");   
    
    // function callbacks
    __myself.statCallback = callbackArr.onstat || null;
    __myself.showCallback = callbackArr.onshow || null;
    __myself.termlistCallback = callbackArr.onterm || null;
    __myself.recordCallback = callbackArr.onrecord || null;
    __myself.bytargetCallback = callbackArr.onbytarget || null;

    // termlist keys
    __myself.termKeys = callbackArr.termlist || "subject";
    
    // some configurational stuff
    __myself.pz2String = "search.pz2";
    __myself.keepAlive = 50000;

    __myself.sessionID = null;
    __myself.initStatusOK = false;
    __myself.pingStatusOK = false;
    __myself.searchStatusOK = false;

    if ( keepAlive < __myself.keepAlive )
        __myself.keepAlive = keepAlive;

    // for sorting
    __myself.currentSort = "relevance";
    // where are we?
    __myself.currentStart = 0;
    __myself.currentNum = 20;

    // last full record retrieved
    __myself.currRecID = null;
    // current query
    __myself.currQuery = null;

    //timers
    __myself.statTime = 2000;
    __myself.statTimer = null;
    __myself.termTime = 1000;
    __myself.termTimer = null;
    __myself.showTime = 1000;
    __myself.showTimer = null;
    __myself.bytargetTime = 1000;
    __myself.bytargetTimer = null;

    // active clients, updated by stat and show
    // might be an issue since bytarget will poll accordingly
    __myself.activeClients = 1;

    // error handling
    $(document).ajaxError( 
    function (request, settings, exception) {
        if ( settings.responseXML && settings.responseXML.getElementsByTagName("error") )
            throw new Error( settings.responseXML.getElementsByTagName("error")[0].childNodes[0].nodeValue);
    });
    
    // auto init session?
    if (autoInit !== false)
        __myself.init(__myself.keepAlive);
}
pz2.prototype = {
    init: function(keepAlive) 
    {
        if ( keepAlive < __myself.keepAlive )
            __myself.keepAlive = keepAlive;  
        
        $.get( __myself.pz2String,
            { "command": "init" },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    __myself.initStatusOK = true;
                    __myself.sessionID = data.getElementsByTagName("session")[0].childNodes[0].nodeValue;
                    setTimeout(__myself.ping, __myself.keepAlive);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    setTimeout("__myself.init()", 1000)
            }
        );
    },
    // no need to ping explicitly
    ping: function() 
    {
        if( !__myself.initStatusOK )
            return;
            // session is not initialized code here

        $.get( __myself.pz2String,
            { "command": "ping", "session": __myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    __myself.pingStatusOK = true;
                    setTimeout("__myself.ping()", __myself.keepAlive);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    setTimeout("__myself.ping()", 1000)
            }
        );
    },
    search: function(query, num, sort)
    {
        clearTimeout(__myself.statTimer);
        clearTimeout(__myself.showTimer);
        clearTimeout(__myself.termTimer);
        clearTimeout(__myself.bytargetTimer);

        if( !__myself.initStatusOK )
            return;
        
        if( query !== undefined )
            __myself.currQuery = query;
        else
            throw new Error("You need to supply query to the search command");
        
        $.get( __myself.pz2String,
            { "command": "search", "session": __myself.sessionID, "query": __myself.currQuery },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    __myself.searchStatusOK = true;
                    //piggyback search
                    __myself.show(0, num, sort)
                    if ( __myself.statCallback )
                        __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime / 2);
                    if ( __myself.termlistCallback )
                        __myself.termTimer = setTimeout("__myself.termlist()", __myself.termTime / 2);
                    if ( __myself.bytargetCallback )
                        __myself.bytargetTimer = setTimeout("__myself.bytarget()", __myself.bytargetTime / 2);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    setTimeout("__myself.search(__myself.currQuery)", 1000);
            }
        );
    },
    stat: function()
    {
        if( !__myself.searchStatusOK )
            return;
        // if called explicitly takes precedence
        clearTimeout(__myself.statTimer);

        $.get( __myself.pz2String,
            { "command": "stat", "session": __myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("stat") ) {
                    var activeClients = Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue );
                    __myself.activeClients = activeClients;
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
                    __myself.statCallback(stat);
                    if (activeClients > 0)
                        __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime); 
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime / 4)
            }
        );
    },
    show: function(start, num, sort)
    {
        if( !__myself.searchStatusOK )
            return;
        // if called explicitly takes precedence
        clearTimeout(__myself.showTimer);
        
        if( sort !== undefined )
            __myself.currentSort = sort;
        if( start !== undefined )
            __myself.currentStart = Number( start );
        if( num !== undefined )
            __myself.currentNum = Number( num );
        
        $.get( __myself.pz2String,
            { "command": "show", "session": __myself.sessionID, "start": __myself.currentStart,
              "num": __myself.currentNum, "sort": __myself.currentSort, "block": 1 },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    // first parse the status data send along with records
                    // this is strictly bound to the format
                    var activeClients = Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue );
                    var show = {
                    "activeclients": activeClients,
                    "merged": Number( data.getElementsByTagName("merged")[0].childNodes[0].nodeValue ),
                    "total": Number( data.getElementsByTagName("total")[0].childNodes[0].nodeValue ),
                    "start": Number( data.getElementsByTagName("start")[0].childNodes[0].nodeValue ),
                    "num": Number( data.getElementsByTagName("num")[0].childNodes[0].nodeValue ),
                    "hits": []
                    }
                    // parse all the first-level nodes for all <hit> tags
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
                    __myself.showCallback(show);
                    if (activeClients > 0)
                        __myself.showTimer = setTimeout("__myself.show()", __myself.showTime);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    __myself.showTimer = setTimeout("__myself.show()", __myself.showTime / 4);
            }
        );
    },
    record: function(id)
    {
        if( !__myself.searchStatusOK )
            return;

        if( id !== undefined )
            __myself.currRecID = id;

        $.get( __myself.pz2String,
            { "command": "record", "session": __myself.sessionID, "id": __myself.currRecID },
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
                    __myself.recordCallback(record);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    setTimeout("__myself.record(__myself.currRecID)", 1000);
            }
        );
    },
    termlist: function()
    {
        if( !__myself.searchStatusOK )
            return;
        // if called explicitly takes precedence
        clearTimeout(__myself.termTimer);

        $.get( __myself.pz2String,
            { "command": "termlist", "session": __myself.sessionID, "name": __myself.termKeys },
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
                    __myself.termlistCallback(termList);
                    if (termList["activeclients"] > 0)
                        __myself.termTimer = setTimeout("__myself.termlist()", __myself.termTime); 
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    __myself.termTimer = setTimeout("__myself.termlist()", __myself.termTime / 4); 
            }
        );

    },
    bytarget: function()
    {
        if( !__myself.searchStatusOK )
            return;
        // if called explicitly takes precedence
        clearTimeout(__myself.bytargetTimer);

        $.get( __myself.pz2String,
            { "command": "bytarget", "session": __myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    var targetNodes = data.getElementsByTagName("target");
                    var bytarget = new Array();
                    for ( i = 0; i < targetNodes.length; i++) {
                        bytarget[i] = new Array();
                        for( j = 0; j < targetNodes[i].childNodes.length; j++ ) {
                            if ( targetNodes[i].childNodes[j].nodeType == Node.ELEMENT_NODE ) {
                                var nodeName = targetNodes[i].childNodes[j].nodeName;
                                var nodeText = targetNodes[i].childNodes[j].firstChild.nodeValue;
                                bytarget[i][nodeName] = nodeText;
                            }
                        }
                    }
                    __myself.bytargetCallback(bytarget);
                    if ( __myself.activeClients > 0 )
                        __myself.bytargetTimer = setTimeout("__myself.bytarget()", __myself.bytargetTime);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    __myself.bytargetTimer = setTimeout("__myself.bytarget()", __myself.bytargetTime / 4);
            }
        );
    }
};
}
