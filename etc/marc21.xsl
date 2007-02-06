<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">

  <xsl:template match="/marc:record">
    <pz:record>

      <xsl:attribute name="mergekey">
        <xsl:text>title </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='a']"/>
	<xsl:text> author </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='100']/marc:subfield[@code='a']"/>
      </xsl:attribute>

      <pz:metadata type="id">
        <xsl:value-of select="marc:controlfield[@tag='001']"/>
      </pz:metadata>

      <xsl:for-each select="marc:datafield[@tag='010']">
        <pz:metadata type="lccn">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <pz:metadata type="title">
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='a']"/>
	<xsl:text> </xsl:text>
	<xsl:value-of select="marc:datafield[@tag='245']/marc:subfield[@code='b']"/>
      </pz:metadata>

      <xsl:for-each select="marc:datafield[@tag='020']">
        <pz:metadata type="isbn">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='260']">
        <pz:metadata type="date">
	  <xsl:value-of select="marc:subfield[@code='c']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='650' or @tag='653']">
	<pz:metadata type="subject">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='100']">
	<pz:metadata type="author">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='520']">
        <pz:metadata type="description">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='700']">
	<pz:metadata type="author">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='720']">
	<pz:metadata type="author">
	  <xsl:value-of select="marc:subfield[@code='a']"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="marc:datafield[@tag='856']">
	<pz:metadata type="url">
	  <xsl:value-of select="marc:subfield[@code='u']"/>
	</pz:metadata>
      </xsl:for-each>

    </pz:record>
  </xsl:template>

</xsl:stylesheet>
