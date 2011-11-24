/* A very simple client that shows a basic usage of the pz2.js
*/

// create a parameters array and pass it to the pz2's constructor
// then register the form submit event with the pz2.search function
// autoInit is set to true on default
var usesessions = false;
var pazpar2path = '/service-proxy/';
var showResponseType = '';
// Facet configuration
var querys = {'su': '', 'au': '', 'xt': ''};
var query_client_server = {'su': 'subject', 'au': 'author', 'xt': 'xtargets'};
var querys_server = {};
var useLimit = 1;
// Fail to get JSON working stabil.
var showResponseType = 'xml';

var imageHelper = new ImageHelper();

if (document.location.hash == '#pazpar2' || document.location.search.match("useproxy=false")) {
    usesessions = true;
    pazpar2path = '/pazpar2/search.pz2';
    showResponseType = 'xml';
}

my_paz = new pz2( { "onshow": my_onshow,
//                    "showtime": 2000,            //each timer (show, stat, term, bytarget) can be specified this way
                    "pazpar2path": pazpar2path,
                    "oninit": my_oninit,
                    "onstat": null,
                    "onterm": my_onterm_iphone,
                    "termlist": "xtargets,subject,author",
                    "onbytarget": null,
		    "usesessions" : usesessions,
                    "showResponseType": showResponseType,
                    "onrecord": my_onrecord,
		    "errorhandler" : my_onerror} 
);
// some state vars
var curPage = 1;
var recPerPage = 10;
var recToShowPageSize = 20;
var recToShow = recToShowPageSize;
var recIDs = {};
var totalRec = 0;
var curDetRecId = '';
var curDetRecData = null;
var curSort = 'relevance';
var curFilter = 'ALL';
var submitted = false;
var SourceMax = 16;
var SubjectMax = 10;
var AuthorMax = 10;
var tab = "recordview"; 

var triedPass = "";
var triedUser = "";

//
// pz2.js event handlers:
//
function my_onerror(error) {
    switch(error.code) {
        // No targets!
    case "8": alert("No resources were selected for the search"); break;
    	// target not configured, this is a pazpar2 bug
        // but for now simply research
    case "9": 
	triggerSearch(); 
	break;
        // authentication
    case "100" : 
	loginFormSubmit();
	//window.location = "login.html"; 
	break;
    default: 
	alert("Unhandled error: " + error.code);
	throw error; // display error in JavaScript console
    }
}

function loginFormSubmit() {
    triedUser = document.loginForm.username.value;
    triedPass = document.loginForm.password.value;
    auth.login( {"username": triedUser,
		"password": triedPass},
	authCb, authCb);
}

function handleKeyPress(e)  
{  
  var key;  
  if(window.event)  
    key = window.event.keyCode;  
  else  
    key = e.which;  

  if(key == 13 || key == 10)  
  {  
      button = document.getElementById('button');
      button.focus();
      button.click();

      return false;  
  }  
  else  
    return true;  
}  

function authCb(authData) {
    if (!authData.loginFailed) {
	triedUser = "";
	triedPass = "";
    }

    if (authData.loggedIn == true) {        
	showhide("recordview");
    }
}

function logOutClick() {
    auth.logOut(authCb, authCb);
}

function loggedOut() {
    var login = document.getElementById("login");
    login.innerHTML = 'Login';
}

function loggingOutFailed() {
    alert("Logging out failed");
}

function login() {
    showhide("login");
}

function logout() {
    auth.logOut(loggedOut, loggingOutFailed, true);
}

function logInOrOut() {
    var loginElement = document.getElementById("login");
    if (loginElement.innerHTML == 'Login')
    	login();
    else
    	logout();
}
function loggedIn() {
    var login = document.getElementById("login");
    login.innerHTML = 'Logout';
    document.getElementById("log").innerHTML = login.innerHTML;
}

function auth_check() {
    auth.check(loggedIn, login);
    domReady();
}

//
// Pz2.js event handlers:
//
function my_oninit() {
    my_paz.stat();
    my_paz.bytarget();
}

