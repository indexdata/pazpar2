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
      <xsl:if test="string-length($medium) &gt; 0">
        <pz:metadata type="medium">
           <xsl:value-of select="$medium"/>
        </pz:metadata>
      </xsl:if>
      <xsl:apply-templates></xsl:apply-templates>
    </pz:record>
  </xsl:template>

  <xsl:template match="float[@name]">
    <pz:metadata>
	<xsl:attribute  name="type">
	  <xsl:value-of select="@name"/>
	</xsl:attribute>
	<xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="str[@name]">
    <pz:metadata>
	<xsl:attribute  name="type">
	  <xsl:value-of select="@name"/>
	</xsl:attribute>
	<xsl:value-of select="."/>
    </pz:metadata>
  </xsl:template>

  <xsl:template match="date[@name]">
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
    <xsl:choose>
      <xsl:when test="../@name = 'medium' and string-length($medium) &gt; 0">
      </xsl:when>
      <xsl:otherwise>
	<pz:metadata>
	  <xsl:attribute  name="type">
	    <xsl:value-of select="../@name"/>
	  </xsl:attribute>
	  <xsl:value-of select="."/>
	</pz:metadata>
	<xsl:if test="../@name='license_url'">
	 <pz:metadata type="license_name">
	  <xsl:choose>
	   <xsl:when test=". = 'http://creativecommons.org/licenses/by/4.0'">
	    CC By
	   </xsl:when>
	   <xsl:when test=". = 'http://creativecommons.org/licenses/by-nd/4.0'">
	    CC By-ND
	   </xsl:when>
	   <xsl:when test=". = 'http://creativecommons.org/licenses/by-nd-sa/4.0'">
	    CC By-ND-SA
	   </xsl:when>
	   <xsl:when test=". = 'http://creativecommons.org/licenses/by-nc/4.0'">
	    CC By-NC
	   </xsl:when>
	   <xsl:when test=". = 'http://creativecommons.org/licenses/by-nc-nd/4.0'">
	    CC By-NC-ND
	   </xsl:when>
	   <xsl:otherwise>
	    [unknown]
	   </xsl:otherwise>
	  </xsl:choose>
	 </pz:metadata>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
