/*
** $Id: pz2.js,v 1.16 2007-05-17 21:00:09 jakub Exp $
** pz2.js - pazpar2's javascript client library.
*/

//since explorer is flawed
if (!window['Node']) {
    window.Node = new Object();
    Node.ELEMENT_NODE = 1;
    Node.ATTRIBUTE_NODE = 2;
    Node.TEXT_NODE = 3;
    Node.CDATA_SECTION_NODE = 4;
    Node.ENTITY_REFERENCE_NODE = 5;
    Node.ENTITY_NODE = 6;
    Node.PROCESSING_INSTRUCTION_NODE = 7;
    Node.COMMENT_NODE = 8;
    Node.DOCUMENT_NODE = 9;
    Node.DOCUMENT_TYPE_NODE = 10;
    Node.DOCUMENT_FRAGMENT_NODE = 11;
    Node.NOTATION_NODE = 12;
}

// prevent execution of more than once
if(typeof window.pz2 == "undefined") {
window.undefined = window.undefined;

var pz2 = function(paramArray) {
    //for convenience
    __myself = this;

    //supported pazpar2's protocol version
    __myself.suppProtoVer = '1';
    __myself.pz2String = "search.pz2";
    __myself.stylesheet = paramArray.detailstylesheet || null;

    //load stylesheet if required in async mode
    if( __myself.stylesheet ) {
        var request = new pzHttpRequest( __myself.stylesheet );
        request.get(
                {},
                function ( doc ) {
                    __myself.xslDoc = doc;
                }
        );
    }

    // at least one callback required
    if ( !paramArray )
        throw new Error("An array with parameters has to be suplied when instantiating a class");
    
    __myself.errorHandler = paramArray.errorhandler || null;
    
    // function callbacks
    __myself.statCallback = paramArray.onstat || null;
    __myself.showCallback = paramArray.onshow || null;
    __myself.termlistCallback = paramArray.onterm || null;
    __myself.recordCallback = paramArray.onrecord || null;
    __myself.bytargetCallback = paramArray.onbytarget || null;
    __myself.resetCallback = paramArray.onreset || null;

    // termlist keys
    __myself.termKeys = paramArray.termlist || "subject";
    
    // some configurational stuff
    __myself.keepAlive = 50000;

    __myself.sessionID = null;
    __myself.initStatusOK = false;
    __myself.pingStatusOK = false;
    __myself.searchStatusOK = false;

    if ( paramArray.keepAlive < __myself.keepAlive )
        __myself.keepAlive = paramArray.keepAlive;

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
    __myself.statTime = paramArray.stattime || 2000;
    __myself.statTimer = null;
    __myself.termTime = paramArray.termtime || 1000;
    __myself.termTimer = null;
    __myself.showTime = paramArray.showtime || 1000;
    __myself.showTimer = null;
    __myself.showFastCount = 4;
    __myself.bytargetTime = paramArray.bytargettime || 1000;
    __myself.bytargetTimer = null;

    //useful?
    __myself.dumpFactor = 500;
    __myself.showCounter = 0;
    __myself.termCounter = 0;

    // active clients, updated by stat and show
    // might be an issue since bytarget will poll accordingly
    __myself.activeClients = 1;
    
    // auto init session?
    if (paramArray.autoInit !== false)
        __myself.init();
};
pz2.prototype = {
    reset: function ()
    {
        __myself.sessionID = null;
        __myself.initStatusOK = false;
        __myself.pingStatusOK = false;
        __myself.searchStatusOK = false;

        clearTimeout(__myself.statTimer);
        clearTimeout(__myself.showTimer);
        clearTimeout(__myself.termTimer);
        clearTimeout(__myself.bytargetTimer);
            
        if ( __myself.resetCallback )
                __myself.resetCallback();
    },
    init: function ( sessionId ) 
    {
        __myself.reset();
        if ( sessionId != undefined ) {
            __myself.initStatusOK = true;
            __myself.sessionID = sessionId;
            __myself.ping();

        } else {
            var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
            request.get(
                { "command": "init" },
                function(data) {
                    if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                        if ( data.getElementsByTagName("protocol")[0].childNodes[0].nodeValue != __myself.suppProtoVer )
                            throw new Error("Server's protocol not supported by the client");
                        __myself.initStatusOK = true;
                        __myself.sessionID = data.getElementsByTagName("session")[0].childNodes[0].nodeValue;
                        setTimeout("__myself.ping()", __myself.keepAlive);
                    }
                    else
                        // if it gets here the http return code was 200 (pz2 errors are 417)
                        // but the response was invalid, it should never occur
                        setTimeout("__myself.init()", 1000);
                }
            );
        }
    },
    // no need to ping explicitly
    ping: function () 
    {
        if( !__myself.initStatusOK )
            return;
            // session is not initialized code here
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
            { "command": "ping", "session": __myself.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    __myself.pingStatusOK = true;
                    setTimeout("__myself.ping()", __myself.keepAlive);
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    setTimeout("__myself.ping()", 1000);
            }
        );
    },
    search: function (query, num, sort, filter)
    {
        clearTimeout(__myself.statTimer);
        clearTimeout(__myself.showTimer);
        clearTimeout(__myself.termTimer);
        clearTimeout(__myself.bytargetTimer);
        
        __myself.showCounter = 0;
        __myself.termCounter = 0;
        
        if( !__myself.initStatusOK )
            return;
        
        if( query !== undefined )
            __myself.currQuery = query;
        else
            throw new Error("You need to supply query to the search command");

        if( filter !== undefined )
            var searchParams = { "command": "search", "session": __myself.sessionID, "query": __myself.currQuery, "filter": filter };
        else
            var searchParams = { "command": "search", "session": __myself.sessionID, "query": __myself.currQuery };
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
            searchParams,
            function(data) {
                if ( data.getElementsByTagName("status")[0].childNodes[0].nodeValue == "OK" ) {
                    __myself.searchStatusOK = true;
                    //piggyback search
                    __myself.show(0, num, sort);
                    if ( __myself.statCallback )
                        __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime / 2);
                    if ( __myself.termlistCallback )
                        //__myself.termlist();
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
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
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
                    };
                    __myself.statCallback(stat);
                    if (activeClients > 0)
                        __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime); 
                }
                else
                    // if it gets here the http return code was 200 (pz2 errors are 417)
                    // but the response was invalid, it should never occur
                    __myself.statTimer = setTimeout("__myself.stat()", __myself.statTime / 4);
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
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        var context = this;
        request.get(
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
                    };
                    // parse all the first-level nodes for all <hit> tags
                    var hits = data.getElementsByTagName("hit");
                    var hit = new Array();
                    for (i = 0; i < hits.length; i++) {
                        show.hits[i] = new Array();
			show.hits[i]['location'] = new Array();
                        for ( j = 0; j < hits[i].childNodes.length; j++) {
			    var locCount = 0;
                            if ( hits[i].childNodes[j].nodeType == Node.ELEMENT_NODE ) {
				if (hits[i].childNodes[j].nodeName == 'location') {
				    var locNode = hits[i].childNodes[j];
				    var id = locNode.getAttribute('id');
				    show.hits[i]['location'][id] = {
					"id": locNode.getAttribute("id"),
					"name": locNode.getAttribute("name")
				    };
				}
				else {
				    var nodeName = hits[i].childNodes[j].nodeName;
                                    var nodeText = hits[i].childNodes[j].firstChild.nodeValue;
				    show.hits[i][nodeName] = nodeText;
				}
                            }
                        }
                    }
                    __myself.showCallback(show);
                    __myself.showCounter++;
		    var delay = __myself.showTime;
		    if (__myself.showCounter > __myself.showFastCount)
			    //delay *= 2;
                            delay += __myself.showCounter * __myself.dumpFactor;
                    if (activeClients > 0)
                        __myself.showTimer = setTimeout("__myself.show()", delay);
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
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
            { "command": "record", "session": __myself.sessionID, "id": __myself.currRecID },
            function(data) {
                var recordNode;
                var record = new Array();
                if ( recordNode = data.getElementsByTagName("record")[0] ) {
                    // if stylesheet was fetched do not parse the response
                    if ( __myself.xslDoc ) {
                        record['xmlDoc'] = data;
                        record['xslDoc'] = __myself.xslDoc;
                    } else {
                        for ( i = 0; i < recordNode.childNodes.length; i++) {
                            if ( recordNode.childNodes[i].nodeType == Node.ELEMENT_NODE ) {
                                var nodeName = recordNode.childNodes[i].nodeName;
                                var nodeText = recordNode.childNodes[i].firstChild.nodeValue;
                                record[nodeName] = nodeText;                            
                            }
                        }
                        // the location is hard coded
                        var locationNodes = recordNode.getElementsByTagName("location");
                        record["location"] = new Array();
                        for ( i = 0; i < locationNodes.length; i++ ) {
                            record["location"][i] = {
                                "id": locationNodes[i].getAttribute("id"),
                                "name": locationNodes[i].getAttribute("name")
                            };
                            for ( j = 0; j < locationNodes[i].childNodes.length; j++) {
                                if ( locationNodes[i].childNodes[j].nodeType == Node.ELEMENT_NODE ) {
                                    var nodeName = locationNodes[i].childNodes[j].nodeName;
                                    var nodeText;
                                    if (locationNodes[i].childNodes[j].firstChild)
                                            nodeText = locationNodes[i].childNodes[j].firstChild.nodeValue;
                                    else
                                            nodeText = '';
                                    record["location"][i][nodeName] = nodeText;                            
                                }
                            }
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
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
            { "command": "termlist", "session": __myself.sessionID, "name": __myself.termKeys },
            function(data) {
                if ( data.getElementsByTagName("termlist") ) {
                    var termList = { "activeclients": Number( data.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue ) };
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
                                "freq": terms[j].getElementsByTagName("frequency")[0].childNodes[0].nodeValue
                            };

                            var termIdNode = terms[j].getElementsByTagName("id");
                            if(terms[j].getElementsByTagName("id").length)
                                term["id"] = termIdNode[0].childNodes[0].nodeValue;

                            termList[listName][j] = term;
                        }
                    }

                    __myself.termlistCallback(termList);
                    __myself.termCounter++;
                    if (termList["activeclients"] > 0)
                        __myself.termTimer = setTimeout("__myself.termlist()", (__myself.termTime + __myself.termCounter*__myself.dumpFactor)); 
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
        var request = new pzHttpRequest(__myself.pz2String, __myself.errorHandler);
        request.get(
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
    },
    // just for testing, probably shouldn't be here
    showNext: function(page)
    {
        var step = page || 1;
        __myself.show( ( step * __myself.currentNum ) + __myself.currentStart );     
    },
    showPrev: function(page)
    {
        if (__myself.currentStart == 0 )
            return false;
        var step = page || 1;
        var newStart = __myself.currentStart - (step * __myself.currentNum );
        __myself.show( newStart > 0 ? newStart : 0 );
    },
    showPage: function(pageNum)
    {
        //var page = pageNum || 1;
        __myself.show(pageNum * __myself.currentNum);
    }
};

/*
*********************************************************************************
** AJAX HELPER CLASS ************************************************************
*********************************************************************************
*/
var pzHttpRequest = function ( url, errorHandler ) {
        this.request = null;
        this.url = url;
        this.errorHandler = errorHandler || null;
        
        if ( window.XMLHttpRequest ) {
            this.request = new XMLHttpRequest();
        } else if ( window.ActiveXObject ) {
            try {
                this.request = new ActiveXObject( 'Msxml2.XMLHTTP' );
            } catch (err) {
                this.request = new ActiveXObject( 'Microsoft.XMLHTTP' );
            }
        }
};

pzHttpRequest.prototype = 
{
    get: function ( params, callback ) 
    {
        this.callback = callback;
        
        var getUrl = this.url;
        var paramArr = new Array();

        for ( var key in params ) {
            paramArr.push(key + '=' + escape(params[key]));
        }

        if ( paramArr.length )
            getUrl += '?' + paramArr.join('&');

        var context = this;
        this.request.open( 'GET', getUrl, true );
        this.request.onreadystatechange = function () {
            context._handleResponse();
        }
        this.request.send(null);
    },

    _handleResponse: function ()
    {
        if ( this.request.readyState == 4 ) {
            if ( this.request.status == 200 ) {
                this.callback( this.request.responseXML );
            }
            // pz errors
            else if ( this.request.status == 417 ) {
                var errMsg = this.request.responseXML.getElementsByTagName("error")[0].childNodes[0].nodeValue;
                var errCode = this.request.responseXML.getElementsByTagName("error")[0].getAttribute("code");
            
                var err = new Error(errMsg);
                err.code = errCode;
	    
                if (this.errorHandler) {
                    this.errorHandler(err);
                }
                else {
                    throw err;
                }
            }
            else {
                var err = new Error("XMLHttpRequest error. STATUS: " 
                            + this.request.status + " STATUS TEXT: " 
                            + this.request.statusText );
                err.code = 'HTTP';
                
                if (this.errorHandler) {
                    this.errorHandler(err);
                }
                else {
                    throw err;
                }
            }
        }
    }
};

/*
*********************************************************************************
** QUERY CLASS ******************************************************************
*********************************************************************************
*/
var pzQuery = function()
{
    this.simpleQuery = '';
    this.singleFilter = null;
    this.advTerms = new Array();
    this.filterHash = new Array();
    this.numTerms = 0;
    this.filterNums = 0;
};
pzQuery.prototype = {
    reset: function()
    {
        this.simpleQuery = '';
        this.advTerms = new Array();
        this.simpleFilter = null;
        this.numTerms = 0;
    },
    addTerm: function(field, value)
    {
        var term = {"field": field, "value": value};
        this.advTerms[this.numTerms] = term;
        this.numTerms++;
    },
    getTermValueByIdx: function(index)
    {
        return this.advTerms[index].value;
    },
    getTermFieldByIdx: function(index)
    {
        return this.advTerms[index].field;
    },
    /* semicolon separated list of terms for given field*/
    getTermsByField: function(field)
    {
        var terms = '';
        for(var i = 0; i < this.advTerms.length; i++)
        {
            if( this.advTerms[i].field == field )
                terms = terms + this.queryHas[i].value + ';';
        }
        return terms;
    },
    addTermsFromList: function(inputString, field)
    {
        var inputArr = inputString.split(';');
        for(var i=0; i < inputArr.length; i++)
        {
            if(inputArr[i].length < 3) continue;
            this.advTerms[this.numTerms] = {"field": field, "value": inputArr[i] };
            this.numTerms++;
        }
    },
    removeTermByIdx: function(index)
    {
        this.advTerms.splice(index, 1);
        this.numTerms--;
    },
    toCCL: function()
    {   
        var ccl = '';
        if( this.simpleQuery != '')
            ccl = '"'+this.simpleQuery+'"';
        for(var i = 0; i < this.advTerms.length; i++)
        {
            if (ccl != '') ccl = ccl + ' and ';
            ccl = ccl + this.advTerms[i].field+'="'+this.advTerms[i].value+'"';
        }
        return ccl;
    },
    addFilter: function(name, value)
    {
        var filter = {"name": name, "id": value };
        this.filterHash[this.filterHash.length] = filter;
        this.filterNums++
        return  this.filterHash.length - 1;
    },
    setFilter: function(name, value)
    {
        this.filterHash = new Array();
        this.filterNums = 0;
        this.addFilter(name, value);
    },
    getFilter: function(index)
    {
        return this.filterHash[index].id;
    },
    getFilterName: function(index)
    {
        return this.filterHash[index].name;
    },
    removeFilter: function(index)
    {
        delete this.filterHash[index];
        this.filterNums--;
    },
    clearFilter: function()
    {
        this.filterHash = new Array();
        this.filterNums = 0;
    },
    getFilterString: function()
    {
        //temporary
        if( this.singleFilter != null ) {
            return 'pz:id='+this.singleFilter.id;
        } 
        else if( this.filterNums <= 0 ) {
            return undefined;
        }

        var filter = 'pz:id=';
        for(var i = 0; i < this.filterHash.length; i++)
        {
            if (this.filterHash[i] == undefined) continue;
            if (filter > 'pz:id=') filter = filter + '|';            
            filter += this.filterHash[i].id; 
        }
        return filter;
    },
    totalLength: function()
    {
        var simpleLength = this.simpleQuery != '' ? 1 : 0;
        return this.advTerms.length + simpleLength;
    },
    clearSingleFilter: function()
    {
        this.singleFilter = null;
    },
    setSingleFilter: function(name, value)
    {
        this.singleFilter = {"name": name, "id": value };
    },
    getSingleFilterName: function()
    {
        return this.singleFilter.name;
    }
}

}
