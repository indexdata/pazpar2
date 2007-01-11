/* $Id: search.js,v 1.20 2007-01-11 12:47:19 sondberg Exp $
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
var page_window = 5;  // Number of pages prior to and after the current page
var facet_list;
var cur_facet = 0;

function initialize ()
{
    facet_list = get_available_facets();
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
    assign_text(document.getElementById("status"), 'Live');
    session = sesid;
    setTimeout(ping_session, 50000);
}

function start_session()
{
    xinitSession = GetXmlHttpObject();
    var url="search.pz2?";
    url += "command=init";
    xinitSession.onreadystatechange=session_started;
    xinitSession.open("GET", url);
    xinitSession.send(null);
}

function ping_session()
{
    if (!session)
	return;
    var url = "search.pz2?command=ping&session=" + session;
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

    assign_text(document.getElementById("targetstatus"), 'Targets loaded');
}

function load_targets()
{
    var fn = document.getElementById("targetfilename").value;
    clearTimeout(termtimer);
    clearTimeout(searchtimer);
    clearTimeout(stattimer);
    clearTimeout(showtimer);
    clear_cell(document.getElementById("stat"));
    if (!fn)
    {
	alert("Please enter a target definition file name");
	return;
    }
    var url="search.pz2?" +
    	"command=load" +
	"&session=" + session +
	"&name=" + fn;
    assign_text(document.getElementById("targetstatus"), 'Loading targets...');
    xloadTargets = GetXmlHttpObject();
    xloadTargets.onreadystatechange=targets_loaded;
    xloadTargets.open("GET", url);
    xloadTargets.send(null);
}


function update_action (new_action) {
    document.search.action_type.value = new_action;
}


function make_pager (hits, offset, max) {
    var html = '';
    var off;
    var start_offset = offset - page_window * max;
    var div_elem = document.createElement('div');
    
    div_elem.className = 'pages';

    if (start_offset < 0) {
        start_offset = 0;
    }

    for (off = start_offset;
         off < hits && off < (start_offset + 2 * page_window * max); 
         off += max) {
        
        var p = off / max + 1;
        var page_elem = create_element('a', p);
        var newline_node = document.createTextNode(' ');

        if ((offset >= off) && (offset < (off + max))) {
            page_elem.className = 'select';
        }

        page_elem.setAttribute('href', '#');
        page_elem.setAttribute('off', off);
        page_elem.onclick = function () {
            update_offset(this.getAttribute('off'));
        };

        div_elem.appendChild(page_elem);
        div_elem.appendChild(newline_node);
    }

    return div_elem;
}


function update_offset (offset) {
    clearTimeout(searchtimer);
    document.search.startrec.value = offset;
    update_action('page');
    check_search();
    update_history();
    return false;
}


function create_element (name, cdata) {
    var elem_node = document.createElement(name);
    var text_node = document.createTextNode(cdata);
    elem_node.appendChild(text_node);

    return elem_node;
}


function clear_cell (cell) {
    while (cell.hasChildNodes())
        cell.removeChild(cell.firstChild);
}


function append_text(cell, text) {
    text_node = document.createTextNode(text);
    cell.appendChild(text_node);
}


function assign_text (cell, text) {
    clear_cell(cell);
    append_text(cell, text);
}


function show_records()
{
    if (xshow.readyState != 4)
	return;
    var i;
    var xml = xshow.responseXML;
    var body = document.getElementById("body");
    var hits = xml.getElementsByTagName("hit");

    clear_cell(body);

    if (!hits[0]) // We should never get here with blocking operations
    {
	assign_text(body, 'No records yet');
	searchtimer = setTimeout(check_search, 250);
    }
    else
    {

	var total = Number(xml.getElementsByTagName('total')[0].childNodes[0].nodeValue);
	var merged = Number(xml.getElementsByTagName('merged')[0].childNodes[0].nodeValue);
	var start = Number(xml.getElementsByTagName('start')[0].childNodes[0].nodeValue);
	var num = Number(xml.getElementsByTagName('num')[0].childNodes[0].nodeValue);
	var clients = Number(xml.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue);
        var pager = make_pager(merged, start,recstoshow);
        var break_node1 = document.createElement('br');
        var break_node2 = document.createElement('br');
        var record_container = document.createElement('div');
        var interval = create_element('div', 'Records : ' + (start + 1) +
                                             ' to ' + (start + num) + ' of ' +
                                             merged + ' (total hits: ' +
                                             total + ')');
        interval.className = 'results';
        record_container.className = 'records';

        body.appendChild(pager);
        body.appendChild(interval);
        body.appendChild(break_node1);
        body.appendChild(break_node2);
        body.appendChild(record_container);

	for (i = 0; i < hits.length; i++)
	{
	    var mk = hits[i].getElementsByTagName("md-title");
            var field = '';

	    if (mk[0]) {
                field = mk[0].childNodes[0].nodeValue;
            } else {
                field = 'N/A';
            }
            
            var record_cell = create_element('a', field);
            record_cell.setAttribute('href', '#');
            record_cell.className = 'record';
            record_container.appendChild(record_cell);
	}

	shown++;
	if (clients > 0)
	{
	    if (shown < 5)
		searchtimer = setTimeout(check_search, 1000);
	    else
		searchtimer = setTimeout(check_search, 2000);
	}
    }
    if (!termtimer)
	termtimer = setTimeout(check_termlist, 500);
}

function check_search()
{
    clearTimeout(searchtimer);
    var url = "search.pz2?" +
        "command=show" +
	"&start=" + document.search.startrec.value +
	"&num=" + recstoshow +
	"&session=" + session +
	"&block=1";
    xshow = GetXmlHttpObject();
    xshow.onreadystatechange=show_records;
    xshow.open("GET", url);
    xshow.send(null);
}


function refine_query (obj) {
    var term = obj.getAttribute('term');
    var cur_termlist = obj.getAttribute('facet');
    var query_cell = document.getElementById('query');
    
    term = term.replace(/[\(\)]/g, '');
    
    if (cur_termlist == 'subject')
	query_cell.value += ' and su=(' + term + ')';
    else if (cur_termlist == 'author')
	query_cell.value += ' and au=(' + term + ')';

    start_search();
}



function show_termlist()
{
    if (xtermlist.readyState != 4)
	return;

    var i;
    var xml = xtermlist.responseXML;
    var body = facet_list[cur_facet][1];
    var facet_name = facet_list[cur_facet][0];
    var hits = xml.getElementsByTagName("term");
    var clients =
	Number(xml.getElementsByTagName("activeclients")[0].childNodes[0].nodeValue);

    cur_facet++;

    if (cur_facet >= facet_list.length)
        cur_facet = 0;

    if (!hits[0])
    {
	termtimer = setTimeout(check_termlist, 500);
    }
    else
    {
	clear_cell(body);
	
        for (i = 0; i < hits.length; i++)
	{
	    var namen = hits[i].getElementsByTagName("name");
	    if (namen[0])
                var term = namen[0].childNodes[0].nodeValue;
                var refine_cell = create_element('a', term);
                refine_cell.setAttribute('href', '#');
                refine_cell.setAttribute('term', term);
                refine_cell.setAttribute('facet', facet_name);
                refine_cell.onclick = function () {
                    refine_query(this);
                    return false;
                };
                body.appendChild(refine_cell);
	}

	if (clients > 0)
	    termtimer = setTimeout(check_termlist, 1000);
    }
}

function check_termlist()
{
    var facet_name = facet_list[cur_facet][0];
    var url = "search.pz2?" +
        "command=termlist" +
	"&session=" + session +
	"&name=" + facet_name;
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
	assign_text(body, '(');
	for (i = 0; i < nodes.length; i++)
	{
	    if (nodes[i].nodeType != 1)
		continue;
	    var value = nodes[i].childNodes[0].nodeValue;
	    if (value == 0)
		continue;
	    var name = nodes[i].nodeName;
	    append_text(body, ' ' + name + '=' + value);
	}

        append_text(body, ')');
	if (clients > 0)
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
    clear_cell(document.getElementById("body"));
    update_history();
    shown = 0;
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


function get_available_facets () {
    var facet_container = document.getElementById('termlists');
    var facet_cells = facet_container.childNodes;
    var facets = Array();
    var i;

    for (i = 0; i < facet_cells.length; i++) {
        var cell = facet_cells.item(i);

        if (cell.className == 'facet') {
            var facet_name = cell.id.replace(/^facet_([^_]+)_terms$/, "$1");
            facets.push(Array(facet_name, cell));
        }
    }

    return facets;
}


function get_facet_container (obj) {
    return document.getElementById(obj.id + '_terms');
}


function toggle_facet (obj) {
    var container = get_facet_container(obj);

    if (obj.className == 'selected') {
        obj.className = 'unselected';
        container.style.display = 'inline';
    } else {
        obj.className = 'selected';
        container.style.display = 'none';
    }
}
