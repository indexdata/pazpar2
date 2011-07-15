<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
		xmlns:pz="http://www.indexdata.com/pazpar2/1.0" >
  <xsl:template  match="/">
    <add>
      <xsl:apply-templates></xsl:apply-templates>
    </add>
  </xsl:template>

  <xsl:template match="pz:record">
    <doc>
      <xsl:apply-templates></xsl:apply-templates>
    </doc>
  </xsl:template>
  <xsl:template match="pz:metadata">
    <xsl:if test="@type">
      <field>
	<xsl:attribute  name="name">
	  <xsl:value-of select="@type"/>
	</xsl:attribute>
	<xsl:value-of select="."/>
      </field>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
