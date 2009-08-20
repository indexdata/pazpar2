<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">
  
  <xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>

<!-- Extract metadata from MARC21/USMARC 
      http://www.loc.gov/marc/bibliographic/ecbdhome.html
-->  
  <xsl:template name="record-hook"/>

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="marc:record">
    <xsl:variable name="title_medium" select="marc:datafield[@tag='245']/marc:subfield[@code='h']"/>
    <xsl:variable name="journal_title" select="marc:datafield[@tag='773']/marc:subfield[@code='t']"/>
    <xsl:variable name="electronic_location_url" select="marc:datafield[@tag='856']/marc:subfield[@code='u']"/>
    <xsl:variable name="fulltext_a" select="marc:datafield[@tag='900']/marc:subfield[@code='a']"/>
    <xsl:variable name="fulltext_b" select="marc:datafield[@tag='900']/marc:subfield[@code='b']"/>
    <xsl:variable name="medium">
      <xsl:choose>
	<xsl:when test="$title_medium">
	  <xsl:value-of select="substring-after(substring-before($title_medium,']'),'[')"/>
	</xsl:when>
	<xsl:when test="$fulltext_a">
	  <xsl:text>electronic resource</xsl:text>
	</xsl:when>
	<xsl:when test="$fulltext_b">
	  <xsl:text>electronic resource</xsl:text>
	</xsl:when>
	<xsl:when test="$journal_title">
	  <xsl:text>article</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text>book</xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <pz:record>
      <xsl:attribute name="mergekey">
        <xsl:text>title </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='a']"/>
	<xsl:text> author </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='100']/marc:subfield[@code='a']"/>
	<xsl:text> medium </xsl:text>
	<xsl:value-of select="$medium"/>
      </xsl:attribute>

      <xsl:apply-templates/>

      <!-- other stylesheets importing this might want to define this -->
      <xsl:call-template name="record-hook"/>

    </pz:record>    
  </xsl:template>

  <xsl:template match="marc:controlfield[@tag='001']">
    <pz:metadata type="id">
      <xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="marc:datafield[@tag='010']">
    <pz:metadata type="lccn">
      <xsl:value-of select="marc:subfield[@code='a']"/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="marc:datafield[@tag='020']">
    <pz:metadata type="isbn">
      <xsl:value-of select="marc:subfield[@code='a']"/>
    </pz:metadata>
  </xsl:template>
  
  <!-- does ANYONE need this?
  <xsl:template match="marc:datafield[@tag='027']">
    <pz:metadata type="tech-rep-nr">
      <xsl:value-of select="marc:subfield[@code='a']"/>
    </pz:metadata>
  </xsl:template>
  -->
  
  <xsl:template match="marc:datafield[@tag='035']">
    <pz:metadata type="system-control-nr">
      <xsl:value-of select="marc:subfield[@code='a']"/>
    </pz:metadata>
  </xsl:template>
  
  <xsl:template match="marc:datafield[@tag='100']">
    <pz:metadata type="author">
      <xsl:value-of select="marc:subfield[@code='a']"/>
    </pz:metadata>
    <pz:metadata type="author-title">
      <xsl:value-of select="marc:subfield[@code='c']"/>
    </pz:metadata>
    <pz:metadata type="author-date">
      <xsl:value-of select="marc:subfield[@code='d']"/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="text()"/>

</xsl:stylesheet>
