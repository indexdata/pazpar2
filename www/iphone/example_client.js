/* A very simple client that shows a basic usage of the pz2.js
*/

// create a parameters array and pass it to the pz2's constructor
// then register the form submit event with the pz2.search function
// autoInit is set to true on default
var usesessions = true;
var pazpar2path = '/pazpar2/search.pz2';
var showResponseType = '';
var querys = {'su': '', 'au': '', 'xt': ''};

if (document.location.hash == '#useproxy') {
    usesessions = false;
    pazpar2path = '/service-proxy/';
    showResponseType = 'json';
}

my_paz = new pz2( { "onshow": my_onshow,
                    "showtime": 500,            //each timer (show, stat, term, bytarget) can be specified this way
                    "pazpar2path": pazpar2path,
                    "oninit": my_oninit,
                    "onstat": my_onstat,
                    "onterm": my_onterm_iphone,
                    "termlist": "xtargets,subject,author",
                    "onbytarget": my_onbytarget,
	 	            "usesessions" : usesessions,
                    "showResponseType": showResponseType,
                    "onrecord": my_onrecord } );
// some state vars
var curPage = 1;
var recPerPage = 20;
var totalRec = 0;
var curDetRecId = '';
var curDetRecData = null;
var curSort = 'relevance';
var curFilter = null;
var submitted = false;
var SourceMax = 16;
var SubjectMax = 10;
var AuthorMax = 10;
var tab = "recordview"; 


//
// pz2.js event handlers:
//
function my_oninit() {
    my_paz.stat();
    my_paz.bytarget();
}

function my_onshow(data) {
    totalRec = data.merged;
    // move it out
    var pager = document.getElementById("pager");
    pager.innerHTML = "";
    pager.innerHTML +='<hr/><div style="float: right">Displaying: ' 
                    + (data.start + 1) + ' to ' + (data.start + data.num) +
                     ' of ' + data.merged + ' (found: ' 
                     + data.total + ')</div>';
    drawPager(pager);
    // navi
    var results = document.getElementById("results");
  
    var html = [];
    for (var i = 0; i < data.hits.length; i++) {
        var hit = data.hits[i];
	      html.push('<li id="recdiv_'+hit.recid+'" >'
           /* +'<span>'+ (i + 1 + recPerPage * (curPage - 1)) +'. </span>' */
            +'<a href="#" id="rec_'+hit.recid
            +'" onclick="showDetails(this.id);return false;">' 
            + hit["md-title"] +'</a> '); 
	      if (hit["md-title-responsibility"] !== undefined) {
    	    html.push('<a href="#">'+hit["md-title-responsibility"]+'</a> ');
  	      if (hit["md-title-remainder"] !== undefined) {
  	        html.push('<a href="#">' + hit["md-title-remainder"] + ' </a> ');
  	      }
      	}
        if (hit.recid == curDetRecId) {
            html.push(renderDetails(curDetRecData));
        }
      	html.push('</div>');
    }
    replaceHtml(results, html.join(''));
}

function my_onstat(data) {
    var stat = document.getElementById("stat");
    if (stat == null)
	return;
    
    stat.innerHTML = '<b> .:STATUS INFO</b> -- Active clients: '
                        + data.activeclients
                        + '/' + data.clients + ' -- </span>'
                        + '<span>Retrieved records: ' + data.records
                        + '/' + data.hits + ' :.</span>';
}

function showhide(newtab) {
	var showtermlist = false;
	if (newtab != null)
		tab = newtab;

	if (tab == "recordview") {
		document.getElementById("recordview").style.display = '';
	}
	else 
		document.getElementById("recordview").style.display = 'none';

	if (tab == "xtargets") {
		document.getElementById("term_xtargets").style.display = '';
		showtermlist = true;
	}
	else
		document.getElementById("term_xtargets").style.display = 'none';

	if (tab == "subjects") {
		document.getElementById("term_subjects").style.display = '';
		showtermlist = true;
	}
	else
		document.getElementById("term_subjects").style.display = 'none';

	if (tab == "authors") {
		document.getElementById("term_authors").style.display = '';
		showtermlist = true;
	}
	else
		document.getElementById("term_authors").style.display = 'none';

	if (showtermlist == false) 
		document.getElementById("termlist").style.display = 'none';
	else 
		document.getElementById("termlist").style.display = '';
}

