<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
		xmlns:pz="http://www.indexdata.com/pazpar2/1.0" >

  <xsl:param name="medium" />

  <xsl:template  match="/">
      <xsl:apply-templates></xsl:apply-templates>
  </xsl:template>

  <xsl:template  match="response">
      <xsl:apply-templates></xsl:apply-templates>
  </xsl:template>

  <xsl:template  match="records">
      <xsl:apply-templates></xsl:apply-templates>
  </xsl:template>

  <xsl:template match="doc">
    <pz:record>
      <xsl:apply-templates></xsl:apply-templates>
    </pz:record>
  </xsl:template>

  <xsl:template match="str[@name]">
    <pz:metadata>
	<xsl:attribute  name="type">
	  <xsl:value-of select="@name"/>
	</xsl:attribute>
	<xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="arr">
    <xsl:for-each select="str">
      <xsl:call-template name="string"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="string">
      <pz:metadata>
	<xsl:attribute  name="type">
	  <xsl:value-of select="../@name"/>
	</xsl:attribute>
	<xsl:choose>
	  <xsl:when test="../@name = 'medium' and string-length($medium) > 0">
	    <xsl:value-of select="$medium"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="."/>
	  </xsl:otherwise>
	</xsl:choose>
      </pz:metadata>
  </xsl:template>

</xsl:stylesheet>
