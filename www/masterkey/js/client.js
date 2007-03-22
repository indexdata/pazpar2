// very simple client that shows a basic usage of the pz2.js

// create a parameters array and pass it to the pz2's constructor
// then register the form submit event with the pz2.search function

var my_paz = new pz2( { "onshow": my_onshow,
                    //"showtime": 1000,
                    //"onstat": my_onstat,
                    "onterm": my_onterm,
                    "termlist": "subject,author,xtargets,date",
                    //"onbytarget": my_onbytarget,
                    "onrecord": my_onrecord } );

var currentSort = 'relevance';
var currentResultsPerPage = 20;
var currentQuery = null;
var currentDetailed = null;

var termStartup = true;    //some things should be done only once

// wait until the DOM is ready (could have been defined in the HTML)
$(document).ready( function() { 
                    document.search.onsubmit = onFormSubmitEventHandler;
                    } );

function onFormSubmitEventHandler() {
    currentQuery = document.search.query.value;
    my_paz.search(document.search.query.value, 20, 'relevance');
    $('div.content').show();
    $("div.leftbar").show();    
    return false;
}
//
// pz2.js event handlers:
//
function my_onshow(data)
{
    var recsBody = $('div.records');
    recsBody.empty();
    
    for (var i = 0; i < data.hits.length; i++) {
        var title = data.hits[i]["md-title"] || 'N/A';
        var author = data.hits[i]["md-author"] || '';
        var id = data.hits[i].recid;
        var count = data.hits[i].count || 1;
        
        recsBody.append('<a href="#" class="result">'+(i+1)+'.</a>\n');
        
        var recBody = $('<div class="record" id="rec_'+id+'></div>').appendTo(recsBody);
        recBody.append('<a href="#">'+title+'</a>\n');

        if( author )
            recBody.append('<i> by </i>');
            $('<a href="#" name="author">'+author+'</a>\n').click(function(){ refine(this.name, this.firstChild.nodeValue) }).appendTo(recBody);
    }
}

/*

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
*/

function my_onstat(data)
{
/*
    var stat = document.getElementById("stat");
    stat.innerHTML = '<div>active clients: ' + data.activeclients + '</div>' +
                     '<div>hits: ' + data.hits + '</div>' +
                     '<div>records: ' + data.records + '</div>' +
                     '<div>clients: ' + data.clients + '</div>' +
                     '<div>searching: ' + data.searching + '</div>';
*/
}

function my_onterm(data)
{
    global = data;
    var termLists = $("#termlists");
    //termLists.empty()

    if(termStartup)
    {
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listName = key;
            if (key == "xtargets")
                listName = "institution";

            var termList = $('<div class="termlist" id="term_'+key+'"/>').appendTo(termLists);
            var termTitle = $('<div class="termTitle"><a href="#" class="unselected">'+listName+'</a></div>').appendTo(termList);
            termTitle.click(function(){
                                if( this.firstChild.className == "selected" ){
                                    this.firstChild.className = "unselected";
                                    $(this.nextSibling).hide();
                                } else {
                                    this.firstChild.className = "selected";
                                    $(this.nextSibling).show();
                                }
                            });

            listEntries = $('<div class="termEntries"></div>');
            listEntries.hide();
            listEntries.appendTo(termList);

            for(var i = 0; i < data[key].length; i++)
            {
                if (key == "xtargets"){
                    var listItem = $('<a href="#" class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name+
                                '<span> ('+data[key][i].freq+')</span></a>').appendTo(listEntries);
                    listItem.click(function(){ 
                        refine(this.name, this.attributes[0].nodeValue) });
                } else {
                    var listItem = $('<a href="#" class="sub" name="'+key+'">'+data[key][i].name+
                                    '<span> ('+data[key][i].freq+')</span></a>').appendTo(listEntries);
                    listItem.click(function(){ refine(this.name, this.firstChild.nodeValue) });
                }
            }        
            $('<hr/>').appendTo(termLists);
        }
        termStartup = false;
    } 
    else 
    {
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listEntries = $('#term_'+key).children('.termEntries');
            listEntries.empty()

            for(var i = 0; i < data[key].length; i++){
                if (key == "xtargets"){
                    var listItem = $('<a href="#" class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name+
                                '<span> ('+data[key][i].freq+')</span></a>').appendTo(listEntries);
                    listItem.click(function(){ 
                        refine(this.name, this.attributes[0].nodeValue) });
                } else {
                    var listItem = $('<a href="#" class="sub" name="'+key+'">'+data[key][i].name+
                                    '<span> ('+data[key][i].freq+')</span></a>').appendTo(listEntries);
                    listItem.click(function(){ refine(this.name, this.firstChild.nodeValue) });
                }
            }         
        }
    }
}

/*
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
*/

function my_onrecord(data)
{
    /*
    details = data;
    recordDiv = document.getElementById(data.recid);
    recordDiv.innerHTML = "<table><tr><td><b>Ttle</b> : </td><td>" + data["md-title"] +
                            "</td></tr><tr><td><b>Date</b> : </td><td>" + data["md-date"] +
                            "</td></tr><tr><td><b>Author</b> : </td><td>" + data["md-author"] +
                            "</td></tr><tr><td><b>Subject</b> : </td><td>" + data["md-subject"] + 
                            "</td></tr><tr><td><b>Location</b> : </td><td>" + data["location"][0].name + "</td></tr></table>";
                            */

}

function my_onbytarget(data)
{
    /*
    targetDiv = document.getElementById("bytarget");
    targetDiv.innerHTML = "<tr><td>ID</td><td>Hits</td><td>Diag</td><td>Rec</td><td>State</td></tr>";
    
    for ( i = 0; i < data.length; i++ ) {
        targetDiv.innerHTML += "<tr><td><b>" + data[i].id +
                               "</b></td><td>" + data[i].hits +
                               "</td><td>" + data[i].diagnostic +
                               "</td><td>" + data[i].records +
                               "</td><td>" + data[i].state + "</td></tr>";
    }
    */
}

function refine(field, value)
{
    var query = '';
    var filter = undefined;
    
    switch(field) {
        case "author": query = ' and au="'+value+'"'; break;
        case "title": query = ' and ti="'+value+'"'; break;
        case "date": query = ' and date="'+value+'"'; break;
        case "subject": query = ' and su="'+value+'"'; break;
        case "xtarget": filter = 'id='+value; break;
    }

    my_paz.search(currentQuery + query, currentResultsPerPage, currentSort, filter);    
}
