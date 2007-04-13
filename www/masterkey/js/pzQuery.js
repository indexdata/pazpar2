/*
*********************************************************************************
** QUERY CLASS ******************************************************************
*********************************************************************************
*/
var pzQuery = function()
{
    this.simpleQuery = '';
    this.advTerms = new Array();
    this.filterHash = new Array();
    this.numTerms = 0;
    this.filterNums = 0;
};
pzQuery.prototype = {
    reset: function()
    {
        this.simpleQuery = '';
        this.advTerms = new Array();
        this.filterHash = new Array();
        this.numTerms = 0;
    },
    addTerm: function(field, value)
    {
        var term = {"field": field, "value": value};
        this.advTerms[this.numTerms] = term;
        this.numTerms++;
    },
    getTermValueByIdx: function(index)
    {
        return this.advTerms[index].value;
    },
    getTermFieldByIdx: function(index)
    {
        return this.advTerms[index].field;
    },
    /* semicolon separated list of terms for given field*/
    getTermsByField: function(field)
    {
        var terms = '';
        for(var i = 0; i < this.advTerms.length; i++)
        {
            if( this.advTerms[i].field == field )
                terms = terms + this.queryHas[i].value + ';';
        }
        return terms;
    },
    addTermsFromList: function(inputString, field)
    {
        var inputArr = inputString.split(';');
        for(var i=0; i < inputArr.length; i++)
        {
            if(inputArr[i].length < 3) continue;
            this.advTerms[this.numTerms] = {"field": field, "value": inputArr[i] };
            this.numTerms++;
        }
    },
    removeTermByIdx: function(index)
    {
        this.advTerms.splice(index, 1);
        this.numTerms--;
    },
    toCCL: function()
    {   
        // TODO escape the characters
        var ccl = '';
        if( this.simpleQuery != '')
            ccl = this.simpleQuery;
        for(var i = 0; i < this.advTerms.length; i++)
        {
            if (ccl != '') ccl = ccl + ' and ';
            ccl = ccl + this.advTerms[i].field+'="'+this.advTerms[i].value+'"';
        }
        return ccl;
    },
    addFilter: function(name, value)
    {
        var filter = {"name": name, "id": value };
        this.filterHash[this.filterNums] = filter;
        this.filterNums++;
    },
    setFilter: function(name, value)
    {
        this.filterHash = new Array();
        this.filterNums = 0;
        this.addFilter(name, value);
    },
    getFilter: function(index)
    {
        return this.filterHash[index].id;
    },
    getFilterName: function(index)
    {
        return this.filterHash[index].name;
    },
    removeFilter: function(index)
    {
        this.filterHash.splice(index, 1);
        this.filterNums--;
    },
    clearFilter: function()
    {
        this.filterHash = new Array();
        this.filterNums = 0;
    },
    getFilterString: function()
    {
        //temporary
        if(!this.filterNums)
            return undefined;
        var filter = '';
        for(var i = 0; i < this.filterHash.length; i++)
        {
            if (filter != '') filter = filter + '|';            
            filter += 'pz:id='+this.filterHash[i].id; 
        }
        return filter;
    },
    totalLength: function()
    {
        var simpleLength = this.simpleQuery != '' ? 1 : 0;
        return this.advTerms.length + simpleLength;
    }
}
