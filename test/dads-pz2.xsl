<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
  xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
  xmlns:zs="http://www.loc.gov/zing/srw/"
  xmlns:tmarc="http://www.indexdata.com/turbomarc">

  <xsl:output indent="yes" method="xml" version="1.0"
    encoding="UTF-8" />
  <xsl:param name="medium"/>

  <!-- Extract metadata from MARC21/USMARC from streamlined marcxml format 
    http://www.loc.gov/marc/bibliographic/ecbdhome.html -->
  <xsl:template name="record-hook" />


  <xsl:template match="/">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="zs:searchRetrieveResponse">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="zs:records">
    <collection>
      <xsl:apply-templates />
    </collection>
  </xsl:template>

  <xsl:template match="zs:record">
      <xsl:apply-templates />
  </xsl:template>  

  <xsl:template match="zs:recordData">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="doc">
    <collection>
      <xsl:apply-templates />
    </collection>
  </xsl:template>

  <xsl:template match="art">
    <xsl:variable name="journal_title" select="journal/title" />
    <xsl:variable name="journal_issn" select="journal/issn" />
    <xsl:variable name="date" select="localInfo/cdate" />
    <xsl:variable name="description" select="abstract/abstract" />

    <xsl:variable name="vmedium">
      <xsl:choose>
        <xsl:when test="$journal_title">article</xsl:when>
        <xsl:otherwise>
          <xsl:text>other</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    
    <pz:record>
      <xsl:for-each select="author/name">
        <pz:metadata type="author">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>
      
      <xsl:for-each select="article/title">
        <pz:metadata type="title">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="journal/issn">
        <pz:metadata type="issn">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="journal/title">
        <pz:metadata type="journal-title">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>
      
    <xsl:for-each select="journal"> 
        <pz:metadata type="journal-subpart">
            <xsl:text>Vol. </xsl:text><xsl:value-of select="vol" />
            <xsl:text> no. </xsl:text><xsl:value-of select="issue" />
            <xsl:text>(</xsl:text><xsl:value-of select="month" /><xsl:text>-</xsl:text><xsl:value-of select="year" /><xsl:text>)</xsl:text>
            <xsl:text> p. </xsl:text><xsl:value-of select="page" />
        </pz:metadata>
    </xsl:for-each>      

      <xsl:for-each select="localInfo/systemno"> 
        <pz:metadata type="system-control-nr">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

      <pz:metadata type="description">
        <xsl:value-of select="$description" />
      </pz:metadata>
      
      <xsl:for-each select="ctrlT/term">
        <pz:metadata type="subject">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d773">
        <pz:metadata type="citation">
          <xsl:for-each select="*">
            <xsl:value-of select="normalize-space(.)" />
            <xsl:text> </xsl:text>
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <pz:metadata type="medium">
        <xsl:value-of select="$vmedium" />
      </pz:metadata>

      <xsl:for-each select="article/fulltext">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <!-- passthrough id data -->
      <xsl:for-each select="pz:metadata">
        <xsl:copy-of select="." />
      </xsl:for-each>

      <!-- other stylesheets importing this might want to define this -->
      <xsl:call-template name="record-hook" />
	
    </pz:record>


  </xsl:template>

  <xsl:template match="text()" />

</xsl:stylesheet>
