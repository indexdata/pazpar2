/*
** $Id: client.js,v 1.21 2007-04-30 03:35:57 quinn Exp $
** MasterKey - pazpar2's javascript client .
*/

/* start with creating pz2 object and passing it event handlers*/
var my_paz = new pz2({ 
                    "onshow": my_onshow,
                    //"showtime": 1000,
                    //"onstat": my_onstat,
                    "onterm": my_onterm,
                    "termlist": "xtargets,subject,author,date",
                    //"onbytarget": my_onbytarget,
                    "onrecord": my_onrecord,
                    "errorhandler": my_errorhandler
                    });

/* some state variable */
var currentSort = 'relevance';
var currentResultsPerPage = 20;
var currentPage = 0;
var curQuery = new pzQuery();

var currentDetailedId = null;
var currentDetailedData = null;

var termStartup = true;
var advancedOn = false;

var showBriefLocations = false;

/* wait until the DOM is ready and register basic handlers */
$(document).ready( function() { 
                    document.search.onsubmit = onFormSubmitEventHandler;

                    document.search.query.value = '';
                    document.search.title.value = '';
                    document.search.author.value = '';
                    document.search.subject.value = '';
                    document.search.date.value = '';
                    
                    $('#advanced').click(toggleAdvanced);

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
} );

/* search button event handler */
function onFormSubmitEventHandler() {
    loadQueryFromForm();
    curQuery.clearFilter();
    fireSearch();
    drawBreadcrumb();
    $('div.motd').empty();
    $('div.content').show();
    $("div.leftbar").show();
    return false;
}

/*
*********************************************************************************
** pz2 Event Handlers ***********************************************************
*********************************************************************************
*/
function my_errorhandler(err)
{
    switch (err.message) 
    {
        case 'QUERY': alert("Your query was not understood. Please rephrase."); break;
        default: alert(err.message);
    }
}

/*
** data.hits["md-title"], data.hits["md-author"], data.hits.recid, data.hits.count
** data.activeclients, data.merged, data.total, data.start, data.num 
*/
function my_onshow(data)
{
    var recsBody = $('div.records');
    recsBody.empty();
    
    for (var i = 0; i < data.hits.length; i++) {
        var title = data.hits[i]["md-title"] || 'N/A';
        var author = data.hits[i]["md-author"] || '';
        var id = data.hits[i].recid;
        var count = data.hits[i].count || 1;
        
        var recBody = $('<div class="record" id="rec_'+id+'"></div>');
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
        
        if( author ) {
            recBody.append('<i> by </i>');
            $('<a name="author" class="recAuthor">'+author+'</a>\n').click(function(){ 
                            refine("authoronly", this.firstChild.nodeValue) }).appendTo(recBody);
        }

        if( currentDetailedId == id ) {
            var detailBox = $('<div class="detail"></div>').appendTo(recBody);
            drawDetailedRec(detailBox);
        }

	if (showBriefLocations) {
	    var location = data.hits[i]['location'];
	    var l;
	    var list = '';
	    for (l in location) {
		if (list)
		    list += ', ';
		list += location[l].name;
	    }
	    recBody.append('<span> ('+list+')</span>');
	}
	else {
	    if( count > 1 ) {
		recBody.append('<span> ('+count+')</span>');
	    }
	}

        recsBody.append('<div class="resultNum">'+(currentPage*currentResultsPerPage+i+1)+'.</a>');
        recsBody.append(recBody);
    }
    drawPager(data.merged, data.total);    
}

/*
** data.activeclients, data.hits, data.records, data.clients, data.searching
*/
function my_onstat(data){}

