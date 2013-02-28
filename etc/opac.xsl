<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">
  
  <xsl:import href="marc21.xsl"/>

  <xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>

<!-- Extract metadata from OPAC records with embedded MAR records
      http://www.loc.gov/marc/bibliographic/ecbdhome.html
-->  

  <xsl:template name="record-hook">
    <xsl:for-each select="/opacRecord/holdings/holding">
      <pz:metadata type="locallocation" empty="PAZPAR2_NULL_VALUE">
	<xsl:value-of select="localLocation"/>
      </pz:metadata>
      <pz:metadata type="callnumber" empty="PAZPAR2_NULL_VALUE">
	<xsl:value-of select="callNumber"/>
      </pz:metadata>
      <pz:metadata type="publicnote" empty="PAZPAR2_NULL_VALUE">
	<xsl:value-of select="publicNote"/>
      </pz:metadata>
      <pz:metadata type="available" empty="PAZPAR2_NULL_VALUE">
        <xsl:choose>
          <xsl:when test="circulations/circulation/availableNow/@value = '1'">
             Available
          </xsl:when>
          <xsl:when test="circulations/circulation/availableNow/@value = '0'">
	    <xsl:value-of select="circulations/circulation/availabiltyDate"/>
	  </xsl:when>
        </xsl:choose>
      </pz:metadata>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="/">
    <xsl:choose>
      <xsl:when test="opacRecord">
        <xsl:apply-templates select="opacRecord/bibliographicRecord"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
