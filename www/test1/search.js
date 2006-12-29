/* $Id: search.js,v 1.2 2006-12-29 10:29:46 sondberg Exp $
 * ---------------------------------------------------
 * Javascript container
 */

var xmlHttp
var xinitSession;
var xloadTargets;
var xsearch;
var xshow;
var xstat;
var xtermlist;
var session = false;
var targetsloaded = false;
var shown;
var searchtimer;
var showtimer;
var termtimer;
var stattimer;
var startrec;
var session_cells = Array('query');
var old_session = session_read();
var url_surveillence;


function initialize ()
{
    start_session();
    session_check();
}


function GetXmlHttpObject()
{ 
    var objXMLHttp=null
    if (window.XMLHttpRequest)
      {
      objXMLHttp=new XMLHttpRequest()
      }
    else if (window.ActiveXObject)
      {
      objXMLHttp=new ActiveXObject("Microsoft.XMLHTTP")
      }
    return objXMLHttp
} 

function session_started()
{
    if (xinitSession.readyState != 4)
	return;
    var xml = xinitSession.responseXML;
    var sesid = xml.getElementsByTagName("session")[0].childNodes[0].nodeValue;
    document.getElementById("status").innerHTML = "Live";
    session = sesid;
}

function start_session()
{
    xinitSession = GetXmlHttpObject();
    var url="search.pz2?";
    url += "command=init";
    xinitSession.onreadystatechange=session_started;
    xinitSession.open("GET", url);
    xinitSession.send(null);
    
    //url_surveillence = setInterval(session_check, 200);
}

function targets_loaded()
{
    if (xloadTargets.readyState != 4)
	return;
    var xml = xloadTargets.responseXML;
    var error = xml.getElementsByTagName("error");
    if (error[0])
    {
	var msg = error[0].childNodes[0].nodeValue;
	alert(msg);
	return;
    }
    document.getElementById("targetstatus").innerHTML = "Targets loaded";
}

function load_targets()
{
    var fn = document.getElementById("targetfilename").value;
    clearTimeout(termtimer);
    clearTimeout(searchtimer);
    clearTimeout(stattimer);
    clearTimeout(showtimer);
    document.getElementById("stat").innerHTML = "";
    if (!fn)
    {
	alert("Please enter a target definition file name");
	return;
    }
    var url="search.pz2?" +
    	"command=load" +
	"&session=" + session +
	"&name=" + fn;
    document.getElementById("targetstatus").innerHTML = "Loading targets...";
    xloadTargets = GetXmlHttpObject();
    xloadTargets.onreadystatechange=targets_loaded;
    xloadTargets.open("GET", url);
    xloadTargets.send(null);
}

function show_records()
{
    if (xshow.readyState != 4)
	return;
    var i;
    var xml = xshow.responseXML;
    var body = document.getElementById("body");
    var hits = xml.getElementsByTagName("hit");
    if (!hits[0]) // We should never get here with blocking operations
    {
	body.innerHTML = "No records yet";
	searchtimer = setTimeout(check_search, 250);
    }
    else
    {

	var total = Number(xml.getElementsByTagName('total')[0].childNodes[0].nodeValue);
	var merged = Number(xml.getElementsByTagName('merged')[0].childNodes[0].nodeValue);
	var start = Number(xml.getElementsByTagName('start')[0].childNodes[0].nodeValue);
	var num = Number(xml.getElementsByTagName('num')[0].childNodes[0].nodeValue);
	body.innerHTML = '<b>Records : ';
	body.innerHTML += (start + 1) + ' to ' + (start + num) +
		' of ' + merged + ' (total hits: ' + total + ')</b>';

	if (start + num < merged)
	    body.innerHTML += ' <a href="" ' +
		'onclick="startrec=' + (start + 20) +
		';check_search(); return false;">Next</a>';

	if (start > 0)
	    body.innerHTML += ' <a href="" ' +
		'onclick="startrec=' + (start - 20) +
		';check_search(); return false;">Previous</a>';

	body.innerHTML += '<br/>';
	for (i = 0; i < hits.length; i++)
	{
	    body.innerHTML += '<p>';
	    body.innerHTML += (i + start + 1) + ': ';
	    var mk = hits[i].getElementsByTagName("title");
	    if (mk[0])
		body.innerHTML += mk[0].childNodes[0].nodeValue;
	    body.innerHTML += '</p>';
	}
	shown++;
	if (shown < 5)
	    searchtimer = setTimeout(check_search, 1000);
	else
	    searchtimer = setTimeout(check_search, 2000);
    }
    if (!termtimer)
	termtimer = setTimeout(check_termlist, 1000);
}

function check_search()
{
    clearTimeout(searchtimer);
    var url = "search.pz2?" +
        "command=show" +
	"&start=" + startrec +
	"&num=15" +
	"&session=" + session +
	"&block=1";
    xshow = GetXmlHttpObject();
    xshow.onreadystatechange=show_records;
    xshow.open("GET", url);
    xshow.send(null);
}


