<?xml version="1.0" encoding="UTF-8"?>
<!--
    This stylesheet expects Connector Frameworks records
-->
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:siebel="http://loc.gov/siebel/elements/1.0/" >

 <xsl:output indent="yes"
        method="xml"
        version="1.0"
        encoding="UTF-8"/>

  <xsl:param name="medium" />

  <!--
    According to cf/builder/templates/parseTask.cff, connectors can
    also generate a "medium" field, but that is ignored in this
    stylesheet, the rule below instead using an XSLT parameter.
    Should the data element be used in preference when it is included?
  -->

  <xsl:template match="/record">
    <pz:record>
      <pz:metadata type="medium">
         <xsl:value-of select="$medium" />
      </pz:metadata>
      <xsl:apply-templates/>
    </pz:record>
  </xsl:template>

  <!--
      The elements mapped in the following clauses should be kept more
      or less in sync with those named in builder/templates/search.cft
      in the "cf" git module.
  -->

  <xsl:template match="date">
    <pz:metadata type="publication-date">
      <xsl:value-of select="."/>
    </pz:metadata>
    <pz:metadata type="date">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="url">
    <pz:metadata type="electronic-url">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="title">
    <pz:metadata type="title">
      <xsl:value-of select="."/>
    </pz:metadata>
    <pz:metadata type="title-complete">
      <xsl:value-of select="." />
    </pz:metadata>
  </xsl:template>

  <xsl:template match="author">
    <pz:metadata type="author">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="description">
    <pz:metadata type="description">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>
  
  <xsl:template match="publisher">
    <pz:metadata type="publisher">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="subject">
    <pz:metadata type="subject">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="isbn">
    <pz:metadata type="isbn">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="issn">
    <pz:metadata type="issn">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="journaltitle">
    <pz:metadata type="journal-title">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="volume">
    <pz:metadata type="volume">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="issue">
    <pz:metadata type="issue">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="page">
    <pz:metadata type="page">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="endpage">
    <pz:metadata type="endpage">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <!--
    According to cf/builder/templates/parseTask.cff, connectors can
    also generate a "holding" field, but it's not clear how that is
    different from "item".  Perhaps this "item" rule should also
    handle "holding" in the same way?
  -->

  <xsl:template match="item">
    <pz:metadata type="locallocation">
      <xsl:choose>
	<xsl:when test="string-length(location)">
	  <xsl:value-of select="location"/>
	</xsl:when>
	<xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
      </xsl:choose>
    </pz:metadata>
    <pz:metadata type="callnumber">
      <xsl:choose>
	<xsl:when test="string-length(callno)">
	  <xsl:value-of select="callno"/>
	</xsl:when>
	<xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
      </xsl:choose>
    </pz:metadata>
    <pz:metadata type="available">
      <xsl:choose>
	<xsl:when test="string-length(available)">
	  <xsl:value-of select="available"/>
	</xsl:when>
	<xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
      </xsl:choose>
    </pz:metadata>
    <pz:metadata type="publicnote">
      <xsl:choose>
	<xsl:when test="string-length(publicnote)">
	  <xsl:value-of select="publicnote"/>
	</xsl:when>
	<xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
      </xsl:choose>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="due">
    <pz:metadata type="due">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="location">
    <pz:metadata type="locallocation">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="callno">
    <pz:metadata type="callnumber">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="thumburl">
    <pz:metadata type="thumburl">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="score">
    <pz:metadata type="score">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="id">
    <pz:metadata type="id">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="available">
    <pz:metadata type="available">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="citation">
    <pz:metadata type="citation">
      <xsl:value-of select="."/>
      </pz:metadata>
  </xsl:template>

  <xsl:template match="relevance">
    <pz:metadata type="relevance">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="booktitle">
    <pz:metadata type="booktitle">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="copyright">
    <pz:metadata type="copyright">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="copyrightabstract">
    <pz:metadata type="copyrightabstract">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="pubtype">
    <pz:metadata type="pubtype">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="doctype">
    <pz:metadata type="doctype">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="extent">
    <pz:metadata type="extent">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="format">
    <pz:metadata type="format">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="languageitem">
    <pz:metadata type="languageitem">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="languageabstract">
    <pz:metadata type="languageabstract">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="permalink">
    <pz:metadata type="permalink">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="text()"/>

</xsl:stylesheet>
