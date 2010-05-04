<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:marc="http://www.loc.gov/MARC21/slim"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  >
  <xsl:output method="text"/>
  <xsl:strip-space elements="*"/>
 
  <xsl:template match="activeclients">
     <xsl:value-of select="."/>
  </xsl:template> 
  <xsl:template match="text()"/>
</xsl:stylesheet>