function my_onterm(data) {
    var termlists = [];
    
    termlists.push('<div id="term_xtargets" >');
    termlists.push('<h4 class="termtitle">Sources</h4>');
    termlists.push('<ul>');
    termlists.push('<li><a href="#" target_id="reset_xt" onclick="limitOrResetTarget(\'reset_xt\',\'All\');return false;">All</a></li>');
    for (var i = 0; i < data.xtargets.length && i < SourceMax; i++ ) {
        termlists.push('<li><a href="#" target_id='+data.xtargets[i].id
            + ' onclick="limitOrResetTarget(this.getAttribute(\'target_id\'), \'' + data.xtargets[i].name + '\');return false;">' 
	    + data.xtargets[i].name + ' (' + data.xtargets[i].freq + ')</a></li>');
    }
    termlists.push('</ul>');
    termlists.push('</div>');
     
    termlists.push('<div id="term_subjects" >');
    termlists.push('<h4>Subjects</h4>');
    termlists.push('<ul>');
    termlists.push('<li><a href="#" target_id="reset_su" onclick="limitOrResetQuery(\'reset_su\',\'All\');return false;">All</a></li>');
    for (var i = 0; i < data.subject.length && i < SubjectMax; i++ ) {
        termlists.push('<li><a href="#" onclick="limitOrResetQuery(\'su\', \'' + data.subject[i].name + '\');return false;">' 
		       + data.subject[i].name + ' (' + data.subject[i].freq + ')</a></li>');
    }
    termlists.push('</ul>');
    termlists.push('</div>');
            
    termlists.push('<div id="term_authors" >');
    termlists.push('<h4 class="termtitle">Authors</h4>');
    termlists.push('<ul>');
    termlists.push('<li><a href="#" onclick="limitOrResetQuery(\'reset_au\',\'All\');return false;">All<a></li>');
    for (var i = 0; i < data.author.length && i < AuthorMax; i++ ) {
        termlists.push('<li><a href="#" onclick="limitQuery(\'au\', \'' + data.author[i].name +'\');return false;">' 
                            + data.author[i].name 
                            + '  (' 
                            + data.author[i].freq 
                            + ')</a></li>');
    }
    termlists.push('</ul>');
    termlists.push('</div>');
    var termlist = document.getElementById("termlist");
    replaceHtml(termlist, termlists.join(''));
    showhide();
}

function serialize(array) {
	var t = typeof (obj);
	if (t != "object" || obj === null) {
		// simple data type
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
		return (arr ? "" : "") + String(json) + (arr ? "]" : "}");
	}
}

var termlist = {};
function my_onterm_iphone(data) {
    my_onterm(data);
    var targets = "reset_xt|All\n";
    
    for (var i = 0; i < data.xtargets.length; i++ ) {
    	
        targets = targets + data.xtargets[i].id + "|" + data.xtargets[i].name + "|" + data.xtargets[i].freq + "\n";
    }
    termlist["xtargets"] = targets;
    var subjects = "reset_su|All\n";
    for (var i = 0; i < data.subject.length; i++ ) {
        subjects = subjects + "su" + "|" + data.subject[i].name + "|" + data.subject[i].freq + "\n";
    }
    termlist["subjects"] = subjects;
    var authors = "reset_au|All\n";
    for (var i = 0; i < data.author.length; i++ ) {
        authors = authors + "au" + "|" + data.author[i].name + "|" + data.author[i].freq + "\n";
    }
    termlist["authors"] = authors;
    //document.getElementById("log").innerHTML = targets + "\n" + subjects + "\n" + authors;
    callback.send("termlist", "refresh");
}

function getTargets() {
	return termlist['xtargets'];
}

function getSubjects() {
	return termlist['subjects'];
}

function getAuthors() {
	return termlist['authors'];
}

function my_onrecord(data) {
    // FIXME: record is async!!
    clearTimeout(my_paz.recordTimer);
    // in case on_show was faster to redraw element
    var detRecordDiv = document.getElementById('det_'+data.recid);
    if (detRecordDiv) return;
    curDetRecData = data;
    var recordDiv = document.getElementById('recdiv_'+curDetRecData.recid);
    var html = renderDetails(curDetRecData);
    recordDiv.innerHTML += html;
}

function my_onrecord_iphone(data) {
    my_onrecord(data);
    callback.send("record", data.recid, data, data.xtargets[i].freq);
}


