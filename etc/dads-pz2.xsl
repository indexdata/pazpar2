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
    <xsl:variable name="date" select="journal/year" />
    <xsl:variable name="description" select="abstract/abstract" />

    <xsl:variable name="has_fulltext" select="article/fulltext"/>
    <xsl:variable name="has_title" select="article/title"/>

    <xsl:variable name="vmedium">
      <xsl:choose>
        <xsl:when  test="$has_title and $has_fulltext">
	  <xsl:text>e-article</xsl:text>
	</xsl:when>
        <xsl:when  test="$has_title and not($has_fulltext)">
	  <xsl:text>article</xsl:text>
	</xsl:when>
        <xsl:otherwise>
          <xsl:text>other</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    
    <pz:record>

      <xsl:for-each select="localInfo/systemno"> 
        <pz:metadata type="id">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

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

      <xsl:for-each select="journal/vol">
        <pz:metadata type="journal-number">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="journal/issue">
        <pz:metadata type="issue-number">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="journal"> 
        <pz:metadata type="journal-subpart">
          <xsl:text>Vol. </xsl:text><xsl:value-of select="vol" /><xsl:text>,</xsl:text>
          <xsl:if test="issue">
	    <xsl:text> no. </xsl:text><xsl:value-of select="issue" />
	  </xsl:if>
	  <xsl:text> (</xsl:text>
	  <xsl:choose>
	    <xsl:when test="month='01'">
	      <xsl:text>Jan. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='02'">
	      <xsl:text>Feb. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='03'">
	      <xsl:text>Mar. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='04'">
	      <xsl:text>Apr. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='05'">
	      <xsl:text>May </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='06'">
	      <xsl:text>June </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='07'">
	      <xsl:text>July </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='08'">
	      <xsl:text>Aug. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='09'">
	      <xsl:text>Sept. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='10'">
	      <xsl:text>Oct. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='11'">
	      <xsl:text>Nov. </xsl:text>
	    </xsl:when>
	    <xsl:when test="month='12'">
	      <xsl:text>Dec. </xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="month"/><xsl:text> </xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:value-of select="year" /><xsl:text>)</xsl:text>
	  <xsl:if test="page"> 
            <xsl:text>, p. </xsl:text><xsl:value-of select="page" />
	  </xsl:if>
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

      <xsl:for-each select="article/fulltext">
	<pz:metadata type="fulltext">
          <xsl:value-of select="." />
	</pz:metadata>
      </xsl:for-each>

      <pz:metadata type="medium">
        <xsl:value-of select="$vmedium" />
<!--
        <xsl:if test="string-length($electronic) and $vmedium != 'electronic'">
          <xsl:text> (electronic)</xsl:text>
        </xsl:if>
-->
      </pz:metadata>


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
