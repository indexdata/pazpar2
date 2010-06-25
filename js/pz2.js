/*
 * Mine
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

var pz2 = function ( paramArray )
{
    
    // at least one callback required
    if ( !paramArray )
        throw new Error("Pz2.js: Array with parameters has to be suplied."); 

    //supported pazpar2's protocol version
    this.suppProtoVer = '1';
    if (typeof paramArray.pazpar2path != "undefined")
        this.pz2String = paramArray.pazpar2path;
    else
        this.pz2String = "/pazpar2/search.pz2";
    this.useSessions = true;
    
    this.stylesheet = paramArray.detailstylesheet || null;
    //load stylesheet if required in async mode
    if( this.stylesheet ) {
        var context = this;
        var request = new pzHttpRequest( this.stylesheet );
        request.get( {}, function ( doc ) { context.xslDoc = doc; } );
    }
    
    this.errorHandler = paramArray.errorhandler || null;
    this.showResponseType = paramArray.showResponseType || "xml";
    
    // function callbacks
    this.initCallback = paramArray.oninit || null;
    this.statCallback = paramArray.onstat || null;
    this.showCallback = paramArray.onshow || null;
    this.termlistCallback = paramArray.onterm || null;
    this.recordCallback = paramArray.onrecord || null;
    this.bytargetCallback = paramArray.onbytarget || null;
    this.resetCallback = paramArray.onreset || null;

    // termlist keys
    this.termKeys = paramArray.termlist || "subject";
    
    // some configurational stuff
    this.keepAlive = 50000;
    
    if ( paramArray.keepAlive < this.keepAlive )
        this.keepAlive = paramArray.keepAlive;

    this.sessionID = null;
    this.serviceId = paramArray.serviceId || null;
    this.initStatusOK = false;
    this.pingStatusOK = false;
    this.searchStatusOK = false;
    
    // for sorting
    this.currentSort = "relevance";

    // where are we?
    this.currentStart = 0;
    this.currentNum = 20;

    // last full record retrieved
    this.currRecID = null;
    
    // current query
    this.currQuery = null;

    //current raw record offset
    this.currRecOffset = null;

    //timers
    this.statTime = paramArray.stattime || 1000;
    this.statTimer = null;
    this.termTime = paramArray.termtime || 1000;
    this.termTimer = null;
    this.showTime = paramArray.showtime || 1000;
    this.showTimer = null;
    this.showFastCount = 4;
    this.bytargetTime = paramArray.bytargettime || 1000;
    this.bytargetTimer = null;
    this.recordTime = paramArray.recordtime || 500;
    this.recordTimer = null;

    // counters for each command and applied delay
    this.dumpFactor = 500;
    this.showCounter = 0;
    this.termCounter = 0;
    this.statCounter = 0;
    this.bytargetCounter = 0;
    this.recordCounter = 0;

    // active clients, updated by stat and show
    // might be an issue since bytarget will poll accordingly
    this.activeClients = 1;

    // if in proxy mode no need to init
    if (paramArray.usesessions != undefined) {
         this.useSessions = paramArray.usesessions;
        this.initStatusOK = true;
    }
    // else, auto init session or wait for a user init?
    if (this.useSessions && paramArray.autoInit !== false) {
        this.init(this.sessionId, this.serviceId);
    }
};

pz2.prototype = 
{
    //error handler for async error throws
   throwError: function (errMsg, errCode)
   {
        var err = new Error(errMsg);
        if (errCode) err.code = errCode;
                
        if (this.errorHandler) {
            this.errorHandler(err);
        }
        else {
            throw err;
        }
   },

    // stop activity by clearing tiemouts 
   stop: function ()
   {
       clearTimeout(this.statTimer);
       clearTimeout(this.showTimer);
       clearTimeout(this.termTimer);
       clearTimeout(this.bytargetTimer);
    },
    
    // reset status variables
    reset: function ()
    {   
        if ( this.useSessions ) {
            this.sessionID = null;
            this.initStatusOK = false;
            this.pingStatusOK = false;
        }
        this.searchStatusOK = false;
        this.stop();
            
        if ( this.resetCallback )
                this.resetCallback();
    },

    init: function (sessionId, serviceId) 
    {
        this.reset();
        
        // session id as a param
        if (sessionId && this.useSessions ) {
            this.initStatusOK = true;
            this.sessionID = sessionId;
            this.ping();
        // old school direct pazpar2 init
        } else if (this.useSessions) {
            var context = this;
            var request = new pzHttpRequest(this.pz2String, this.errorHandler);
            var opts = {'command' : 'init'};
            if (serviceId) opts.service = serviceId;
            request.safeGet(
                opts,
                function(data) {
                    if ( data.getElementsByTagName("status")[0]
                            .childNodes[0].nodeValue == "OK" ) {
                        if ( data.getElementsByTagName("protocol")[0]
                                .childNodes[0].nodeValue 
                            != context.suppProtoVer )
                            throw new Error(
                                "Server's protocol not supported by the client"
                            );
                        context.initStatusOK = true;
                        context.sessionID = 
                            data.getElementsByTagName("session")[0]
                                .childNodes[0].nodeValue;
                        setTimeout(
                            function () {
                                context.ping();
                            },
                            context.keepAlive
                        );
                        if ( context.initCallback )
                            context.initCallback();
                    }
                    else
                        context.throwError('Init failed. Malformed WS resonse.',
                                            110);
                }
            );
        // when through proxy no need to init
        } else {
            this.initStatusOK = true;
	}
    },
    // no need to ping explicitly
    ping: function () 
    {
        // pinging only makes sense when using pazpar2 directly
        if( !this.initStatusOK || !this.useSessions )
            throw new Error(
            'Pz2.js: Ping not allowed (proxy mode) or session not initialized.'
            );
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
            { "command": "ping", "session": this.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0]
                        .childNodes[0].nodeValue == "OK" ) {
                    context.pingStatusOK = true;
                    setTimeout(
                        function () {
                            context.ping();
                        }, 
                        context.keepAlive
                    );
                }
                else
                    context.throwError('Ping failed. Malformed WS resonse.',
                                        111);
            }
        );
    },
    search: function (query, num, sort, filter, showfrom, addParamsArr)
    {
        clearTimeout(this.statTimer);
        clearTimeout(this.showTimer);
        clearTimeout(this.termTimer);
        clearTimeout(this.bytargetTimer);
        
        this.showCounter = 0;
        this.termCounter = 0;
        this.bytargetCounter = 0;
        this.statCounter = 0;
        this.activeClients = 1;
        
        // no proxy mode
        if( !this.initStatusOK )
            throw new Error('Pz2.js: session not initialized.');
        
        if( query !== undefined )
            this.currQuery = query;
        else
            throw new Error("Pz2.js: no query supplied to the search command.");
        
        if ( showfrom !== undefined )
            var start = showfrom;
        else
            var start = 0;

	var searchParams = { 
            "command": "search",
            "query": this.currQuery, 
            "session": this.sessionID 
        };
	
        if (filter !== undefined)
	    searchParams["filter"] = filter;

        // copy additional parmeters, do not overwrite
        if (addParamsArr != undefined) {
            for (var prop in addParamsArr) {
                if (!searchParams.hasOwnProperty(prop))
                    searchParams[prop] = addParamsArr[prop];
            }
        }
        
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
            searchParams,
            function(data) {
                if ( data.getElementsByTagName("status")[0]
                        .childNodes[0].nodeValue == "OK" ) {
                    context.searchStatusOK = true;
                    //piggyback search
                    context.show(start, num, sort);
                    if (context.statCallback)
                        context.stat();
                    if (context.termlistCallback)
                        context.termlist();
                    if (context.bytargetCallback)
                        context.bytarget();
                }
                else
                    context.throwError('Search failed. Malformed WS resonse.',
                                        112);
            }
        );
    },
    stat: function()
    {
        if( !this.initStatusOK )
            throw new Error('Pz2.js: session not initialized.');
        
        // if called explicitly takes precedence
        clearTimeout(this.statTimer);
        
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
            { "command": "stat", "session": this.sessionID },
            function(data) {
                if ( data.getElementsByTagName("stat") ) {
                    var activeClients = 
                        Number( data.getElementsByTagName("activeclients")[0]
                                    .childNodes[0].nodeValue );
                    context.activeClients = activeClients;

		    var stat = Element_parseChildNodes(data.documentElement);

                    context.statCounter++;
		    var delay = context.statTime 
                        + context.statCounter * context.dumpFactor;
                    
                    if ( activeClients > 0 )
                        context.statTimer = 
                            setTimeout( 
                                function () {
                                    context.stat();
                                },
                                delay
                            );
                    context.statCallback(stat);
                }
                else
                    context.throwError('Stat failed. Malformed WS resonse.',
                                        113);
            }
        );
    },
    show: function(start, num, sort)
    {
        if( !this.searchStatusOK && this.useSessions )
            throw new Error(
                'Pz2.js: show command has to be preceded with a search command.'
            );
        
        // if called explicitly takes precedence
        clearTimeout(this.showTimer);
        
        if( sort !== undefined )
            this.currentSort = sort;
        if( start !== undefined )
            this.currentStart = Number( start );
        if( num !== undefined )
            this.currentNum = Number( num );

        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
          {
            "command": "show", 
            "session": this.sessionID, 
            "start": this.currentStart,
            "num": this.currentNum, 
            "sort": this.currentSort, 
            "block": 1,
            "type": this.showResponseType
          },
          function(data, type) {
            var show = null;
            var activeClients = 0;
            if (type === "json") {
              show = {};
              activeClients = Number(data.activeclients[0]);
              show.activeclients = activeClients;
              show.merged = Number(data.merged[0]);
              show.total = Number(data.total[0]);
              show.start = Number(data.start[0]);
              show.num = Number(data.num[0]);
              show.hits = data.hit;
            } else if (data.getElementsByTagName("status")[0]
                  .childNodes[0].nodeValue == "OK") {
                // first parse the status data send along with records
                // this is strictly bound to the format
                activeClients = 
                  Number(data.getElementsByTagName("activeclients")[0]
                      .childNodes[0].nodeValue);
                show = {
                  "activeclients": activeClients,
                  "merged": 
                    Number( data.getElementsByTagName("merged")[0]
                        .childNodes[0].nodeValue ),
                  "total": 
                    Number( data.getElementsByTagName("total")[0]
                        .childNodes[0].nodeValue ),
                  "start": 
                    Number( data.getElementsByTagName("start")[0]
                        .childNodes[0].nodeValue ),
                  "num": 
                    Number( data.getElementsByTagName("num")[0]
                        .childNodes[0].nodeValue ),
                  "hits": []
                };
                // parse all the first-level nodes for all <hit> tags
                var hits = data.getElementsByTagName("hit");
                for (i = 0; i < hits.length; i++)
                  show.hits[i] = Element_parseChildNodes(hits[i]);
            } else {
              context.throwError('Show failed. Malformed WS resonse.',
                  114);
            }
            context.activeClients = activeClients; 
            context.showCounter++;
            var delay = context.showTime;
            if (context.showCounter > context.showFastCount)
              delay += context.showCounter * context.dumpFactor;
            if ( activeClients > 0 )
              context.showTimer = setTimeout(
                function () {
                  context.show();
                }, 
                delay);
            context.showCallback(show);
          }
        );
    },
    record: function(id, offset, syntax, handler)
    {
        // we may call record with no previous search if in proxy mode
        if(!this.searchStatusOK && this.useSessions)
           throw new Error(
            'Pz2.js: record command has to be preceded with a search command.'
            );
        
        if( id !== undefined )
            this.currRecID = id;
        
	var recordParams = { 
            "command": "record", 
            "session": this.sessionID,
            "id": this.currRecID 
        };
	
	this.currRecOffset = null;
        if (offset != undefined) {
	    recordParams["offset"] = offset;
            this.currRecOffset = offset;
        }

        if (syntax != undefined)
            recordParams['syntax'] = syntax;

        //overwrite default callback id needed
        var callback = this.recordCallback;
        var args = undefined;
        if (handler != undefined) {
            callback = handler['callback'];
            args = handler['args'];
        }
        
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);

        request.safeGet(
	    recordParams,
            function(data) {
                var recordNode;
                var record;                                
                //raw record
                if (context.currRecOffset !== null) {
                    record = new Array();
                    record['xmlDoc'] = data;
                    record['offset'] = context.currRecOffset;
                    callback(record, args);
                //pz2 record
                } else if ( recordNode = 
                    data.getElementsByTagName("record")[0] ) {
                    // if stylesheet was fetched do not parse the response
                    if ( context.xslDoc ) {
                        record = new Array();
                        record['xmlDoc'] = data;
                        record['xslDoc'] = context.xslDoc;
                        record['recid'] = 
                            recordNode.getElementsByTagName("recid")[0]
                                .firstChild.nodeValue;
                    //parse record
                    } else {
                        record = Element_parseChildNodes(recordNode);
                    }    
		    var activeClients = 
		       Number( data.getElementsByTagName("activeclients")[0]
		 		.childNodes[0].nodeValue );
		    context.activeClients = activeClients; 
                    context.recordCounter++;
                    var delay = context.recordTime + context.recordCounter * context.dumpFactor;
                    if ( activeClients > 0 )
                        context.recordTimer = 
                           setTimeout ( 
                               function() {
                                  context.record(id, offset, syntax, handler);
                                  },
                                  delay
                               );                                    
                    callback(record, args);
                }
                else
                    context.throwError('Record failed. Malformed WS resonse.',
                                        115);
            }
        );
    },

    termlist: function()
    {
        if( !this.searchStatusOK && this.useSessions )
            throw new Error(
            'Pz2.js: termlist command has to be preceded with a search command.'
            );

        // if called explicitly takes precedence
        clearTimeout(this.termTimer);
        
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
            { 
                "command": "termlist", 
                "session": this.sessionID, 
                "name": this.termKeys 
            },
            function(data) {
                if ( data.getElementsByTagName("termlist") ) {
                    var activeClients = 
                        Number( data.getElementsByTagName("activeclients")[0]
                                    .childNodes[0].nodeValue );
                    context.activeClients = activeClients;
                    var termList = { "activeclients":  activeClients };
                    var termLists = data.getElementsByTagName("list");
                    //for each termlist
                    for (i = 0; i < termLists.length; i++) {
                	var listName = termLists[i].getAttribute('name');
                        termList[listName] = new Array();
                        var terms = termLists[i].getElementsByTagName('term');
                        //for each term in the list
                        for (j = 0; j < terms.length; j++) { 
                            var term = {
                                "name": 
                                    (terms[j].getElementsByTagName("name")[0]
                                        .childNodes.length 
                                    ? terms[j].getElementsByTagName("name")[0]
                                        .childNodes[0].nodeValue
                                    : 'ERROR'),
                                "freq": 
                                    terms[j]
                                    .getElementsByTagName("frequency")[0]
                                    .childNodes[0].nodeValue || 'ERROR'
                            };

                            var termIdNode = 
                                terms[j].getElementsByTagName("id");
                            if(terms[j].getElementsByTagName("id").length)
                                term["id"] = 
                                    termIdNode[0].childNodes[0].nodeValue;
                            termList[listName][j] = term;
                        }
                    }

                    context.termCounter++;
                    var delay = context.termTime 
                        + context.termCounter * context.dumpFactor;
                    if ( activeClients > 0 )
                        context.termTimer = 
                            setTimeout(
                                function () {
                                    context.termlist();
                                }, 
                                delay
                            );
                   
                   context.termlistCallback(termList);
                }
                else
                    context.throwError('Termlist failed. Malformed WS resonse.',
                                        116);
            }
        );

    },
    bytarget: function()
    {
        if( !this.initStatusOK && this.useSessions )
            throw new Error(
            'Pz2.js: bytarget command has to be preceded with a search command.'
            );
        
        // no need to continue
        if( !this.searchStatusOK )
            return;

        // if called explicitly takes precedence
        clearTimeout(this.bytargetTimer);
        
        var context = this;
        var request = new pzHttpRequest(this.pz2String, this.errorHandler);
        request.safeGet(
            { "command": "bytarget", "session": this.sessionID },
            function(data) {
                if ( data.getElementsByTagName("status")[0]
                        .childNodes[0].nodeValue == "OK" ) {
                    var targetNodes = data.getElementsByTagName("target");
                    var bytarget = new Array();
                    for ( i = 0; i < targetNodes.length; i++) {
                        bytarget[i] = new Array();
                        for( j = 0; j < targetNodes[i].childNodes.length; j++ ) {
                            if ( targetNodes[i].childNodes[j].nodeType 
                                == Node.ELEMENT_NODE ) {
                                var nodeName = 
                                    targetNodes[i].childNodes[j].nodeName;
                                var nodeText = 
                                    targetNodes[i].childNodes[j]
                                        .firstChild.nodeValue;
                                bytarget[i][nodeName] = nodeText;
                            }
                        }
                    }
                    
                    context.bytargetCounter++;
                    var delay = context.bytargetTime 
                        + context.bytargetCounter * context.dumpFactor;
                    if ( context.activeClients > 0 )
                        context.bytargetTimer = 
                            setTimeout(
                                function () {
                                    context.bytarget();
                                }, 
                                delay
                            );

                    context.bytargetCallback(bytarget);
                }
                else
                    context.throwError('Bytarget failed. Malformed WS resonse.',
                                        117);
            }
        );
    },
    
    // just for testing, probably shouldn't be here
    showNext: function(page)
    {
        var step = page || 1;
        this.show( ( step * this.currentNum ) + this.currentStart );     
    },

    showPrev: function(page)
    {
        if (this.currentStart == 0 )
            return false;
        var step = page || 1;
        var newStart = this.currentStart - (step * this.currentNum );
        this.show( newStart > 0 ? newStart : 0 );
    },

    showPage: function(pageNum)
    {
        //var page = pageNum || 1;
        this.show(pageNum * this.currentNum);
    }
};

/*
********************************************************************************
** AJAX HELPER CLASS ***********************************************************
********************************************************************************
*/
var pzHttpRequest = function ( url, errorHandler ) {
        this.maxUrlLength = 2048;
        this.request = null;
        this.url = url;
        this.errorHandler = errorHandler || null;
        this.async = true;
        this.requestHeaders = {};
        
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
    safeGet: function ( params, callback )
    {
        var encodedParams =  this.encodeParams(params);
        var url = this._urlAppendParams(encodedParams);
        if (url.length >= this.maxUrlLength) {
            this.requestHeaders["Content-Type"]
                = "application/x-www-form-urlencoded";
            this._send( 'POST', this.url, encodedParams, callback );
        } else {
            this._send( 'GET', url, '', callback );
        }
    },

    get: function ( params, callback ) 
    {
        this._send( 'GET', this._urlAppendParams(this.encodeParams(params)), 
            '', callback );
    },

    post: function ( params, data, callback )
    {
        this._send( 'POST', this._urlAppendParams(this.encodeParams(params)), 
            data, callback );
    },

    load: function ()
    {
        this.async = false;
        this.request.open( 'GET', this.url, this.async );
        this.request.send('');
        if ( this.request.status == 200 )
            return this.request.responseXML;
    },

    encodeParams: function (params)
    {
        var sep = "";
        var encoded = "";
        for (var key in params) {
            if (params[key] != null) {
                encoded += sep + key + '=' + encodeURIComponent(params[key]);
                sep = '&';
            }
        }
        return encoded;
    },

    _send: function ( type, url, data, callback)
    {
        var context = this;
        this.callback = callback;
        this.async = true;
        this.request.open( type, url, this.async );
        for (var key in this.requestHeaders)
            this.request.setRequestHeader(key, this.requestHeaders[key]);
        this.request.onreadystatechange = function () {
            context._handleResponse(url); /// url used ONLY for error reporting
        }
        this.request.send(data);
    },

    _urlAppendParams: function (encodedParams)
    {
        if (encodedParams)
            return this.url + "?" + encodedParams;
        else
            return this.url;
    },

    _handleResponse: function (savedUrlForErrorReporting)
    {
        if ( this.request.readyState == 4 ) { 
            // pick up appplication errors first
            var errNode = null;
            if (this.request.responseXML &&
                (errNode = this.request.responseXML.documentElement)
                && errNode.nodeName == 'error') {
                var errMsg = errNode.getAttribute("msg");
                var errCode = errNode.getAttribute("code");
                var errAddInfo = '';
                if (errNode.childNodes.length)
                    errAddInfo = ': ' + errNode.childNodes[0].nodeValue;
                           
                var err = new Error(errMsg + errAddInfo);
                err.code = errCode;
	    
                if (this.errorHandler) {
                    this.errorHandler(err);
                }
                else {
                    throw err;
                }
            } else if (this.request.status == 200 && 
                       this.request.responseXML == null) {
              if (this.request.responseText != null) {
                //assume JSON
                
		var json = null; 
		if (this.JSON == null)
		    json = eval("(" + this.request.responseText + ")");
		else { 
		    try	{
		    	json = JSON.parse(this.request.responseText, null);
		    }
		    catch (e) {
			    json = eval("(" + this.request.responseText + ")");
		    }
		this.callback(json, "json");
              } else {
                var err = new Error("XML response is empty but no error " +
                                    "for " + savedUrlForErrorReporting);
                err.code = -1;
                if (this.errorHandler) {
                    this.errorHandler(err);
                } else {
                    throw err;
                }
              }
            } else if (this.request.status == 200) {
                this.callback(this.request.responseXML);
            } else {
                var err = new Error("HTTP response not OK: " 
                            + this.request.status + " - " 
                            + this.request.statusText );
                err.code = '00' + this.request.status;        
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
********************************************************************************
** XML HELPER FUNCTIONS ********************************************************
********************************************************************************
*/

// DOMDocument

if ( window.ActiveXObject) {
    var DOMDoc = document;
} else {
    var DOMDoc = Document.prototype;
}

DOMDoc.newXmlDoc = function ( root )
{
    var doc;

    if (document.implementation && document.implementation.createDocument) {
        doc = document.implementation.createDocument('', root, null);
    } else if ( window.ActiveXObject ) {
        doc = new ActiveXObject("MSXML2.DOMDocument");
        doc.loadXML('<' + root + '/>');
    } else {
        throw new Error ('No XML support in this browser');
    }

    return doc;
}

   
DOMDoc.parseXmlFromString = function ( xmlString ) 
{
    var doc;

    if ( window.DOMParser ) {
        var parser = new DOMParser();
        doc = parser.parseFromString( xmlString, "text/xml");
    } else if ( window.ActiveXObject ) {
        doc = new ActiveXObject("MSXML2.DOMDocument");
        doc.loadXML( xmlString );
    } else {
        throw new Error ("No XML parsing support in this browser.");
    }

    return doc;
}

DOMDoc.transformToDoc = function (xmlDoc, xslDoc)
{
    if ( window.XSLTProcessor ) {
        var proc = new XSLTProcessor();
        proc.importStylesheet( xslDoc );
        return proc.transformToDocument(xmlDoc);
    } else if ( window.ActiveXObject ) {
        return document.parseXmlFromString(xmlDoc.transformNode(xslDoc));
    } else {
        alert( 'Unable to perform XSLT transformation in this browser' );
    }
}
 
// DOMElement

Element_removeFromDoc = function (DOM_Element)
{
    DOM_Element.parentNode.removeChild(DOM_Element);
}

Element_emptyChildren = function (DOM_Element)
{
    while( DOM_Element.firstChild ) {
        DOM_Element.removeChild( DOM_Element.firstChild )
    }
}

Element_appendTransformResult = function ( DOM_Element, xmlDoc, xslDoc )
{
    if ( window.XSLTProcessor ) {
        var proc = new XSLTProcessor();
        proc.importStylesheet( xslDoc );
        var docFrag = false;
        docFrag = proc.transformToFragment( xmlDoc, DOM_Element.ownerDocument );
        DOM_Element.appendChild(docFrag);
    } else if ( window.ActiveXObject ) {
        DOM_Element.innerHTML = xmlDoc.transformNode( xslDoc );
    } else {
        alert( 'Unable to perform XSLT transformation in this browser' );
    }
}
 
Element_appendTextNode = function (DOM_Element, tagName, textContent )
{
    var node = DOM_Element.ownerDocument.createElement(tagName);
    var text = DOM_Element.ownerDocument.createTextNode(textContent);

    DOM_Element.appendChild(node);
    node.appendChild(text);

    return node;
}

Element_setTextContent = function ( DOM_Element, textContent )
{
    if (typeof DOM_Element.textContent !== "undefined") {
        DOM_Element.textContent = textContent;
    } else if (typeof DOM_Element.innerText !== "undefined" ) {
        DOM_Element.innerText = textContent;
    } else {
        throw new Error("Cannot set text content of the node, no such method.");
    }
}

Element_getTextContent = function (DOM_Element)
{
    if ( typeof DOM_Element.textContent != 'undefined' ) {
        return DOM_Element.textContent;
    } else if (typeof DOM_Element.text != 'undefined') {
        return DOM_Element.text;
    } else {
        throw new Error("Cannot get text content of the node, no such method.");
    }
}

Element_parseChildNodes = function (node)
{
    var parsed = {};
    var hasChildElems = false;

    if (node.hasChildNodes()) {
        var children = node.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == Node.ELEMENT_NODE) {
                hasChildElems = true;
                var nodeName = child.nodeName; 
                if (!(nodeName in parsed))
                    parsed[nodeName] = [];
                parsed[nodeName].push(Element_parseChildNodes(child));
            }
        }
    }

    var attrs = node.attributes;
    for (var i = 0; i < attrs.length; i++) {
        var attrName = '@' + attrs[i].nodeName;
        var attrValue = attrs[i].nodeValue;
        parsed[attrName] = attrValue;
    }

    // if no nested elements, get text content
    if (node.hasChildNodes() && !hasChildElems) {
        if (node.attributes.length) 
            parsed['#text'] = node.firstChild.nodeValue;
        else
            parsed = node.firstChild.nodeValue;
    }
    
    return parsed;
}

/* do not remove trailing bracket */
}
