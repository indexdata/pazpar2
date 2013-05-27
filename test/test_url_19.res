<?xml version="1.0" encoding="UTF-8"?>
<service xmlns="http://www.indexdata.com/pazpar2/1.0">
      <icu_chain id="relevance" locale="en">
	<transform rule="[:Control:] Any-Remove"/>
	<tokenize rule="l"/>
	<transform rule="[[:WhiteSpace:][:Punctuation:]`] Remove"/>
	<casemap rule="l"/>
      </icu_chain>
      
      <icu_chain id="sort" locale="en">
	<transform rule="[[:Control:][:WhiteSpace:][:Punctuation:]`] Remove"/>
	<casemap rule="l"/>
      </icu_chain>
      
      <icu_chain id="mergekey" locale="en">
	<tokenize rule="l"/>
	<transform rule="[[:Control:][:WhiteSpace:][:Punctuation:]`] Remove"/>
	<casemap rule="l"/>
      </icu_chain>
      
      <icu_chain id="facet" locale="en">
	<display/>
	<transform rule="Title"/>
      </icu_chain>
      
      <metadata name="url" merge="unique"/>
      <metadata name="title" brief="yes" sortkey="skiparticle" merge="longest" rank="6" mergekey="required"/>
      <metadata name="title-remainder" brief="yes" merge="longest" rank="5"/>
      <metadata name="isbn"/>
      <metadata name="date" brief="yes" sortkey="numeric" type="year" merge="range" termlist="yes"/>
      <metadata name="author" brief="yes" termlist="yes" merge="longest" rank="2" mergekey="optional"/>
      <metadata name="subject" merge="unique" termlist="yes" rank="3"/>
      <metadata name="id"/>
      <metadata name="lccn" merge="unique"/>
      <metadata name="description" brief="yes" merge="longest" rank="3"/>
    </service>