function my_onbytarget(data) {
    var targetDiv = document.getElementById("bytarget");
    var table ='<table><thead><tr><td>Target ID</td><td>Hits</td><td>Diags</td>'
        +'<td>Records</td><td>State</td></tr></thead><tbody>';
    
    for (var i = 0; i < data.length; i++ ) {
        table += "<tr><td>" + data[i].id +
            "</td><td>" + data[i].hits +
            "</td><td>" + data[i].diagnostic +
            "</td><td>" + data[i].records +
            "</td><td>" + data[i].state + "</td></tr>";
    }

    table += '</tbody></table>';
    targetDiv.innerHTML = table;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// wait until the DOM is ready
function domReady () 
{ 
    document.search.onsubmit = onFormSubmitEventHandler;
    document.search.query.value = '';
    document.select.sort.onchange = onSelectDdChange;
    document.select.perpage.onchange = onSelectDdChange;
    if (document.location.search.match("inApp=true")) 
    	applicationMode(true);
}
 
function applicationMode(newmode) 
{
	var searchdiv = document.getElementById("searchForm");
	var navi = document.getElementById("navi");
	if (newmode)
		inApp = newmode;
	if (inApp) {
    	document.getElementById("heading").style.display="none";
       	searchdiv.style.display = 'none';
       	navi.style.display = 'none';
	}
	else { 
		searchdiv.style.display = '';
		document.search.onsubmit = onFormSubmit;
	}
	callback.init();
}
// when search button pressed
function onFormSubmitEventHandler() 
{
    resetPage();
    loadSelect();
    triggerSearch();
    submitted = true;
    return false;
}

function onSelectDdChange()
{
    if (!submitted) return false;
    resetPage();
    loadSelect();
    my_paz.show(0, recPerPage, curSort);
    return false;
}

function resetPage()
{
    curPage = 1;
    totalRec = 0;
}

function triggerSearch ()
{
    my_paz.search(document.search.query.value, recPerPage, curSort, curFilter);
}

function loadSelect ()
{
    curSort = document.select.sort.value;
    recPerPage = document.select.perpage.value;
}

// limit the query after clicking the facet
function limitQuery(field, value)
{
	var newQuery = ' and ' + field + '="' + value + '"';
	querys[field] += newQuery;
    document.search.query.value += newQuery;
    onFormSubmitEventHandler();
    showhide("recordview");
}

//limit the query after clicking the facet
function removeQuery (field, value) {
	document.search.query.value.replace(' and ' + field + '="' + value + '"', '');
    onFormSubmitEventHandler();
    showhide("recordview");
}

//limit the query after clicking the facet
function limitOrResetQuery (field, value, selected) {
	if (field == 'reset_su' || field == 'reset_au') {
		var reset_field = field.substring(6);
		document.search.query.value = document.search.query.value.replace(querys[reset_field], '');
		querys[reset_field] = '';
	    onFormSubmitEventHandler();
	    showhide("recordview");
	}
	else 
		limitQuery(field, value);
	//alert("limitOrResetQuerry: query after: " + document.search.query.value);
}

// limit by target functions
function limitTarget (id, name)
{
    var navi = document.getElementById('navi');
    navi.innerHTML = 
        'Source: <a class="crossout" href="#" onclick="delimitTarget();return false;">'
        + name + '</a>';
    navi.innerHTML += '<hr/>';
    curFilter = 'pz:id=' + id;
    resetPage();
    loadSelect();
    triggerSearch();
    showhide("recordview");
    return false;
}

function delimitTarget ()
{
    var navi = document.getElementById('navi');
    navi.innerHTML = '';
    curFilter = null; 
    resetPage();
    loadSelect();
    triggerSearch();
    return false;
}

function limitOrResetTarget(id, name) {
	if (id == 'reset_xt') {
		delimitTarget();
	}
	else {
		limitTarget(id,name);
	}
}

function drawPager (pagerDiv)
{
    //client indexes pages from 1 but pz2 from 0
    var onsides = 6;
    var pages = Math.ceil(totalRec / recPerPage);
    
    var firstClkbl = ( curPage - onsides > 0 ) 
        ? curPage - onsides
        : 1;

    var lastClkbl = firstClkbl + 2*onsides < pages
        ? firstClkbl + 2*onsides
        : pages;

    var prev = '<span id="prev">&#60;&#60; Prev</span><b> | </b>';
    if (curPage > 1)
        var prev = '<a href="#" id="prev" onclick="pagerPrev();">'
        +'&#60;&#60; Prev</a><b> | </b>';

    var middle = '';
    for(var i = firstClkbl; i <= lastClkbl; i++) {
        var numLabel = i;
        if(i == curPage)
            numLabel = '<b>' + i + '</b>';

        middle += '<a href="#" onclick="showPage(' + i + ')"> '
            + numLabel + ' </a>';
    }
    
    var next = '<b> | </b><span id="next">Next &#62;&#62;</span>';
    if (pages - curPage > 0)
    var next = '<b> | </b><a href="#" id="next" onclick="pagerNext()">'
        +'Next &#62;&#62;</a>';

    predots = '';
    if (firstClkbl > 1)
        predots = '...';

    postdots = '';
    if (lastClkbl < pages)
        postdots = '...';

    pagerDiv.innerHTML += '<div style="float: clear">' 
        + prev + predots + middle + postdots + next + '</div><hr/>';
}

function showPage (pageNum)
{
    curPage = pageNum;
    my_paz.showPage( curPage - 1 );
}

// simple paging functions

function pagerNext() {
    if ( totalRec - recPerPage*curPage > 0) {
        my_paz.showNext();
        curPage++;
    }
}

function pagerPrev() {
    if ( my_paz.showPrev() != false )
        curPage--;
}

// swithing view between targets and records

function switchView(view) {
    
    var targets = document.getElementById('targetview');
    var records = document.getElementById('recordview');
    
    switch(view) {
        case 'targetview':
            targets.style.display = "block";            
            records.style.display = "none";
            break;
        case 'recordview':
            targets.style.display = "none";            
            records.style.display = "block";
            break;
        default:
            alert('Unknown view.');
    }
}

// detailed record drawing
function showDetails (prefixRecId) {
    var recId = prefixRecId.replace('rec_', '');
    var oldRecId = curDetRecId;
    curDetRecId = recId;
    
    // remove current detailed view if any
    var detRecordDiv = document.getElementById('det_'+oldRecId);
    // lovin DOM!
    if (detRecordDiv)
      detRecordDiv.parentNode.removeChild(detRecordDiv);

    // if the same clicked, just hide
    if (recId == oldRecId) {
        curDetRecId = '';
        curDetRecData = null;
        return;
    }
    // request the record
    my_paz.record(recId);
}

function replaceHtml(el, html) {
  var oldEl = typeof el === "string" ? document.getElementById(el) : el;
  /*@cc_on // Pure innerHTML is slightly faster in IE
    oldEl.innerHTML = html;
    return oldEl;
    @*/
  var newEl = oldEl.cloneNode(false);
  newEl.innerHTML = html;
  oldEl.parentNode.replaceChild(newEl, oldEl);
  /* Since we just removed the old element from the DOM, return a reference
     to the new element, which can be used to restore variable references. */
  return newEl;
};

function renderDetails(data, marker)
{
    var details = '<div class="details" id="det_'+data.recid+'"><table>';
    if (marker) details += '<tr><td>'+ marker + '</td></tr>';
    if (data["md-title"] != undefined) {
        details += '<tr><td><b>Title</b></td><td><b>:</b> '+data["md-title"];
  	if (data["md-title-remainder"] !== undefined) {
	      details += ' : <span>' + data["md-title-remainder"] + ' </span>';
  	}
  	if (data["md-title-responsibility"] !== undefined) {
	      details += ' <span><i>'+ data["md-title-responsibility"] +'</i></span>';
  	}
 	  details += '</td></tr>';
    }
    if (data["md-date"] != undefined)
        details += '<tr><td><b>Date</b></td><td><b>:</b> ' + data["md-date"] + '</td></tr>';
    if (data["md-author"] != undefined)
        details += '<tr><td><b>Author</b></td><td><b>:</b> ' + data["md-author"] + '</td></tr>';
    if (data["md-electronic-url"] != undefined)
        details += '<tr><td><b>URL</b></td><td><b>:</b> <a href="' + data["md-electronic-url"] + '" target="_blank">' + data["md-electronic-url"] + '</a>' + '</td></tr>';
    if (data["location"][0]["md-subject"] != undefined)
        details += '<tr><td><b>Subject</b></td><td><b>:</b> ' + data["location"][0]["md-subject"] + '</td></tr>';
    if (data["location"][0]["@name"] != undefined)
        details += '<tr><td><b>Location</b></td><td><b>:</b> ' + data["location"][0]["@name"] + " (" +data["location"][0]["@id"] + ")" + '</td></tr>';
    details += '</table></div>';
    return details;
}
 //EOF
