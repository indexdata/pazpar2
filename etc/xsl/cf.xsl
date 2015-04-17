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

    <xsl:variable name="startpage" select="/record/page" />
    <xsl:variable name="endpage" select="/record/endpage" />

  <!-- Use medium parameter if given. Default to medium from connector -->
  <xsl:template match="/record">
    <pz:record>
      <pz:metadata type="medium">
        <xsl:choose>
          <xsl:when test="string-length($medium)">
            <xsl:value-of select="$medium" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:if test="medium">
             <xsl:value-of select="medium" />
            </xsl:if>
          </xsl:otherwise>
        </xsl:choose>
      </pz:metadata>

      <!-- calculate md-pages-number for startpage/endpage -->
      <xsl:if test="string-length($startpage)">
        <pz:metadata type="pages-number">
          <xsl:value-of select="$startpage" />
          <xsl:if test="string-length($endpage)">
            <xsl:text>-</xsl:text>
            <xsl:value-of select="$endpage" />
          </xsl:if>
        </pz:metadata>
      </xsl:if>

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
    <pz:metadata type="due" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="due"/>
    </pz:metadata>
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

  <!-- no-op template to avoid printing medium out -->
  <xsl:template match="medium" />

  <xsl:template match="volume">
    <pz:metadata type="volume-number">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="issue">
    <pz:metadata type="issue-number">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="affiliation">
    <pz:metadata type="affiliation-person" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="person"/>
    </pz:metadata>
    <pz:metadata type="affiliation-institution" empty="PAZPAR2_NULL_VALUE">
      <xsl:value-of select="institution"/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="*" >
    <pz:metadata type="{local-name()}">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="text()"/>

</xsl:stylesheet>
