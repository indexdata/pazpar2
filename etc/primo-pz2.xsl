<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
  xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
  xmlns:tmarc="http://www.indexdata.com/turbomarc"
  xmlns:prim="http://www.exlibrisgroup.com/xsd/primo/primo_nm_bib" 
  xmlns:sear="http://www.exlibrisgroup.com/xsd/jaguar/search"

>

  <xsl:output indent="yes" method="xml" version="1.0"
    encoding="UTF-8" />

  <xsl:variable name="type" select="/opt/prim:PrimoNMBib/prim:display/@prim:type"/>
  <xsl:variable name="is_article" select="$type = 'article'" />
  <xsl:variable name="fulltext" select="/opt/prim:PrimoNMBib/prim:delivery/@prim:fulltext"/>
  <xsl:variable name="has_fulltext">
    <xsl:choose>
      <xsl:when test="$fulltext = 'no_fulltext' ">
        <xsl:text>no</xsl:text>
      </xsl:when>
      <xsl:when test="$fulltext = 'fulltext'">
        <xsl:text>yes</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>no</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

    

  <xsl:template name="record-hook" />

  <xsl:template match="/">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="opt">
    <pz:record>
      <xsl:apply-templates />
    </pz:record>
  </xsl:template>

  <xsl:template match="prim:PrimoNMBib">
    <xsl:apply-templates />
  </xsl:template>
  
  <xsl:template match="prim:control"> 
    <xsl:if test="@prim:recordid">
      <pz:metadata type="id">
	<xsl:value-of select="@prim:recordid"/>
      </pz:metadata>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="prim:addata">
    <!--    <xsl:variable name="yearmonthday" select="@date" /> -->

    <xsl:if test="@prim:date">
      <pz:metadata type="date">
	<xsl:value-of select="substring(@date,1,4)" />
      </pz:metadata>
      <pz:metadata type="journal-month">
	<xsl:value-of select="substring(@date,4,1)" />
      </pz:metadata>
    </xsl:if>

    <xsl:if test="@prim:volume">
      <pz:metadata type="journal-number">
	<xsl:value-of select="@prim:volume" />
      </pz:metadata>
    </xsl:if>

    <xsl:if test="@prim:issue">
      <pz:metadata type="issue-number">
	<xsl:value-of select="@prim:issue" />
      </pz:metadata>
    </xsl:if>

    <xsl:if test="@prim:issn">
      <pz:metadata type="issn">
        <xsl:value-of select="@prim:issn" />
      </pz:metadata>
    </xsl:if>

    <xsl:if test="@prim:jtitle">
      <pz:metadata type="journal-title">
	<xsl:value-of select="@prim:jtitle" />
      </pz:metadata>
    </xsl:if>
    
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="prim:delivery">  
    <xsl:if test="$has_fulltext">
      <pz:metadata type="has-fulltext">
        <xsl:value-of select="$has_fulltext" />
      </pz:metadata>
    </xsl:if>

    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="prim:display" name="display" >
    <xsl:if test="@prim:creator">
      <pz:metadata type="author">
        <xsl:value-of select="@prim:creator" />
      </pz:metadata>
    </xsl:if>  


    <xsl:if test="@prim:type">
      <xsl:variable name="type" select="@prim:type"/>
      <pz:metadata type="medium">
	<xsl:choose>
          <xsl:when test="$type ='article' and $has_fulltext = 'yes'">
	    <xsl:text>e-article</xsl:text>
	  </xsl:when>
          <xsl:when  test="$type = 'article' and $has_fulltext = 'no'">
	    <xsl:text>article</xsl:text>
	  </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$type"/>
          </xsl:otherwise>
	</xsl:choose>
<!--        <xsl:value-of select="$type" /> -->
      </pz:metadata>
      <pz:metadata type="debug_isarticle"><xsl:value-of select="$is_article"/></pz:metadata>
    </xsl:if>  


    <xsl:if test="@prim:title">
      <pz:metadata type="title">
	<xsl:value-of select="@prim:title" />
      </pz:metadata>
    </xsl:if>  

    <xsl:if test="@prim:ispartof">
      <pz:metadata type="journal-subpart">
	<xsl:value-of select="@prim:ispartof" />
      </pz:metadata>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="sear:LINKS" >
    <xsl:if test="@sear:openurl"> 
      <pz:metadata type="electronic-url">
	<xsl:value-of select="@sear:openurl"/>
      </pz:metadata>
    </xsl:if>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="prim:search">
    <xsl:if test="@prim:description">
      <pz:metadata type="description">
	<xsl:value-of select="@prim:description" />
      </pz:metadata>
    </xsl:if>


    <xsl:if test="@prim:sub">
      <pz:metadata type="subject">
        <xsl:value-of select="@prim:sub" />
      </pz:metadata>
    </xsl:if>

    <!-- passthrough id data -->
    <xsl:for-each select="pz:metadata">
      <xsl:copy-of select="." />
    </xsl:for-each>
    <!-- other stylesheets importing this might want to define this -->

    <xsl:call-template name="record-hook" />
	
  </xsl:template>

  <xsl:template match="text()" />

</xsl:stylesheet>
