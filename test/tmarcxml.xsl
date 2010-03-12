<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:tmarc="http://www.indexdata.com/MARC21/turboxml" >
  
  <xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>

<!-- 
     Extract metadata from MARC21/USMARC from streamlined marcxml format
      http://www.loc.gov/marc/bibliographic/ecbdhome.html
-->  
  <xsl:template name="record-hook"/>


  <xsl:template match="/">
    <pz:collection>
     <xsl:apply-templates/>
    </pz:collection>
  </xsl:template>

  <xsl:template match="tmarc:r">
    <xsl:variable name="title_medium" select="tmarc:d245/tmarc:sh"/>
    <xsl:variable name="journal_title" select="tmarc:d773/tmarc:st"/>
    <xsl:variable name="electronic_location_url" select="tmarc:d856/tmarc:su"/>
    <xsl:variable name="fulltext_a" select="tmarc:d900/tmarc:sa"/>
    <xsl:variable name="fulltext_b" select="tmarc:d900/tmarc:sb"/>
	<!-- Does not always hit the right substring. The field is not always fixed-width? -->
    <xsl:variable name="control_lang" select="substring(tmarc:c008, 36, 3)"/>
    <xsl:variable name="contains110" select="tmarc:d110"/>
    <xsl:variable name="hasAuthorFields" select="tmarc:d100 or tmarc:d111"/>
    
    <xsl:variable name="medium">
	<xsl:choose>
		<xsl:when test="$title_medium">
			<xsl:value-of select="translate($title_medium, ' []/', '')" />
		</xsl:when>
		<xsl:when test="$fulltext_a">
			<xsl:text>electronic resource</xsl:text>
		</xsl:when>
		<xsl:when test="$fulltext_b">
			<xsl:text>electronic resource</xsl:text>
		</xsl:when>
		<xsl:when test="$journal_title">
			<xsl:text>article</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>book</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
    </xsl:variable>

    <pz:record>
	<xsl:attribute name="mergekey">
        <xsl:text>title </xsl:text>
	<xsl:value-of select="tmarc:d245/tmarc:sa" />
	<xsl:text> author </xsl:text>
	<xsl:value-of select="tmarc:d100/tmarc:sa" />
	<xsl:text> medium </xsl:text>
	<xsl:value-of select="$medium" />
      </xsl:attribute>

      <xsl:for-each select="tmarc:c001">
        <pz:metadata type="id">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d010">
        <pz:metadata type="lccn">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d020">
        <pz:metadata type="isbn">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d022">
        <pz:metadata type="issn">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d027">
        <pz:metadata type="tech-rep-nr">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d035">
        <pz:metadata type="system-control-nr">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d100">
	<pz:metadata type="author">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="author-title">
	  <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
	<pz:metadata type="author-date">
	  <xsl:value-of select="tmarc:sd"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d110">
	<pz:metadata type="corporate-name">
	    <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="corporate-location">
	    <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
	<pz:metadata type="corporate-date">
	    <xsl:value-of select="tmarc:sd"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d111">
	<pz:metadata type="meeting-name">
	    <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="meeting-location">
	    <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
	<pz:metadata type="meeting-date">
	    <xsl:value-of select="tmarc:sd"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d260">
	<pz:metadata type="date">
	    <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d130">
        <pz:metadata type="title-uniform">
          <xsl:value-of select="tmarc:sa"/>
        </pz:metadata>
        <pz:metadata type="title-uniform-media">
          <xsl:value-of select="tmarc:sm"/>
        </pz:metadata>
        <pz:metadata type="title-uniform-parts">
          <xsl:value-of select="tmarc:sn"/>
        </pz:metadata>
        <pz:metadata type="title-uniform-partname">
          <xsl:value-of select="tmarc:sp"/>
        </pz:metadata>
        <pz:metadata type="title-uniform-key">
          <xsl:value-of select="tmarc:sr"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d245">
        <pz:metadata type="title">
          <xsl:value-of select="tmarc:sa"/>
        </pz:metadata>
        <pz:metadata type="title-remainder">
          <xsl:value-of select="tmarc:sb"/>
        </pz:metadata>
        <pz:metadata type="title-responsibility">
          <xsl:value-of select="tmarc:sc"/>
        </pz:metadata>
        <pz:metadata type="title-dates">
          <xsl:value-of select="tmarc:sf"/>
        </pz:metadata>
        <pz:metadata type="title-medium">
          <xsl:value-of select="tmarc:sh"/>
        </pz:metadata>
        <pz:metadata type="title-number-section">
          <xsl:value-of select="tmarc:sn"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d250">
	<pz:metadata type="edition">
	    <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d260">
        <pz:metadata type="publication-place">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
        <pz:metadata type="publication-name">
	  <xsl:value-of select="tmarc:sb"/>
	</pz:metadata>
        <pz:metadata type="publication-date">
	  <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d300">
	<pz:metadata type="physical-extent">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="physical-format">
	  <xsl:value-of select="tmarc:sb"/>
	</pz:metadata>
	<pz:metadata type="physical-dimensions">
	  <xsl:value-of select="tmarc:sc"/>
	</pz:metadata>
	<pz:metadata type="physical-accomp">
	  <xsl:value-of select="tmarc:se"/>
	</pz:metadata>
	<pz:metadata type="physical-unittype">
	  <xsl:value-of select="tmarc:sf"/>
	</pz:metadata>
	<pz:metadata type="physical-unitsize">
	  <xsl:value-of select="tmarc:sg"/>
	</pz:metadata>
	<pz:metadata type="physical-specified">
	  <xsl:value-of select="tmarc:s3"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d440">
	<pz:metadata type="series-title">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d500">
	<pz:metadata type="description">
          <xsl:value-of select="*/text()"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d505">
	<pz:metadata type="description">
          <xsl:value-of select="*/text()"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d518">
	<pz:metadata type="description">
          <xsl:value-of select="*/text()"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d520">
	<pz:metadata type="description">
          <xsl:value-of select="*/text()"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d522">
	<pz:metadata type="description">
          <xsl:value-of select="*/text()"/>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d600" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d610" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d611" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d630" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d648" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d650" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d651" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d653" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d654" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d655" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d656" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d657" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d658" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d662" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d69X" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

