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
var currentPage = 0;

var currentDetailedId = null;
var currentDetailedData = null;

var termStartup = true;    //some things should be done only once

// wait until the DOM is ready (could have been defined in the HTML)
$(document).ready( function() { 
                    document.search.onsubmit = onFormSubmitEventHandler;
                    } );

function onFormSubmitEventHandler() {
    currentQuery = document.search.query.value;
    $('#sort').change(function(){ 
                    currentSort = this.value;
                    currentPage = 0;
                    my_paz.show(0, currentResultsPerPage, currentSort);
                    });
    $('#perpage').change(function(){ 
                    currentResultsPerPage = this.value;
                    currentPage = 0;
                    my_paz.show(0, currentResultsPerPage, currentSort);
                    });
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
    global = data;
    var recsBody = $('div.records');
    recsBody.empty();
    
    for (var i = 0; i < data.hits.length; i++) {
        var title = data.hits[i]["md-title"] || 'N/A';
        var author = data.hits[i]["md-author"] || '';
        var id = data.hits[i].recid;
        var count = data.hits[i].count || 1;
        
        var recBody = $('<div class="record" id="rec_'+id+'></div>');
        var aTitle = $('<a class="recTitle">'+title+'</a>').appendTo(recBody);
        aTitle.click(function(){
                        var clickedId = this.parentNode.id.split('_')[1];
                        if(currentDetailedId == clickedId){
                            $(this.parentNode.lastChild).remove();
                            currentDetailedId = null;
                            return;
                        } else if (currentDetailedId != null) {
                            $('#rec_'+currentDetailedId).children('.detail').remove();
                        }
                        currentDetailedId = clickedId;
                        my_paz.record(currentDetailedId);
                        });
        
        if( author )
            recBody.append('<i> by </i>');
            $('<a name="author" class="recAuthor">'+author+'</a>\n').click(function(){ refine(this.name, this.firstChild.nodeValue) }).appendTo(recBody);

        if( currentDetailedId == id ){
            var detailBox = $('<div class="detail"></div>').appendTo(recBody);
            drawDetailedRec(detailBox);
        }

        recsBody.append('<div class="resultNum">'+(currentPage*currentResultsPerPage+i+1)+'.</a>');
        recsBody.append(recBody);

    }

    drawPager(data.merged, data.total);    
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
    var termLists = $("#termlists");

    if(termStartup)
    {
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listName = key;
            if (key == "xtargets")
                listName = "institution";

            var termList = $('<div class="termlist" id="term_'+key+'"/>').appendTo(termLists);
            var termTitle = $('<div class="termTitle"><a class="unselected">'+listName+'</a></div>').appendTo(termList);
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
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name+
                                '<span> ('+data[key][i].freq+')</span></a>').appendTo(listEntries);
                    listItem.click(function(){ 
                        refine(this.name, this.attributes[0].nodeValue) });
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name+
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
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name+
                                '<span> ('+data[key][i].freq+')</span></a>').click(function(){ 
                                                                        refine(this.name, this.attributes[0].nodeValue) });
                    listItem.appendTo(listEntries);
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name+
                                    '<span> ('+data[key][i].freq+')</span></a>').click(function(){ 
                                                                        refine(this.name, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                }
            }         
        }
    }
}

function my_onrecord(data)
{
    currentDetailedData = data;
    drawDetailedRec();
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

function drawDetailedRec(detailBox)
{
    if( detailBox == undefined )
        detailBox = $('<div class="detail"></div>').appendTo($('#rec_'+currentDetailedId));
    
    detailBox.append('Details:<hr/>');
    var detailTable = $('<table></table>');
    var recDate = currentDetailedData["md-date"];
    var recSubject = currentDetailedData["md-subject"];
    var recLocation = currentDetailedData["location"];

    if( recDate )
        detailTable.append('<tr><td class="item">Published:</td><td>'+recDate+'</td></tr>');
    if( recSubject )
        detailTable.append('<tr><td class="item">Subject:</td><td>'+recSubject+'</td></tr>');
    if( recLocation )
        detailTable.append('<tr><td class="item">Available at:</td><td>&nbsp;</td></tr>');

    for(var i=0; i < recLocation.length; i++)
    {
        detailTable.append('<tr><td class="item">&nbsp;</td><td>'+recLocation[i].name+'</td></tr>');
    }

    detailTable.appendTo(detailBox);
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
    
    currentPage = 0;
    my_paz.search(currentQuery + query, currentResultsPerPage, currentSort, filter);    
}

function drawPager(max, hits)
{
    var firstOnPage = currentPage * currentResultsPerPage + 1;
    var lastOnPage = (firstOnPage + currentResultsPerPage - 1) < max ? (firstOnPage + currentResultsPerPage - 1) : max;

    var results = $('div.showing');
    results.empty();
    results.append('Displaying: <b>'+firstOnPage+'</b> to <b>'+lastOnPage+
                            '</b> of <b>'+max+'</b>'); //(total hits: '+hits+')');
    var pager = $('div.pages');
    pager.empty();
    
    if ( currentPage > 0 ){
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(0));
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="previous_inactive">Previous</a>');

    var numPages = Math.ceil(max / currentResultsPerPage);
    
    for(var i = 1; i <= numPages; i++)
    {
        if( i == (currentPage + 1) ){
           $('<a class="select">'+i+'</a>').appendTo(pager);
           continue;
        }
        var pageLink = $('<a class="page">'+i+'</a>');
        var plClone = pageLink.clone();

        pageLink.click(function() { 
            my_paz.showPage(this.firstChild.nodeValue - 1);
            currentPage = (this.firstChild.nodeValue - 1);
            });

        plClone.click(function() { 
            my_paz.showPage(this.firstChild.nodeValue - 1);
            currentPage = (this.firstChild.nodeValue - 1);
            });

        //nasty hack
        pager.eq(0).append(pageLink);
        pager.eq(1).append(plClone);
    }

    if ( currentPage < (numPages-1) ){
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(0));
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="next_inactive">Next</a>');
}
