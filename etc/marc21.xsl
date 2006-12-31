<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">

  <xsl:template match="/marc:record">
    <pz:record>

      <pz:metadata type="title">
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='a']"/>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='b']"/>
      </pz:metadata>

      <pz:mergekey>
        <xsl:text>title </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='a']"/>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='b']"/>
	<xsl:text> author </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='100']/marc:subfield[@code='a']"/>
      </pz:mergekey>

      <xsl:for-each select="marc:datafield[@tag='650']">
	<pz:facet type="subject">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:facet>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='100']">
	<pz:facet type="author">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:facet>
      </xsl:for-each>
    </pz:record>
  </xsl:template>

</xsl:stylesheet>