function showMoreRecords() {
    var i = recToShow;
    recToShow += recToShowPageSize;
    for ( ; i < recToShow && i < recPerPage; i++) {
	var element = document.getElementById(recIDs[i]);
	if (element)
	    element.style.display = '';
    }
    if (i == recPerPage) {
	var element = document.getElementById('recdiv_END');
	if (element)
	    element.style.display = 'none';
    }
}

function hideRecords() {
    for ( var i = 0; i < recToShow; i++) {
	var element = document.getElementById(recIDs[i]);
	if (element && recIDs != curDetRecId)
	    element.style.display = 'none';
    }
    var element = document.getElementById('recdiv_END');
    if (element)
	element.style.display = 'none';
}

function showRecords() {
    for (var i = 0 ; i < recToShow && i < recPerPage; i++) {
	var element = document.getElementById(recIDs[i]);
	if (element)
	    element.style.display = '';
    }
    var element = document.getElementById('recdiv_END');
    if (element) {
	if (i == recPerPage)
	    element.style.display = 'none';
	else
	    element.style.display = '';
    }
}




function my_onshow(data) {
    totalRec = data.merged;
    // move it out
    var pager = document.getElementById("pager");
    pager.innerHTML = "";
    drawPager(pager);
    pager.innerHTML +='<div class="status">Displaying: ' 
                    + (data.start + 1) + ' to ' + (data.start + data.num) +
                     ' of ' + data.merged + ' (found: ' 
                     + data.total + ')</div>';

    var results = document.getElementById("results");
    
    var html = [];
    if (data.hits == undefined) 
	return ;
    var style = '';
    for (var i = 0; i < data.hits.length; i++) {
        var hit = data.hits[i];
	var recDivID = "recdiv_" + hit.recid; 
	recIDs[i] = recDivID;
	var lines = 0;
	if (i == recToShow)
	    style = ' style="display:none" ';
	html.push('<li class="img" id="' + recDivID + '" ' + style +  '>' );
	html.push('<a class="img" href="#' + i + '" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;" >');
	if (1) {
            var useThumbnails = hit["md-use_thumbnails"];
            var thumburls = hit["md-thumburl"];
            if (thumburls && (useThumbnails == undefined || useThumbnails == "1")) {
		var thumbnailtag = imageHelper.getImageTagByRecId(hit.recid,"md-thumburl", 60, "S"); 
		html.push(thumbnailtag);
	    } else { 
		if (hit["md-isbn"] != undefined) { 
		    var coverimagetag = imageHelper.getImageTagByRecId(hit.recid, "md-isbn", 60, "S"); 
		    if (coverimagetag.length>0) { 
                        html.push(coverimagetag);
		    } else { 
			html.push("&nbsp;");
		    }
		}
	    }
	} 
	html.push("</a>");
	html.push('<a href="#" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;">');
	html.push(hit["md-title"]); 
	html.push("</a>");

	if (hit["md-title-remainder"] != undefined) {
	    html.push('<a href="#" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;">');
	    html.push(hit["md-title-remainder"]);
	    html.push("</a>");
	    lines++;
	}
	if (hit["md-author"] != undefined) {
	    html.push('<a href="#" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;">');
	    html.push(hit["md-author"]);
	    html.push("</a>");
	    lines++;
	}
	else if (hit["md-title-responsibility"] != undefined) {
	    html.push('<a href="#" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;">');
    	    html.push(hit["md-title-responsibility"]);
	    html.push("</a>");
	    lines++;
	}
	for (var idx = lines ; idx < 2 ; idx++) {
	    html.push('<a href="#" id="rec_'+hit.recid + '" onclick="showDetails(this.id);return false;">');
	    html.push("&nbsp;");	    
	    html.push("</a>");
	}
/*
        if (hit.recid == curDetRecId) {
            html.push(renderDetails_iphone(curDetRecData));
        }
*/
      	html.push('</li>');
    }
    if (data.activeclients == 0)
	document.getElementById("loading").style.display = 'none';
/*
    // set up "More..." if needed. 
    style = 'display:none';
    if (recToShow < recPerPage) {
	style = 'display:block';
    }
    html.push('<li class="img" id="recdiv_END" style="' + style + '"><a onclick="showMoreRecords()">More...</a></li>');     
*/
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

function showhide(newtab, hash) {
    var showtermlist = false;
    if (newtab != null)
	tab = newtab;
    
    if (tab == "recordview") {
	document.getElementById("recordview").style.display = '';
	if (hash != undefined)
	    document.location.hash = hash;
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

    if (tab == "detailview") {
	document.getElementById("detailview").style.display = '';
    }
    else {
	document.getElementById("detailview").style.display = 'none';
	var element = document.getElementById("rec_" + curDetRecId);
	if (element != undefined)
	    element.scrollIntoView();

    }
    if (showtermlist == false) 
	document.getElementById("termlist").style.display = 'none';
    else 
	document.getElementById("termlist").style.display = '';

    var tabDiv = document.getElementById("loginDiv");
    if (tab == "login") {
	tabDiv.style.display = '';
    }
    else {
	tabDiv.style.display = 'none';
    }
}

function my_onterm(data) {
    var termlists = [];
    
    termlists.push('<div id="term_xtargets" >');
    termlists.push('<h4 class="termtitle">Sources</h4>');
    termlists.push('<ul class="termlist">');
    termlists.push('<li> <a href="#" target_id="reset_xt" onclick="limitOrResetTarget(\'reset_xt\',\'All\');return false;">All</a></li>');
    for (var i = 0; i < data.xtargets.length && i < SourceMax; i++ ) {
        termlists.push('<li class="termlist"><a href="#" target_id='+data.xtargets[i].id
            + ' onclick="limitOrResetTarget(this.getAttribute(\'target_id\'), \'' + data.xtargets[i].name + '\');return false;">' 
	    + data.xtargets[i].name + ' (' + data.xtargets[i].freq + ')</a></li>');
    }
    termlists.push('</ul>');
    termlists.push('</div>');
     
    termlists.push('<div id="term_subjects" >');
    termlists.push('<h4>Subjects</h4>');
    termlists.push('<ul class="termlist">');
    termlists.push('<li><a href="#" target_id="reset_su" onclick="limitOrResetQuery(\'reset_su\',\'All\');return false;">All</a></li>');
    for (var i = 0; i < data.subject.length && i < SubjectMax; i++ ) {
        termlists.push('<li><a href="#" onclick="limitOrResetQuery(\'su\', \'' + data.subject[i].name + '\');return false;">' 
		       + data.subject[i].name + ' (' + data.subject[i].freq + ')</a></li>');
    }
    termlists.push('</ul>');
    termlists.push('</div>');
            
    termlists.push('<div id="term_authors" >');
    termlists.push('<h4 class="termtitle">Authors</h4>');
    termlists.push('<ul class="termlist">');
    termlists.push('<li><a href="#" onclick="limitOrResetQuery(\'reset_au\',\'All\');return false;">All</a></li>');
    for (var i = 0; i < data.author.length && i < AuthorMax; i++ ) {
        termlists.push('<li><a href="#" onclick="limitOrResetQuery(\'au\', \'' + data.author[i].name +'\');return false;">' 
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
    var detailRecordDiv = document.getElementById('detailrecord');
    if (!detailRecordDiv) 
	return;
    curDetRecData = data;
    var html = renderDetails_iphone(curDetRecData);
    detailRecordDiv.innerHTML = html;
    showhide('detailview');
    document.getElementById("loading").style.display = 'none';
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
    else
    	applicationMode(false);

    var params = parseQueryString(window.location.search);
    if (params.query) {
	document.search.query.value = params.query;
	onFormSubmitEventHandler();
    }
}
 
function applicationMode(newmode) 
{
    var searchdiv = document.getElementById("searchForm");
    if (newmode)
	inApp = newmode;
    if (inApp) {
    	document.getElementById("heading").style.display="none";
       	searchdiv.style.display = 'none';
    }
    else { 
	
	document.getElementById("nav").style.display="";
	document.getElementById("normal").style.display="inline";
	document.getElementById("normal").style.visibility="";
	searchdiv.style.display = '';
	document.search.onsubmit = onFormSubmit;
    }
    callback.init();
}
// when search button pressed
function onFormSubmitEventHandler() 
{
    resetPage();
    document.getElementById("logo").style.display = 'none';
    loadSelect();
    triggerSearch();
    submitted = true;
    return true;
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

function getFacets() {
    var result = "";
    for (var key in querys_server) {
	if (result.length > 0)
	    result += ","
	result += querys_server[key];
    }
    return result;
}

function triggerSearch ()
{
    // Restore to initial page size
    recToShow = recToShowPageSize;
    document.getElementById("loading").style.display = 'inline';
    my_paz.search(document.search.query.value, recPerPage, curSort, curFilter, undefined,
	{
    	   "limit" : getFacets() 
	}
	);
    showhide("recordview");
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

// limit the query after clicking the facet
function limitQueryServer(field, value)
{
    // Check for client field usage
    var fieldname = query_client_server[field];
    if (!fieldname) 
	fieldname = field;	
    
    var newQuery = fieldname + '=' + value.replace(",", "\\,").replace("|", "\\,");
    // Does it already exists?
    if (querys_server[fieldname]) 
	querys_server[fieldname] += "," + newQuery;
    else
	querys_server[fieldname] = newQuery;
//  document.search.query.value += newQuery;
  onFormSubmitEventHandler();
  showhide("recordview");
}



// limit the query after clicking the facet
function removeQuery (field, value) {
	document.search.query.value.replace(' and ' + field + '="' + value + '"', '');
    onFormSubmitEventHandler();
    showhide("recordview");
}

// limit the query after clicking the facet
function limitOrResetQuery (field, value, selected) {
    if (useLimit) {
	limitOrResetQueryServer(field,value, selected);
	return ;
    }
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

// limit the query after clicking the facet
function limitOrResetQueryServer (field, value, selected) {
    if (field.substring(0,6) == 'reset_') {
	var clientname = field.substring(6);
	var fieldname = query_client_server[clientname];
	if (!fieldname) 
	    fieldname = clientname;	
	delete querys_server[fieldname];
	onFormSubmitEventHandler();
	showhide("recordview");
    }
    else 
	limitQueryServer(field, value);
	//alert("limitOrResetQuerry: query after: " + document.search.query.value);
}




// limit by target functions
function limitTarget (id, name)
{
    curFilter = 'pz:id=' + id;
    resetPage();
    loadSelect();
    triggerSearch();
    return false;
}

function delimitTarget ()
{
    curFilter = 'ALL'; 
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
    var onsides = 2;
    var pages = Math.ceil(totalRec / recPerPage);
    
    var firstClkbl = ( curPage - onsides > 0 ) 
        ? curPage - onsides
        : 1;

    var lastClkbl = firstClkbl + 2*onsides < pages
        ? firstClkbl + 2*onsides
        : pages;

    var prev = '<span id="prev">Prev</span><b> | </b>';
    if (curPage > 1)
        var prev = '<a href="#" id="prev" onclick="pagerPrev();">'
        +'Prev</a><b> | </b>';

    var middle = '';
    for(var i = firstClkbl; i <= lastClkbl; i++) {
        var numLabel = i;
        if(i == curPage)
            numLabel = '<b>' + i + '</b>';

        middle += '<a href="#" onclick="showPage(' + i + ')"> '
            + numLabel + ' </a>';
    }
    
    var next = '<b> | </b><span id="next">Next</span>';
    if (pages - curPage > 0)
    var next = '<b> | </b><a href="#" id="next" onclick="pagerNext()">'
        +'Next</a>';

    predots = '';
    if (firstClkbl > 1)
        predots = '...';

    postdots = '';
    if (lastClkbl < pages)
        postdots = '...';

    pagerDiv.innerHTML += '<div class="pager">' 
        + prev + predots + middle + postdots + next + '</div>';
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
    
    // if the same clicked, just show it again
    if (recId == oldRecId) {
	showhide('detailview');
        return;
    }
    // request the record
    document.getElementById("loading").style.display = 'inline';
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
  	if (data["md-title-remainder"] != undefined) {
	      details += ' : <span>' + data["md-title-remainder"] + ' </span>';
  	}
  	if (data["md-author"] != undefined) {
	      details += ' <span><i>'+ data["md-auhtor"] +'</i></span>';
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

var default_tag = 'big';
function renderLine(title, value, tag) {
    if (tag == undefined)
	tag = default_tag;
    if (value != undefined)
        return '<li><h3>' + title + '</h3><' + tag + '>' + value + '</' + tag + '></li>';
    return '';
}

function renderLines(title, values, name, tag) {
    if (tag == undefined)
	tag = default_tag;
    var result = "";
    if (values != undefined && values.length)
	for (var idx = 0 ; idx < values.length ; idx++)
	    if (values[idx][name] != undefined )
		result += values[idx][name] + ' ';
    if (result != "") 
	result = '<li><h3>' + title + '</h3><' + tag + '>' + result + '</' + tag + '></li>';
    return result;
}

// Values is a array of locations. 

function renderLinesURL(title, values, name, url, tag) {
    if (tag == undefined)
	tag = default_tag;
    var result = "";
    result = '<li><h3>' + title + '</h3><' + tag + ' style="display: inline-block;">';
    if (values != undefined && values.length) {
	for (var idx = 0 ; idx < values.length ; idx++) {
	    var url = choose_url(values[idx], auth.proxyUrl);
	    if (url != null)
		result += '<a target="_blank" href="' + url + '">' + values[idx][name] + '</a><br>';
	    else
		result += values[idx][name] + '<br>';
	}
    }
    result += '</' + tag + '></li>';
    return result;
}

function renderLineURL(title, URL, display) {
    if (URL != undefined)
    	return '<li><h3>' + title + '</h3><a href="' + URL + '" target="_blank">' + display + '</a></li>';
    return '';
}

function renderLineEmail(dtitle, email, display) {
    if (email != undefined)
        return '<li><h3>' + title + '</h3> <a href="mailto:' + email + '" target="_blank">' + display + '</a></li>';
    return '';
}


function find_prioritized(values) {
    for (var index = 0; index < values.length; index++) {
	if (values[index] != undefined)
	    return values[index];
    }
    return undefined;
}

function renderDetails_iphone(data, marker)
{
	//return renderDetails(data,marker);

    if (!data) 
	return ""; 
    var details = '<div class="details" id="det_'+data.recid+'" >'
    if (marker) 
    	details += '<h4>'+ marker + '</h4>'; 
    details += '<ul class="field" >';

    var title  = '';
    if (data["md-title"] != undefined) {
    	title +=  data["md-title"];
        if (data["md-title-remainder"] != undefined) {
	    title += '<br><i>' + data["md-title-remainder"] + '</i>';
        }
    }
    details += renderLine('Title', title);

    var author = find_prioritized(
	[
	    data["md-author"],
	    data["md-title-responsibility"]
	]
    );

    var coverimagetag = imageHelper.getImageTagByRecId(data.recid, "md-isbn", undefined, "M");
    details 
    	+=renderLine('Date', 	data["md-date"])
    	+ renderLine('Author', 	data["md-author"])
//    	+ renderLineURL('URL', 	data["md-electronic-url"], data["md-electronic-url"])
    	+ renderLine('Description', 	data["md-description"])
//    	+ renderLines('Subjects', data["location"], "md-subject")
    ;

    details += renderLinesURL('Location', data["location"], "@name", "md-url_recipe");
    details += '<li><a href="#" onclick="showhide(\'recordview\')" style="font-size: 18px;">Back</a></li>';
    if (coverimagetag.length>0) {
	details += renderLine('&nbsp;', coverimagetag);
    }

    details += '</ul></div>';
    return details;
}

//EOF
