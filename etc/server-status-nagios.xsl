<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

 <xsl:output indent="no" method="text" version="1.0" encoding="UTF-8" />

  <xsl:template match="/server-status">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="sessions">
    <xsl:text>export SESSIONS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="clients">
    <xsl:text>export CLIENTS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="resultsets">
    <xsl:text>export RESULTSETS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="*">
  </xsl:template>

</xsl:stylesheet>