/*
** data[listname]: name, freq, [id]
*/
function my_onterm(data)
{
    if(termStartup)
    {
        var termLists = $("#termlists");
        
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listName = key;
            var listClass = "unselected";

            if (key == "xtargets"){
                listName = "resource";
                listClass = "selected";
            }

            var termList = $('<div class="termlist" id="term_'+key+'"/>').appendTo(termLists);
            var termTitle = $('<div class="termTitle"><a class="'+listClass+'">'+listName+'</a></div>').appendTo(termList);
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
            if (key != "xtargets") listEntries.hide();
            listEntries.appendTo(termList);

            for(var i = 0; i < data[key].length; i++)
            {
                if (key == "xtargets"){
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name +'<span> ('+data[key][i].freq+')</span>'+'</a>');
                    listItem.click(function(){ 
                        refine(this.name, this.attributes[0].nodeValue, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name
                            +'<span> ('+data[key][i].freq+')</span>'+'</a>');
                    listItem.click(function(){ refine(this.name, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
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
            if( data[key].length ) listEntries.empty();

            for(var i = 0; i < data[key].length; i++){
                if (key == "xtargets"){
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name+'<span> ('+data[key][i].freq+')</span>'+'</a>').click(function(){ 
                                    refine(this.name, this.attributes[0].nodeValue, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name
                                +'<span> ('+data[key][i].freq+')</span>'+'</a>').click(function(){ 
                                                                        refine(this.name, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                }
            }         
        }
    }
}

/*
** data["md-title"], data["md-date"], data["md-author"], data["md-subject"], data["location"][0].name
*/
function my_onrecord(data)
{
    currentDetailedData = data;
    drawDetailedRec();
}

/*
** data[i].id, data[i].hits, data[i].diagnostic, data[i].records, data[i].state
*/
function my_onbytarget(data){}

/*
*********************************************************************************
** HELPER FUNCTIONS *************************************************************
*********************************************************************************
*/
function fireSearch()
{
    $('div.showing').empty().text('No records to show.');
    $('div.pages').empty().html('&nbsp;');
    $('div.records').empty();
    currentDetailedId = null;
    if( !curQuery.totalLength() )
        return false;
    my_paz.search(curQuery.toCCL(), currentResultsPerPage, currentSort, curQuery.getFilterString() );
}

function toggleAdvanced()
{
    if(advancedOn){
        $("div.advanced").hide();
        $("div.search").height(73);
        advancedOn = false;
        $("#advanced").text("Advanced search");
    } else {
        $("div.search").height(173);
        $("div.advanced").show();
        advancedOn = true;
        $("#advanced").text("Simple search");
        loadFormFieldsFromQuery();
    }
}

function drawDetailedRec(detailBox)
{
    if( detailBox == undefined )
        detailBox = $('<div class="detail"></div>').appendTo($('#rec_'+currentDetailedId));
    
    var detailTable = $('<table></table>');
    var recLocation = currentDetailedData["location"];

    var hdtarget;
    if( recLocation ) {
        hdtarget = $('<tr><td class="item">Available at:</td></tr>');
	detailTable.append(hdtarget);

	for(var i=0; i < recLocation.length; i++)
	{
	    if (!hdtarget)
		hdtarget = $('<tr><td class="item">&nbsp;</td></tr>').appendTo(detailTable);
	    var url = recLocation[i]["md-url"];
	    var description = recLocation[i]["md-description"];
	    hdtarget.append('<td><b>'+recLocation[i].name+'</b></td>');
	    if (description)
		detailTable.append($('<tr><td>&nbsp</td><td>'+description+'</td></tr>'));
	    if (url) {
		var tline = $('<tr><td>&nbsp;</td></tr>');
		var td = $('<td></td>').appendTo(tline);
		var tlink = $('<a>Go to resource</a>');
		tlink.attr('href', url);;
		tlink.attr('target', '_blank');
		tlink.appendTo(td);
		detailTable.append(tline);
	    }
	    hdtarget = undefined;
	}
    }

    detailTable.appendTo(detailBox);
}

function refine(field, value, opt)
{
    switch(field) {
        case "authoronly":  curQuery.reset(); curQuery.addTerm('au', value); break;
        case "author":  curQuery.addTerm('au', value); break;
        case "title":   curQuery.addTerm('ti', value); break;
        case "date":    curQuery.addTerm('date', value); break;
        case "subject": curQuery.addTerm('su', value); break;
        case "xtarget": curQuery.setFilter(opt, value); break;
    }

    if(advancedOn)
        loadFormFieldsFromQuery();

    currentPage = 0;
    drawBreadcrumb();
    fireSearch();
}

function loadQueryFromForm()
{
    curQuery.reset();
    curQuery.simpleQuery = document.search.query.value;

    if( advancedOn )
    {
        curQuery.addTermsFromList(document.search.author.value, 'au');
        curQuery.addTermsFromList(document.search.title.value, 'ti');
        curQuery.addTermsFromList(document.search.date.value, 'date');
        curQuery.addTermsFromList(document.search.subject.value, 'su');
    }
}

function loadFormFieldsFromQuery()
{
    document.search.author.value = '';
    document.search.title.value = '';
    document.search.date.value = '';
    document.search.subject.value = '';

    for(var i = 0; i < curQuery.numTerms; i++)
    {
        switch( curQuery.getTermFieldByIdx(i) )
        {
            case "au": document.search.author.value += curQuery.getTermValueByIdx(i) + '; '; break;
            case "ti": document.search.title.value += curQuery.getTermValueByIdx(i) + '; '; break;
            case "date": document.search.date.value += curQuery.getTermValueByIdx(i) + '; '; break;
            case "su": document.search.subject.value += curQuery.getTermValueByIdx(i) + '; '; break;
        }
    }
}

function drawPager(max, hits)
{
    var firstOnPage = currentPage * currentResultsPerPage + 1;
    var lastOnPage = (firstOnPage + currentResultsPerPage - 1) < max ? (firstOnPage + currentResultsPerPage - 1) : max;

    var results = $('div.showing');
    results.empty();
    results.append('Displaying: <b>'+firstOnPage+'</b> to <b>'+lastOnPage+
                            '</b> of <b>'+max+'</b> (total hits: '+hits+')');
    var pager = $('div.pages');
    pager.empty();
    
    if ( currentPage > 0 ){
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(0));
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="previous_inactive">Previous</a>');

    var numPages = Math.ceil(max / currentResultsPerPage);

    var start = ( currentPage - 5 > 0 ? currentPage - 5 : 1 );
    var stop =  ( start + 12 < numPages ? start + 12 : numPages );

    if (start > 1) $('<span>... </span>').appendTo(pager);
    
    for(var i = start; i <= stop; i++)
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

    if (stop < numPages) $('<span> ...</span>').appendTo(pager);

    if ( currentPage < (numPages-1) ){
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(0));
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="next_inactive">Next</a>');
}

function drawBreadcrumb()
{
    var bc = $("#breadcrumb");
    bc.empty();
    
    if(curQuery.filterNums) $('<strong id="filter"><a>'+curQuery.getFilterName(0)+'</a>: </strong>').click(function() {
                                curQuery.removeFilter(0);
                                refine();
                                }).appendTo(bc);

    bc.append('<span>'+curQuery.simpleQuery+'</span>');

    for(var i = 0; i < curQuery.numTerms; i++){
        bc.append('<strong> + </strong>');
        var bcLink = $('<a id="pos_'+i+'">'+curQuery.getTermValueByIdx(i)+'</a>').click(function() { 
                                            curQuery.removeTermByIdx(this.id.split('_')[1]);
                                            refine(); 
                                            });
        bc.append(bcLink);
    }
}
