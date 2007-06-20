/* $Id: search.js,v 1.14 2007-06-20 19:27:18 adam Exp $
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
var session_cells = Array('query', 'startrec', 'action_type');
var old_session = session_read();
var url_surveillence;
var recstoshow = 15;
var cur_termlist = "subject";

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

function SendXmlHttpObject(obj, url, handler)
{
    obj.onreadystatechange=handler;
    obj.open("GET", url);
    obj.send(null);
}

function session_started()
{
    if (xinitSession.readyState != 4)
	return;
    var xml = xinitSession.responseXML;
    var sesid = xml.getElementsByTagName("session")[0].childNodes[0].nodeValue;
    document.getElementById("status").innerHTML = "Live";
    session = sesid;
    setTimeout(ping_session, 50000);
}

function start_session()
{
    xinitSession = GetXmlHttpObject();
    var url="/pazpar2/search.pz2?";
    url += "command=init";
    xinitSession.onreadystatechange=session_started;
    xinitSession.open("GET", url);
    xinitSession.send(null);
    
    //url_surveillence = setInterval(session_check, 200);
}

function ping_session()
{
    if (!session)
	return;
    var url = "/pazpar2/search.pz2?command=ping&session=" + session;
    SendXmlHttpObject(xpingSession = GetXmlHttpObject(), url, session_pinged);
}

function session_pinged()
{
    if (xpingSession.readyState != 4)
	return;
    var xml = xpingSession.responseXML;
    var error = xml.getElementsByTagName("error");
    if (error[0])
    {
	var msg = error[0].childNodes[0].nodeValue;
	alert(msg);
	location = "?";
	return;
    }
    setTimeout(ping_session, 50000);
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
    var url="/pazpar2/search.pz2?" +
    	"command=load" +
	"&session=" + session +
	"&name=" + fn;
    document.getElementById("targetstatus").innerHTML = "Loading targets...";
    xloadTargets = GetXmlHttpObject();
    xloadTargets.onreadystatechange=targets_loaded;
    xloadTargets.open("GET", url);
    xloadTargets.send(null);
}


function update_action (new_action) {
    document.search.action_type.value = new_action;
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
	searchtimer = setTimeout(check_search, 2000);
    }
    else
    {

	var total = Number(xml.getElementsByTagName('total')[0].childNodes[0].nodeValue);
	var merged = Number(xml.getElementsByTagName('merged')[0].childNodes[0].nodeValue);
	var start = Number(xml.getElementsByTagName('start')[0].childNodes[0].nodeValue);
	var num = Number(xml.getElementsByTagName('num')[0].childNodes[0].nodeValue);
	var clients = Number(xml.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue);
	body.innerHTML = '<b>Records : ';
	body.innerHTML += (start + 1) + ' to ' + (start + num) +
		' of ' + merged + ' (total hits: ' + total + ')</b>';

	if (start + num < merged)
	    body.innerHTML += ' <a href="" ' +
		'onclick="document.search.startrec.value=' + (start + recstoshow) +
                ";update_action('page')" +
		';check_search(); update_history(); return false;">Next</a>';

	if (start > 0)
	    body.innerHTML += ' <a href="" ' +
		'onclick="document.search.startrec.value=' + (start - recstoshow) +
                ";update_action('page')" +
		';check_search(); update_history();return false;">Previous</a>';

	body.innerHTML += '<br/>';
	for (i = 0; i < hits.length; i++)
	{
	    body.innerHTML += '<p>';
	    body.innerHTML += (i + start + 1) + ': ';
	    var mk = hits[i].getElementsByTagName("md-title");
	    if (mk[0])
		body.innerHTML += mk[0].childNodes[0].nodeValue;
	    body.innerHTML += '</p>';
	}
	if (shown >= 0) {
	    shown++;	
	    if (shown < 5)
		searchtimer = setTimeout(check_search, 1000);
	    else
		searchtimer = setTimeout(check_search, 2000);
	    if (clients == 0)
		shown = -1;
	}
    }
    if (!termtimer)
	termtimer = setTimeout(check_termlist, 2000);
}

function check_search()
{
    clearTimeout(searchtimer);
    var url = "/pazpar2/search.pz2?" +
        "command=show" +
	"&start=" + document.search.startrec.value +
	"&num=" + recstoshow +
	"&session=" + session +
	"&sort=relevance";
    xshow = GetXmlHttpObject();
    xshow.onreadystatechange=show_records;
    xshow.open("GET", url);
    xshow.send(null);
}


function refine_query (obj) {
    var query_cell = document.getElementById('query');
    var term = obj.firstChild.nodeValue;
    
    term = term.replace(/[\(\)]/g, '');
    if (cur_termlist == 'subject')
	query_cell.value += ' and su=(' + term + ')';
    else if (cur_termlist == 'author')
	query_cell.value += ' and au=(' + term + ')';
    start_search();
}

function set_termlist(termlist)
{
    cur_termlist = termlist;
    check_termlist();
    if (termtimer)
    {
	clearTimeout(termtimer);
	termtimer = 0;
    }
}

function show_termlistoptions(body)
{
    var opts = Array(
        Array('subject', 'Subject'),
	Array('author', 'Author')
    );

    for (i in opts)
    {
	if (opts[i][0] == cur_termlist)
	    body.innerHTML += opts[i][1];
	else
	    body.innerHTML += '<a href="" onclick="set_termlist(\'' + opts[i][0] +
		'\'); return false">' + opts[i][1] + '</a>';
	body.innerHTML += ' ';
    }
    body.innerHTML += '<p>';
}

function show_termlist()
{
    if (xtermlist.readyState != 4)
	return;

    var i;
    var xml = xtermlist.responseXML;
    var body = document.getElementById("termlist");
    var hits = xml.getElementsByTagName("term");
    var clients =
	Number(xml.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue);
    if (!hits[0])
    {
	termtimer = setTimeout(check_termlist, 2000);
    }
    else
    {
	body.innerHTML = "<b>Limit results:</b><br>";
	show_termlistoptions(body);
	for (i = 0; i < hits.length; i++)
	{
	    var namen = hits[i].getElementsByTagName("name");
	    if (namen[0])
		body.innerHTML += '<a href="#" onclick="refine_query(this)">' +
                                  namen[0].childNodes[0].nodeValue +
                                  '</a>';
	    body.innerHTML += '<br>';
	}
	if (clients > 0)
	    termtimer = setTimeout(check_termlist, 2000);
    }
}

function check_termlist()
{
    var url = "/pazpar2/search.pz2?" +
        "command=termlist" +
	"&session=" + session +
	"&num=20" +
	"&name=" + cur_termlist;
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
    var clients =
	Number(xml.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue);
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
	if (clients > 0)
	    stattimer = setTimeout(check_stat, 2000);
    }
}

function check_stat()
{
    var url = "/pazpar2/search.pz2?" +
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
    shown = 0;
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
    var url = "/pazpar2/search.pz2?" +
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
    document.search.startrec.value = 0;
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
    var action = document.search.action_type.value;

    clearInterval(url_surveillence);

    if ( session != unescape(old_session) )
    {
        session_restore(session);

        if (action == 'search') {
            start_search();
        } else if (action == 'page') {
            check_search();
        } else {
            alert('Unregocnized action_type: ' + action);
            return;
        }
    }
    
    url_surveillence = setInterval(session_check, 200);
}
