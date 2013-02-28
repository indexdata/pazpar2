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

  <xsl:template match="journaltitle">
    <pz:metadata type="journal-title">
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
    <pz:metadata type="locallocation" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="location"/>
    </pz:metadata>
    <pz:metadata type="callnumber" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="callno"/>
    </pz:metadata>
    <pz:metadata type="available" empty="PAZPAR_NULL_VALUE">
      <xsl:value-of select="available"/>
    </pz:metadata>
    <pz:metadata type="publicnote" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="publicnote"/>
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

  <xsl:template match="*" >
    <pz:metadata type="{local-name()}">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="text()"/>

</xsl:stylesheet>
