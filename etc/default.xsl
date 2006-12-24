<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0">

  <xsl:template match="*">
    <pz:record>
      <xsl:match-templates/>
    </pz:record>
  </xsl:template>

  <xsl:template match="datafield[@tag='650']/subfield[@code='a']">
    <pz:facet type="subject">
      <xsl:value-of select="."/>
    </pz:facet>
  </xsl:template>
  
</xsl:stylesheet>

