function init() {
    my_paz = new pz2( { "onshow": my_onshow, "onstat": my_onstat, "onterm": my_onterm }, true );
}

function my_onshow(data) {
    var body = document.getElementById("body");

    body.innerHTML = '<div>active clients: ' + data.activeclients + '</div>' +
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
    for ( i = 0; i < data.author.length; i++ ) {
        termlist.innerHTML += '<div><span>' + data.author[i].name + ' </span><span> (' + data.author[i].freq + ')</span></div>';
    }
}
