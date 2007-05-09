/*
*********************************************************************************
** QUERY CLASS ******************************************************************
*********************************************************************************
*/
var pzQuery = function()
{
    this.simpleQuery = '';
    this.singleFilter = null;
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
        this.simpleFilter = null;
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
            ccl = '"'+this.simpleQuery+'"';
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
        this.filterHash[this.filterHash.length] = filter;
        this.filterNums++
        return  this.filterHash.length - 1;
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
        delete this.filterHash[index];
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
        if( this.singleFilter != null ) {
            return 'pz:id='+this.singleFilter.id;
        } 
        else if( this.filterNums <= 0 ) {
            return undefined;
        }

        var filter = 'pz:id=';
        for(var i = 0; i < this.filterHash.length; i++)
        {
            if (this.filterHash[i] == undefined) continue;
            if (filter > 'pz:id=') filter = filter + '|';            
            filter += this.filterHash[i].id; 
        }
        return filter;
    },
    totalLength: function()
    {
        var simpleLength = this.simpleQuery != '' ? 1 : 0;
        return this.advTerms.length + simpleLength;
    },
    clearSingleFilter: function()
    {
        this.singleFilter = null;
    },
    setSingleFilter: function(name, value)
    {
        this.singleFilter = {"name": name, "id": value };
    },
    getSingleFilterName: function()
    {
        return this.singleFilter.name;
    }
}
