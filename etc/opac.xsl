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
      <pz:metadata type="locallocation">
        <xsl:choose>
	  <xsl:when test="localLocation">
	    <xsl:value-of select="localLocation"/>
	  </xsl:when>
	  <xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
	</xsl:choose>
      </pz:metadata>
      <pz:metadata type="callnumber">
        <xsl:choose>
	  <xsl:when test="callNumber">
	    <xsl:value-of select="callNumber"/>
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
	  <xsl:when test="publicNote">
	    <xsl:value-of select="publicNote"/>
	  </xsl:when>
	  <xsl:otherwise>PAZPAR2_NULL_VALUE</xsl:otherwise>
	</xsl:choose>
      </pz:metadata>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="/">
    <xsl:apply-templates select="opacRecord/bibliographicRecord"/>
  </xsl:template>

</xsl:stylesheet>
