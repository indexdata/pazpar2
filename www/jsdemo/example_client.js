/* A very simple client that shows a basic usage of the pz2.js
** $Id: example_client.js,v 1.2 2007-06-22 10:54:46 adam Exp $
*/

// create a parameters array and pass it to the pz2's constructor
// then register the form submit event with the pz2.search function
// autoInit is set to true on default

my_paz = new pz2( { "onshow": my_onshow,
                    "showtime": 500,            //each timer (show, stat, term, bytarget) can be specified this way
                    "pazpar2path": "/pazpar2/search.pz2",
                    "onstat": my_onstat,
                    "onterm": my_onterm,
                    "termlist": "subject,author",
                    "onbytarget": my_onbytarget,
	 	    "usesessions" : true,
                    "onrecord": my_onrecord } );
// some state vars
var curPage = 1;
var recPerPage = 20;
var totalRec = 0;
var curDetRecId = -1;
var curDetRecData = null;

// wait until the DOM is ready
function domReady () 
{ 
    document.search.onsubmit = onFormSubmitEventHandler;
    my_paz.stat();
    my_paz.bytarget();
}

// when search button pressed
function onFormSubmitEventHandler() 
{
    curPage = 1;
    curDetRecId = -1;
    totalRec = 0;
    my_paz.search(document.search.query.value, recPerPage, 'relevance');
    return false;
}

//
// pz2.js event handlers:
//

function my_onshow(data) {
    totalRec = data.merged;
    
    var body = document.getElementById("body");
    body.innerHTML = "";

    body.innerHTML +='<hr/><div style="float: right">Displaying: ' 
                    + data.start + ' to ' + (data.start + data.num) +
                     ' of ' + data.merged + ' (total not merged hits: ' 
                     + data.total + ')</div>';

    body.innerHTML += '<div style="float: clear"><span class="jslink" id="prev" onclick="pagerPrev();">'
                    +'&#60;&#60; Prev</span> <b>|</b> ' 
                    +'<span class="jslink" id="next" onclick="pagerNext()">'
                    +'Next &#62;&#62;</span></div><hr/>';
    
    for (var i = 0; i < data.hits.length; i++) {
        var hit = data.hits[i];
        body.innerHTML += '<div class="record" id="rec_' + hit.recid + '" onclick="showDetails(this.id)">'
                        +'<span>' + (i + 1 + recPerPage * ( curPage - 1)) + '. </span>'
                        +'<span class="jslink"><b>' + hit["md-title"] +
                        ' </b></span> by <span><i>' + hit["md-author"] + '</i></span></div>';

        if ( hit.recid == curDetRecId ) {
            drawCurDetails();
        }
    }
    
}

function my_onstat(data) {
    var stat = document.getElementById("stat");
    stat.innerHTML = '<span>Active clients: '+ data.activeclients
                        + '/' + data.clients + ' | </span>'
                        + '<span>Retrieved records: ' + data.records
                        + '/' + data.hits + '</span>';
}

function my_onterm(data) {
    var termlist = document.getElementById("termlist");
    termlist.innerHTML = "<hr/><b>TERMLISTS:</b><hr/>";
    termlist.innerHTML += '<div class="termtitle">.::Subjects</div>';
    for (var i = 0; i < data.subject.length; i++ ) {
        termlist.innerHTML += '<span>' 
                            + data.subject[i].name 
                            + ' </span><span> (' 
                            + data.subject[i].freq 
                            + ')</span><br/>';
    }
    termlist.innerHTML += "<hr/>";
    termlist.innerHTML += '<div class="termtitle">.::Authors</div>';
    for (var i = 0; i < data.author.length; i++ ) {
        termlist.innerHTML += '<span>' 
                            + data.author[i].name 
                            + ' </span><span> (' 
                            + data.author[i].freq 
                            + ')</span><br/>';
    }

}

function my_onrecord(data) {
    // in case on_show was faster to redraw element
    var detRecordDiv = document.getElementById('det_'+data.recid);
    if ( detRecordDiv )
        return;

    curDetRecData = data;
    drawCurDetails();
}

function my_onbytarget(data) {
    var targetDiv = document.getElementById("bytarget");
    var table = '<thead><tr><td>Target ID</td><td>Hits</td><td>Diags</td>'
                         +'<td>Records</td><td>State</td></tr></thead><tbody>';
    
    for (var i = 0; i < data.length; i++ ) {
        table += "<tr><td>" + data[i].id +
                    "</td><td>" + data[i].hits +
                    "</td><td>" + data[i].diagnostic +
                    "</td><td>" + data[i].records +
                    "</td><td>" + data[i].state + "</td></tr>";
    }

    table += '</tbody>';
    targetDiv.innerHTML = table;

}

// detailed record drawing
function showDetails ( prefixRecId ) {
    var recId = Number(prefixRecId.replace('rec_', ''));
    
    // remove current detailed view if any
    var detRecordDiv = document.getElementById('det_'+curDetRecId);
    // lovin DOM!
    if ( detRecordDiv )
            detRecordDiv.parentNode.removeChild(detRecordDiv);

    // if the same clicked do not redraw
    if ( recId == curDetRecId ) {
        curDetRecId = -1;
        return;
    }

    curDetRecId = recId;

    // request the record
    my_paz.record(recId);
}

function drawCurDetails ()
{
    var data = curDetRecData;
    var recordDiv = document.getElementById('rec_'+data.recid);
    recordDiv.innerHTML += '<div class="details" id="det_'+data.recid+
                            '"><table><tr><td><b>Ttle</b></td><td><b>:</b> '+data["md-title"] +
                            "</td></tr><tr><td><b>Date</b></td><td><b>:</b> " + data["md-date"] +
                            "</td></tr><tr><td><b>Author</b></td><td><b>:</b> " + data["md-author"] +
                            "</td></tr><tr><td><b>Subject</b></td><td><b>:</b> " + data["md-subject"] + 
                            "</td></tr><tr><td><b>Location</b></td><td><b>:</b> " + data["location"][0].name + 
                            "</td></tr></table></div>";
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
