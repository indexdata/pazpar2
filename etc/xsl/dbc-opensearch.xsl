<?xml version="1.0" encoding="UTF-8"?>
<!--

    This stylesheet expects oai/dc records
-->
<xsl:stylesheet
    version="1.0"
    xmlns="http://oss.dbc.dk/ns/opensearch" 
    xmlns:os="http://oss.dbc.dk/ns/opensearch" 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dcterms="http://purl.org/dc/terms/"
    xmlns:dkabm="http://biblstandard.dk/abm/namespace/dkabm/" 
    xmlns:ac="http://biblstandard.dk/ac/namespace/" 
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
>

 <xsl:output indent="yes"
        method="xml"
        version="1.0"
        encoding="UTF-8"/>

<!--
  <xsl:template match="os:searchResult">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="os:collection">
    <xsl:apply-templates/>
  </xsl:template>
-->

  <xsl:template match="dkabm:record">
    <pz:record>
      <pz:metadata type="id">
        <xsl:value-of select="//os:identifier"/>
      </pz:metadata>

      <xsl:for-each select="dc:title">
        <pz:metadata type="title">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:date">
        <pz:metadata type="date">
      	  <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:subject">
        <pz:metadata type="subject">
	        <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:creator">
	      <pz:metadata type="author">
          <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:description">
        <pz:metadata type="description">
	        <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:identifier">
        <pz:metadata type="electronic-url">
	        <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dc:type">
        <pz:metadata type="medium">
	        <xsl:value-of select="."/>
	      </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="dcterms:bibliographicCitation">
        <pz:metadata type="citation">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

    </pz:record>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="identifier">
    <xsl:variable name="identifier">
      <xsl:value-of select="identifier"/>
    </xsl:variable>
    <xsl:value-of select="."/>
  </xsl:template>


  <xsl:template match="text()"/>

</xsl:stylesheet>