<!--
     or tmarc:d651 or tmarc:d653 or tmarc:d654 or tmarc:d655 or tmarc:d656 or tmarc:d657 or tmarc:d658 or tmarc:d662 or tmarc:d69X">
-->

<!--
      <xsl:for-each select="tmarc:d600" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="tmarc:sa tmarc:sb tmarc:sc tmarc:sd ">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>
-->

      <xsl:for-each select="tmarc:d856">
	<pz:metadata type="electronic-url">
	  <xsl:value-of select="tmarc:su"/>
	</pz:metadata>
	<pz:metadata type="electronic-text">
	  <xsl:if test="tmarc:sy" >
	    <xsl:value-of select="tmarc:sy/text()" />
	  </xsl:if>
	  <xsl:if test="tmarc:s3">
	    <xsl:value-of select="tmarc:s3/text()" />
	  </xsl:if>
	</pz:metadata>
	<pz:metadata type="electronic-note">
	  <xsl:value-of select="tmarc:sz"/>
	</pz:metadata>
	<pz:metadata type="electronic-format-instruction">
	  <xsl:value-of select="tmarc:si"/>
	</pz:metadata>
	<pz:metadata type="electronic-format-type">
	  <xsl:value-of select="tmarc:sq"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d773">
	<pz:metadata type="citation">
	  <xsl:for-each select="*">
	    <xsl:value-of select="normalize-space(.)"/>
	    <xsl:text> </xsl:text>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d852">
        <xsl:if test="tmarc:sy">
	  <pz:metadata type="publicnote">
	    <xsl:value-of select="tmarc:sy"/>
	  </pz:metadata>
	</xsl:if>
	<xsl:if test="tmarc:sh">
	  <pz:metadata type="callnumber">
	    <xsl:value-of select="tmarc:sh"/>
	  </pz:metadata>
	</xsl:if>
      </xsl:for-each>

      <pz:metadata type="medium">
	<xsl:value-of select="$medium"/>
      </pz:metadata>
      
      <xsl:for-each select="tmarc:d900/tmarc:sa">
        <pz:metadata type="fulltext">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

      <!-- <xsl:if test="$fulltext_a">
	<pz:metadata type="fulltext">
	  <xsl:value-of select="$fulltext_a"/>
	</pz:metadata>
      </xsl:if>
-->

      <xsl:for-each select="tmarc:d900/tmarc:sb">
        <pz:metadata type="fulltext">
          <xsl:value-of select="."/>
        </pz:metadata>
      </xsl:for-each>

      <!-- <xsl:if test="$fulltext_b">
	<pz:metadata type="fulltext">
	  <xsl:value-of select="$fulltext_b"/>
	</pz:metadata>
      </xsl:if>
-->

      <xsl:for-each select="tmarc:d907">
<!--  or tmarc:d901"> -->
        <pz:metadata type="iii-id">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d926">
        <pz:metadata type="holding">
	  <xsl:for-each select="tmarc:s">
	    <xsl:if test="position() > 1">
	      <xsl:text> </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d948">
        <pz:metadata type="holding">
	  <xsl:for-each select="tmarc:s">
	    <xsl:if test="position() > 1">
	      <xsl:text> </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d991">
        <pz:metadata type="holding">
	  <xsl:for-each select="tmarc:s">
	    <xsl:if test="position() > 1">
	      <xsl:text> </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <!-- passthrough id data -->
      <xsl:for-each select="pz:metadata">
          <xsl:copy-of select="."/>
      </xsl:for-each>

      <!-- other stylesheets importing this might want to define this -->
      <xsl:call-template name="record-hook"/>

    </pz:record>    
  </xsl:template>
  
  <xsl:template match="text()"/>

  <xsl:template name="shortTitle">
    <xsl:param name="tag" />
    <xsl:for-each select="tmarc:d">
      <xsl:value-of select="tmarc:sa" />
      <xsl:value-of select="tmarc:sm" />
      <xsl:value-of select="tmarc:sn" />
      <xsl:value-of select="tmarc:sp" />
      <xsl:value-of select="tmarc:sr" />
    </xsl:for-each>
  </xsl:template>


  <xsl:template name="description">
    <xsl:param name="element" />
    <xsl:for-each select="$element">
	<pz:metadata type="description">
            <xsl:value-of select="*/text()"/>
        </pz:metadata>
    </xsl:for-each>
    <xsl:apply-templates/>
  </xsl:template>


  <xsl:template name="subject">
    <xsl:param name="element" />
      <xsl:for-each select="$element" > 
        <pz:metadata type="subject">
	  <xsl:value-of select="tmarc:sa"/>
	</pz:metadata>
	<pz:metadata type="subject-long">
	  <xsl:for-each select="node()/text()">
	    <xsl:if test="position() > 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="."/>
	  </xsl:for-each>
	</pz:metadata>
      </xsl:for-each>
  </xsl:template>


</xsl:stylesheet>
