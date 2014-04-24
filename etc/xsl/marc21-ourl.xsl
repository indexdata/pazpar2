<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">

  
  <xsl:import href="marc21.xsl" />
  <xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>

  <xsl:include href="pz2-ourl-marc21.xsl" />

  <xsl:template name="record-hook">
   Our hook
      <xsl:if test="$open_url_resolver">
        <pz:metadata type="open-url">
            <xsl:call-template name="insert-md-openurl" />
        </pz:metadata>
      </xsl:if>
</xsl:template>

</xsl:stylesheet>
