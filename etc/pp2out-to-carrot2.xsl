<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>
<xsl:template match="/">
  <searchresult>
    <!-- TODO make query an xsl parameter -->
    <query>water</query>
    <xsl:for-each select="show/hit">
    <document>
      <xsl:attribute name="id">
        <xsl:value-of select="location/md-id" />
      </xsl:attribute>
      <title><xsl:value-of select="md-title" /></title>
      <url><xsl:value-of select="recid" /></url>
      <snippet xml:space="preserve">
        <xsl:for-each select="md-description">
          <xsl:value-of select="." />
        </xsl:for-each>
        <xsl:value-of select="md-title-responsibility" />
      </snippet>
    </document> 
    </xsl:for-each>
  </searchresult>
</xsl:template>
</xsl:stylesheet>