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

  <xsl:template match="/record">
    <pz:record>
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

  <xsl:template match="available">
    <pz:metadata type="available">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="due">
    <pz:metadata type="due">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="location">
    <pz:metadata type="location">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="callno">
    <pz:metadata type="callno">
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

  <xsl:template match="text()"/>

</xsl:stylesheet>
