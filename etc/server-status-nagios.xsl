<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

 <xsl:output indent="no" method="text" version="1.0" encoding="UTF-8" />

  <xsl:template match="/server-status">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="memory">
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

  <xsl:template match="arena">
    <xsl:text>AREA=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="ordblks">
    <xsl:text>ORDBLKS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="uordblks">
    <xsl:text>UORDBLKS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="fordblks">
    <xsl:text>FORDBLKS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="keepcost">
    <xsl:text>KEEPCOST=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="hblks">
    <xsl:text>HBLKS=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="hblkhd">
    <xsl:text>HBLKHD=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="virt">
    <xsl:text>VIRT=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="virtuse">
    <xsl:text>VIRTUSE=</xsl:text><xsl:value-of select="." />
  </xsl:template>

  <xsl:template match="*" />

</xsl:stylesheet>
