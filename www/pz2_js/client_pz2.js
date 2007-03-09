// very simple client that shows a basic usage of the pz2.js

// create a parameters array and pass it to the pz2's constructor
// then register the form submit event with the pz2.search function

my_paz = new pz2( { "onshow": my_onshow,
                    "onstat": my_onstat,
                    "onterm": my_onterm,
                    "termlist": "subject,author",
                    "onbytarget": my_onbytarget,
                    "onrecord": my_onrecord } );

// wait until the DOM is rady (could have been defined in the HTML)
$(document).ready( function() { document.search.onsubmit = onFormSubmitEventHandler; } );

function onFormSubmitEventHandler() {
    my_paz.search(document.search.query.value, 15, 'relevance');
    return false;
}
//
// pz2.js event handlers:
//
function my_onshow(data) {
    var body = document.getElementById("body");
    body.innerHTML = "";
    for ( i = 0; i < data.hits.length; i++) {
        var hit = data.hits[i];
        body.innerHTML += '<div id="' + hit.recid + '" onclick="my_paz.record(this.id)"><span>' + i + 
                          '. </span><span><b>' + hit["md-title"] +
                          ' </b></span> by <span><i>' + hit["md-author"] + '</i></span></div>';
    }
    body.innerHTML += "<hr/>";
    body.innerHTML += '<div>active clients: ' + data.activeclients + '</div>' +
                     '<div>merged: ' + data.merged + '</div>' +
                     '<div>total: ' + data.total + '</div>' +
                     '<div>start: ' + data.start + '</div>' +
                     '<div>num: ' + data.num + '</div>';
}

function my_onstat(data) {
    var stat = document.getElementById("stat");
    stat.innerHTML = '<div>active clients: ' + data.activeclients + '</div>' +
                     '<div>hits: ' + data.hits + '</div>' +
                     '<div>records: ' + data.records + '</div>' +
                     '<div>clients: ' + data.clients + '</div>' +
                     '<div>searching: ' + data.searching + '</div>';
}

function my_onterm(data) {
    var termlist = document.getElementById("termlist");
    termlist.innerHTML = "";
    termlist.innerHTML  += "<div><b> --Author-- </b></div>";
    for ( i = 0; i < data.author.length; i++ ) {
        termlist.innerHTML += '<div><span>' + data.author[i].name + ' </span><span> (' + data.author[i].freq + ')</span></div>';
    }
    termlist.innerHTML += "<hr/>";
    termlist.innerHTML += "<div><b> --Subject-- </b></div>";
    for ( i = 0; i < data.subject.length; i++ ) {
        termlist.innerHTML += '<div><span>' + data.subject[i].name + ' </span><span> (' + data.subject[i].freq + ')</span></div>';
    }
}

function my_onrecord(data) {
    details = data;
    recordDiv = document.getElementById(data.recid);
    recordDiv.innerHTML = "<table><tr><td><b>Ttle</b> : </td><td>" + data["md-title"] +
                            "</td></tr><tr><td><b>Date</b> : </td><td>" + data["md-date"] +
                            "</td></tr><tr><td><b>Author</b> : </td><td>" + data["md-author"] +
                            "</td></tr><tr><td><b>Subject</b> : </td><td>" + data["md-subject"] + 
                            "</td></tr><tr><td><b>Location</b> : </td><td>" + data["location"][0].name + "</td></tr></table>";

}

function my_onbytarget(data) {
    targetDiv = document.getElementById("bytarget");
    targetDiv.innerHTML = "<tr><td>ID</td><td>Hits</td><td>Diag</td><td>Rec</td><td>State</td></tr>";
    
    for ( i = 0; i < data.length; i++ ) {
        targetDiv.innerHTML += "<tr><td><b>" + data[i].id +
                               "</b></td><td>" + data[i].hits +
                               "</td><td>" + data[i].diagnostic +
                               "</td><td>" + data[i].records +
                               "</td><td>" + data[i].state + "</td></tr>";
    }
}