function refine_query (obj) {
    var query_cell = document.getElementById('query');
    var subject = obj.innerHTML;
    
    subject = subject.replace(/[\(\)]/g, '');
    query_cell.value += ' and su=(' + subject + ')';
    start_search();
}

function show_termlist()
{
    if (xtermlist.readyState != 4)
	return;

    var i;
    var xml = xtermlist.responseXML;
    var body = document.getElementById("termlist");
    var hits = xml.getElementsByTagName("term");
    if (!hits[0])
    {
	termtimer = setTimeout(check_termlist, 1000);
        
    }
    else
    {
	body.innerHTML = "<b>Limit results:</b><br>";
	for (i = 0; i < hits.length; i++)
	{
	    var namen = hits[i].getElementsByTagName("name");
	    if (namen[0])
		body.innerHTML += '<a href="#" onclick="refine_query(this)">' +
                                  namen[0].childNodes[0].nodeValue +
                                  '</a>';
	    body.innerHTML += '<br>';
	}
	termtimer = setTimeout(check_termlist, 2000);
    }
}


function check_termlist()
{
    var url = "search.pz2?" +
        "command=termlist" +
	"&session=" + session;
    xtermlist = GetXmlHttpObject();
    xtermlist.onreadystatechange=show_termlist;
    xtermlist.open("GET", url);
    xtermlist.send(null);
}

function show_stat()
{
    if (xstat.readyState != 4)
	return;
    var i;
    var xml = xstat.responseXML;
    var body = document.getElementById("stat");
    var nodes = xml.childNodes[0].childNodes;
    if (!nodes[0])
    {
	stattimer  = setTimeout(check_stat, 500);
    }
    else
    {
	body.innerHTML = "(";
	for (i = 0; i < nodes.length; i++)
	{
	    if (nodes[i].nodeType != 1)
		continue;
	    var value = nodes[i].childNodes[0].nodeValue;
	    if (value == 0)
		continue;
	    var name = nodes[i].nodeName;
	    body.innerHTML += ' ' + name + '=' + value;
	}
	body.innerHTML += ')';
	stattimer = setTimeout(check_stat, 2000);
    }
}

function check_stat()
{
    var url = "search.pz2?" +
        "command=stat" +
	"&session=" + session;
    xstat = GetXmlHttpObject();
    xstat.onreadystatechange=show_stat;
    xstat.open("GET", url);
    xstat.send(null);
}

function search_started()
{
    if (xsearch.readyState != 4)
	return;
    var xml = xsearch.responseXML;
    var error = xml.getElementsByTagName("error");
    if (error[0])
    {
	var msg = error[0].childNodes[0].nodeValue;
	alert(msg);
	return;
    }
    check_search();
    stattimer = setTimeout(check_stat, 1000);
}

function start_search()
{
    clearTimeout(termtimer);
    termtimer = 0;
    clearTimeout(searchtimer);
    searchtimer = 0;
    clearTimeout(stattimer);
    stattimer = 0;
    clearTimeout(showtimer);
    showtimer = 0;
    if (!targets_loaded)
    {
	alert("Please load targets first");
	return;
    }
    var query = escape(document.getElementById('query').value);
    var url = "search.pz2?" +
        "command=search" +
	"&session=" + session +
	"&query=" + query;
    xsearch = GetXmlHttpObject();
    xsearch.onreadystatechange=search_started;
    xsearch.open("GET", url);
    xsearch.send(null);
    document.getElementById("termlist").innerHTML = '';
    document.getElementById("body").innerHTML = '';
    update_history();
    shown = 0;
    startrec = 0;
}


function session_encode ()
{
    var i;
    var session = '';

    for (i = 0; i < session_cells.length; i++)
    {
        var name = session_cells[i];
        var value = escape(document.getElementById(name).value);
        session += '&' + name + '=' + value;
    }

    return session;
}


function session_restore (session)
{
    var fields = session.split(/&/);
    var i;

    for (i = 1; i < fields.length; i++)
    {
        var pair = fields[i].split(/=/);
        var key = pair.shift();
        var value = pair.join('=');
        var cell = document.getElementById(key);

        cell.value = value;
    }
    
}


function session_read ()
{
    var ses = window.location.hash.replace(/^#/, '');
    return ses;
}


function session_store (new_value)
{
    window.location.hash = '#' + new_value;
}


function update_history ()
{
    var session = session_encode();
    session_store(session);
    old_session = session;
}


function session_check ()
{
    var session = session_read();

    clearInterval(url_surveillence);

    if ( session != unescape(old_session) )
    {
        session_restore(session);
        start_search();
        
    }
    
    url_surveillence = setInterval(session_check, 200);
}
