<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns="http://www.indexdata.com/turbomarc"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
  xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
  xmlns:tmarc="http://www.indexdata.com/turbomarc">

  <xsl:output indent="yes" method="xml" version="1.0"
    encoding="UTF-8" />
  <xsl:param name="medium"/>

  <!-- Extract metadata from MARC21/USMARC from streamlined marcxml format 
    http://www.loc.gov/marc/bibliographic/ecbdhome.html -->
  <xsl:template name="record-hook" />


  <xsl:template match="/">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="tmarc:collection">
    <collection>
      <xsl:apply-templates />
    </collection>
  </xsl:template>

  <xsl:template match="tmarc:r">
    <xsl:variable name="title_medium" select="tmarc:d245/tmarc:sh" />
    <xsl:variable name="journal_title" select="tmarc:d773/tmarc:st" />
    <xsl:variable name="electronic_location_url" select="tmarc:d856/tmarc:su" />
    <xsl:variable name="fulltext_a" select="tmarc:d900/tmarc:sa" />
    <xsl:variable name="fulltext_b" select="tmarc:d900/tmarc:sb" />
    <!-- Does not always hit the right substring. The field is not always fixed-width? -->
    <xsl:variable name="control_lang" select="substring(tmarc:c008, 36, 3)" />
    <xsl:variable name="contains110" select="tmarc:d110" />
    <xsl:variable name="hasAuthorFields" select="tmarc:d100 or tmarc:d111" />
    <xsl:variable name="typeofrec" select="substring(tmarc:l, 7, 1)"/>
    <xsl:variable name="typeofvm" select="substring(tmarc:c008, 34, 1)"/>
    <xsl:variable name="biblevel" select="substring(tmarc:l, 8, 1)"/>
    <xsl:variable name="physdes" select="substring(tmarc:c007, 1, 1)"/>
    <xsl:variable name="form1" select="substring(tmarc:c008, 24, 1)"/>
    <xsl:variable name="form2" select="substring(tmarc:c008, 30, 1)"/>
    <xsl:variable name="oclca" select="substring(tmarc:c007, 1, 1)"/>
    <xsl:variable name="oclcb" select="substring(tmarc:c007, 2, 1)"/>
    <xsl:variable name="oclcd" select="substring(tmarc:c007, 4, 1)"/>
    <xsl:variable name="oclce" select="substring(tmarc:c007, 5, 1)"/>
    <xsl:variable name="typeofserial" select="substring(tmarc:c008, 22, 1)"/>

    <xsl:variable name="electronic">
      <xsl:choose>
        <xsl:when test="$form1='s' or $form1='q' or $form1='o' or
	   $form2='s' or $form2='q' or $form2='o'">
	   <xsl:text>yes</xsl:text>
	</xsl:when>
	<xsl:otherwise/>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="vmedium">
      <xsl:choose>
        <xsl:when test="string-length($medium)"><xsl:value-of select="$medium" /></xsl:when>
        <xsl:when test="($typeofrec='a' or $typeofrec='t') and $biblevel='m'">book</xsl:when>
        <xsl:when test="$typeofrec='j' or $typeofrec='i'">
	  <xsl:text>recording</xsl:text>
	  <xsl:choose>
	    <xsl:when test="$oclcb='d' and $oclcd='f'">-cd</xsl:when>
	    <xsl:when test="$oclcb='s'">-cassette</xsl:when>
	    <xsl:when test="$oclcb='d' and $oclcd='a' or $oclcd='b' or
	      $oclcd='c' or $oclcd='d' or $oclcd='e'">-vinyl</xsl:when>
	  </xsl:choose>
	</xsl:when>
	<xsl:when test="$typeofrec='g'">
	  <xsl:choose>
	    <xsl:when test="$typeofvm='m' or $typeofvm='v'">
	      <xsl:text>video</xsl:text>
	      <xsl:choose>
	        <xsl:when test="$oclca='v' and $oclcb='d' and $oclce='v'">-dvd</xsl:when>
	        <xsl:when test="$oclca='v' and $oclcb='d' and $oclce='s'">-blu-ray</xsl:when>
	        <xsl:when test="$oclca='v' and $oclcb='f' and $oclce='b'">-vhs</xsl:when>
	      </xsl:choose>
	    </xsl:when>
	    <xsl:otherwise>audio-visual</xsl:otherwise>
	  </xsl:choose>
	</xsl:when>
	<xsl:when test="$typeofrec='a' and $biblevel='s'">
	  <xsl:choose>
	    <xsl:when test="$typeofserial='n'">newspaper</xsl:when>
	    <xsl:otherwise>journal</xsl:otherwise>
	  </xsl:choose>
	</xsl:when>
	<xsl:when test="$typeofrec='e' or $typeofrec='f'">map</xsl:when>
	<xsl:when test="$typeofrec='c' or $typeofrec='d'">music-score</xsl:when>
	<xsl:when test="$form1='a' or $form1='b' or $form1='c'">microform</xsl:when>
	<xsl:when test="$typeofrec='t'">thesis</xsl:when>
        <!-- <xsl:when test="$journal_title">article</xsl:when> -->
	<xsl:when test="($typeofrec='a' or $typeofrec='i') and
	    ($typeofserial='d' or $typeofserial='w')">web</xsl:when>
	<xsl:when test="$typeofrec='a' and $biblevel='b'">article</xsl:when>
	<xsl:when test="$typeofrec='m'">electronic</xsl:when>
        <xsl:otherwise>
          <xsl:text>other</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="has_fulltext">
      <xsl:choose>
        <xsl:when test="tmarc:d856/tmarc:sq">
          <xsl:text>yes</xsl:text>
        </xsl:when>
        <xsl:when test="tmarc:d856/tmarc:si='TEXT*'">
          <xsl:text>yes</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>no</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="oclc_number">
      <xsl:choose>
        <xsl:when test='contains(tmarc:c001,"ocn") or
                        contains(tmarc:c001,"ocm") or
                        contains(tmarc:c001,"OCoLC") '>
         <xsl:value-of select="tmarc:c001"/>
        </xsl:when>
        <xsl:when test='contains(tmarc:d035/tmarc:sa,"ocn") or
                        contains(tmarc:d035/tmarc:sa,"ocm") or
                        contains(tmarc:d035/tmarc:sa,"OCoLC") '>
         <xsl:value-of select="tmarc:d035/tmarc:sa"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="date_008">
      <xsl:choose>
        <xsl:when test="contains('cestpudikmr', substring(tmarc:c008, 7, 1))">
          <xsl:value-of select="substring(tmarc:c008, 8, 4)" />
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="date_end_008">
      <xsl:choose>
        <xsl:when test="contains('dikmr', substring(tmarc:c008, 7, 1))">
          <xsl:value-of select="substring(tmarc:c008, 12, 4)" />
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <pz:record>
<!--
      <xsl:attribute name="mergekey">
    <xsl:text>title </xsl:text>
  <xsl:value-of select="tmarc:d245/tmarc:sa" />
  <xsl:text> author </xsl:text>
  <xsl:value-of select="tmarc:d100/tmarc:sa" />
  <xsl:text> medium </xsl:text>
  <xsl:value-of select="$medium" />
    </xsl:attribute>
  -->

      <xsl:for-each select="tmarc:c001">
        <pz:metadata type="id">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:if test="string-length($oclc_number) &gt; 0">
	<pz:metadata type="oclc-number">
	  <xsl:value-of select="$oclc_number" />
	</pz:metadata>
      </xsl:if>

      <xsl:for-each select="tmarc:d010">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="lccn">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d020">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="isbn">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d022">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="issn">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>
      
      <xsl:for-each select="tmarc:d027">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="tech-rep-nr">
	    <xsl:value-of select="tmarc:sa" />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>
      
      <xsl:for-each select="tmarc:d035"> 
        <pz:metadata type="system-control-nr">
          <xsl:choose>
            <xsl:when test="tmarc:sa">
              <xsl:value-of select="tmarc:sa"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="tmarc:sb"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d100">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="author">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="author-title">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sd">
	  <pz:metadata type="author-date">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d110">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="corporate-name">
          <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="corporate-location">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sd">
	  <pz:metadata type="corporate-date">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>
      
      <xsl:for-each select="tmarc:d111">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="meeting-name">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="meeting-location">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sd">
	  <pz:metadata type="meeting-date">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d260">
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="date">
	    <xsl:value-of select="translate(., 'cp[].', '')" />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:if test="string-length($date_008) &gt; 0 and not(tmarc:d260)">
        <pz:metadata type="date">
          <xsl:choose>
            <xsl:when test="$date_end_008">
              <xsl:value-of select="concat($date_008,'-',$date_end_008)" />
            </xsl:when>
            <xsl:otherwise> 
              <xsl:value-of select="$date_008" />
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:if>

      <xsl:for-each select="tmarc:d130">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="title-uniform">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sm">
	  <pz:metadata type="title-uniform-media">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sn">
	  <pz:metadata type="title-uniform-parts">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sp">
	  <pz:metadata type="title-uniform-partname">
	    <xsl:value-of select="." />	    
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sr">
	  <pz:metadata type="title-uniform-key">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>
      
      <xsl:for-each select="tmarc:d245">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="title">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sb">
	  <pz:metadata type="title-remainder">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="title-responsibility">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sf">
	  <pz:metadata type="title-dates">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sh">
	  <pz:metadata type="title-medium">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sn">
	  <pz:metadata type="title-number-section">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:if test="tmarc:sa">
	  <pz:metadata type="title-complete">
	    <xsl:value-of select="tmarc:sa" />
	    <xsl:if test="tmarc:sb" ><xsl:value-of select="concat(' ', tmarc:sb)" /></xsl:if>
	  </pz:metadata>
	</xsl:if>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d250">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="edition">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d260">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="publication-place">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sb">
	  <pz:metadata type="publication-name">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="publication-date">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d300">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="physical-extent">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sb">
	  <pz:metadata type="physical-format">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sc">
	  <pz:metadata type="physical-dimensions">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:se">
	  <pz:metadata type="physical-accomp">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sf">
	  <pz:metadata type="physical-unittype">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sg">
	  <pz:metadata type="physical-unitsize">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:s3">
	  <pz:metadata type="physical-specified">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d440">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="series-title">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d500">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d505">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d518">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d520">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d522">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d911">
        <pz:metadata type="description">
          <xsl:for-each select="node()">
            <xsl:value-of select="text()" />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d600 | tmarc:d610 | tmarc:d611 | tmarc:d630 |
                            tmarc:d648 | tmarc:d650 | tmarc:d651 | tmarc:d653 |
                            tmarc:d654 | tmarc:d655 | tmarc:d656 | tmarc:d657 |
                            tmarc:d658 | tmarc:d662 | tmarc:d69X">
	<xsl:for-each select="tmarc:sa">
	  <pz:metadata type="subject">
	    <xsl:value-of select="."/>
	  </pz:metadata>
	</xsl:for-each>
	<pz:metadata type="subject-long">
           <xsl:for-each select="node()/text()">
             <xsl:if test="position() &gt; 1">
               <xsl:text>, </xsl:text>
             </xsl:if>
            <xsl:variable name='value'>
              <xsl:value-of select='normalize-space(.)'/>
            </xsl:variable>
            <xsl:choose>
              <xsl:when test="substring($value, string-length($value)) = ','">
                <xsl:value-of select="substring($value, 1, string-length($value)-1)"/>
              </xsl:when>
              <xsl:otherwise>
                <xsl:value-of select="$value"/>
              </xsl:otherwise>
            </xsl:choose> 
          </xsl:for-each>
         </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d856">
	<xsl:for-each select="tmarc:su">
	  <pz:metadata type="electronic-url">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
        <pz:metadata type="electronic-text">
	  <xsl:choose>
	    <xsl:when test="tmarc:sy">
	      <xsl:value-of select="tmarc:sy/text()" />
	    </xsl:when>
	    <xsl:when test="tmarc:s3">
	      <xsl:value-of select="tmarc:s3/text()" />
	    </xsl:when>
	    <xsl:when test="tmarc:sa">
	      <xsl:value-of select="tmarc:sa/text()" />
	    </xsl:when>
	    <xsl:otherwise>Get resource</xsl:otherwise>
	  </xsl:choose>
        </pz:metadata>
	<xsl:for-each select="tmarc:sz">
	  <pz:metadata type="electronic-note">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:si">
	  <pz:metadata type="electronic-format-instruction">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
	<xsl:for-each select="tmarc:sq">
	  <pz:metadata type="electronic-format-type">
	    <xsl:value-of select="." />
	  </pz:metadata>
	</xsl:for-each>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d773">
        <pz:metadata type="citation">
          <xsl:for-each select="*">
            <xsl:value-of select="normalize-space(.)" />
            <xsl:text> </xsl:text>
          </xsl:for-each>
        </pz:metadata>
        <xsl:for-each select="tmarc:st">
          <pz:metadata type="journal-title">
            <xsl:value-of select="."/>
          </pz:metadata>
        </xsl:for-each>
        <xsl:if test="tmarc:sg">
	  <xsl:variable name="value">
            <xsl:for-each select="tmarc:sg">
              <xsl:value-of select="."/>
            </xsl:for-each>
	  </xsl:variable>
          <pz:metadata type="journal-subpart">
            <xsl:value-of select="$value"/>
	  </pz:metadata>
	  <xsl:variable name="l">
	    <xsl:value-of select="translate($value,
				  'ABCDEFGHIJKLMNOPQRSTUVWXYZ.',
				  'abcdefghijklmnopqrstuvwxyz ') "/>
	  </xsl:variable>
	  <xsl:variable name="volume">
	    <xsl:choose>
	      <xsl:when test="string-length(substring-after($l,'vol ')) &gt; 0">
		<xsl:value-of select="substring-before(normalize-space(substring-after($l,'vol ')),' ')"/>
	      </xsl:when>
	      <xsl:when test="string-length(substring-after($l,'v ')) &gt; 0">
		<xsl:value-of select="substring-before(normalize-space(substring-after($l,'v ')),' ')"/>
	      </xsl:when>
	    </xsl:choose>
	  </xsl:variable>
	  <xsl:variable name="issue">
	    <xsl:value-of select="substring-before(translate(normalize-space(substring-after($l,'issue')), ',', ' '),' ')"/>
	  </xsl:variable>
	  <xsl:variable name="pages">
	    <xsl:choose>
	      <xsl:when test="string-length(substring-after($l,' p ')) &gt; 0">
		<xsl:value-of select="normalize-space(substring-after($l,' p '))"/>
	      </xsl:when>
	      <xsl:when test="string-length(substring-after($l,',p')) &gt; 0">
		<xsl:value-of select="normalize-space(substring-after($l,',p'))"/>
	      </xsl:when>
	      <xsl:when test="string-length(substring-after($l,' p')) &gt; 0">
		<xsl:value-of select="normalize-space(substring-after($l,' p'))"/>
	      </xsl:when>
	    </xsl:choose>
	  </xsl:variable>

	  <!-- volume -->
	  <xsl:if test="string-length($volume) &gt; 0">
	    <pz:metadata type="volume-number">
	      <xsl:value-of select="$volume"/>
	    </pz:metadata>
	  </xsl:if>
	  <!-- issue -->
	  <xsl:if test="string-length($issue) &gt; 0">
	    <pz:metadata type="issue-number">
	      <xsl:value-of select="$issue"/>
	    </pz:metadata>
	  </xsl:if>
	  <!-- pages -->
	  <xsl:if test="string-length($pages) &gt; 0">
	    <pz:metadata type="pages-number">
	      <xsl:value-of select="$pages"/>
	    </pz:metadata>
	  </xsl:if>

	  <!-- season -->
        </xsl:if>
        <xsl:if test="tmarc:sp">
          <pz:metadata type="journal-title-abbrev">
            <xsl:value-of select="tmarc:sp"/>
          </pz:metadata>
        </xsl:if>
      </xsl:for-each>

      <xsl:if test="not(ancestor::opacRecord)">
        <xsl:for-each select="tmarc:d852">
          <xsl:variable name="vCall">
            <xsl:for-each select="tmarc:sh | tmarc:si">
              <xsl:value-of select="concat(.,' ')"/>
            </xsl:for-each>
          </xsl:variable>
          <xsl:variable name="vLocation">
            <xsl:choose>
              <xsl:when test="tmarc:sb">
                <xsl:for-each select="tmarc:sb">
                  <xsl:value-of select="concat(.,' ')"/>
                </xsl:for-each>
              </xsl:when>
              <xsl:otherwise>
                <xsl:value-of select="tmarc:sa"/>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:variable>
          <pz:metadata type="locallocation" empty="PAZPAR2_NULL_VALUE">
            <xsl:value-of select="normalize-space($vLocation)"/>
          </pz:metadata>
          <pz:metadata type="callnumber" empty="PAZPAR2_NULL_VALUE">
            <xsl:value-of select="normalize-space($vCall)" />
          </pz:metadata>
          <xsl:for-each select="tmarc:sy">
            <pz:metadata type="publicnote">
              <xsl:choose>
                <xsl:when test="text() = '1'">Available</xsl:when>
                <xsl:when test="text() = '0'">Not Available</xsl:when>
                <xsl:otherwise><xsl:value-of select="." /></xsl:otherwise>
              </xsl:choose>
            </pz:metadata>
          </xsl:for-each>
          <pz:metadata type="available">PAZPAR2_NULL_VALUE</pz:metadata>
        </xsl:for-each>
      </xsl:if>

      <xsl:for-each select="tmarc:d876">
        <xsl:if test="tmarc:sf">
          <pz:metadata type="loan-period">
            <xsl:value-of select="concat(tmarc:s5,':',tmarc:sf)" />
          </pz:metadata>
        </xsl:if>
      </xsl:for-each>

      <pz:metadata type="medium">
        <xsl:value-of select="$vmedium" />
        <xsl:choose>
	  <xsl:when test="string-length($electronic) and $vmedium != 'electronic'">
	    <xsl:text> (electronic)</xsl:text>
	  </xsl:when>
          <xsl:when test="$vmedium = 'book' and ($form1 = 'd' or ($oclca='t' and $oclcb='b'))">
            <xsl:text> (large print)</xsl:text>
          </xsl:when>
        </xsl:choose>
      </pz:metadata>

      <xsl:for-each select="tmarc:d900/tmarc:sa">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <!-- <xsl:if test="$fulltext_a"> <pz:metadata type="fulltext"> <xsl:value-of 
        select="$fulltext_a"/> </pz:metadata> </xsl:if> -->

      <xsl:for-each select="tmarc:d900/tmarc:sb">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:se">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:sf">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:si">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:sk">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:sq">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:ss">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:su">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d900/tmarc:sy">
        <pz:metadata type="fulltext">
          <xsl:value-of select="." />
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d903">
        <xsl:if test="number(tmarc:sa) &gt; 0">
          <pz:metadata type="publication-date">
            <xsl:value-of select="number(tmarc:sa)"/>
          </pz:metadata>
          <pz:metadata type="date">
            <xsl:value-of select="number(tmarc:sa)"/>
          </pz:metadata>
        </xsl:if>
      </xsl:for-each>

      <!-- <xsl:if test="$fulltext_b"> <pz:metadata type="fulltext"> <xsl:value-of 
        select="$fulltext_b"/> </pz:metadata> </xsl:if> -->

      <pz:metadata type="has-fulltext">
        <xsl:value-of select="$has_fulltext"/>
      </pz:metadata>

      <xsl:for-each select="tmarc:d907 | tmarc:d901">
        <pz:metadata type="iii-id">
          <xsl:value-of select="tmarc:sa" />
        </pz:metadata>
      </xsl:for-each>

      <xsl:if test="not(ancestor::opacRecord)">
        <xsl:for-each select="tmarc:d926">
          <pz:metadata type="locallocation" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:sa"/>
	  </pz:metadata>
          <pz:metadata type="callnumber" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:sc"/>
	  </pz:metadata>
          <pz:metadata type="available" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:se"/>
	  </pz:metadata>
        </xsl:for-each>
      </xsl:if>

      <!-- OhioLINK holdings -->
      <xsl:if test="not(ancestor::opacRecord)">
        <xsl:for-each select="tmarc:d945">
	  <pz:metadata type="locallocation" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:sa"/>
	  </pz:metadata>
	  <pz:metadata type="callnumber" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:sb"/>
	  </pz:metadata>
	  <pz:metadata type="publicnote" empty="PAZPAR2_NULL_VALUE">
	    <xsl:value-of select="tmarc:sc"/>
	  </pz:metadata>
	  <pz:metadata type="available" empty="PAZPAR2_NULL_VALUE">
            <xsl:choose>
              <xsl:when test="tmarc:ss = 'N'">Available</xsl:when>
              <xsl:otherwise>
	        <xsl:value-of select="tmarc:sd"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </pz:metadata>
        </xsl:for-each>
      </xsl:if>

      <xsl:for-each select="tmarc:d948">
        <pz:metadata type="holding">
          <xsl:for-each select="tmarc:s">
            <xsl:if test="position() > 1">
              <xsl:text> </xsl:text>
            </xsl:if>
            <xsl:value-of select="." />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d991">
        <pz:metadata type="holding">
          <xsl:for-each select="tmarc:s">
            <xsl:if test="position() > 1">
              <xsl:text> </xsl:text>
            </xsl:if>
            <xsl:value-of select="." />
          </xsl:for-each>
        </pz:metadata>
      </xsl:for-each>

      <xsl:for-each select="tmarc:d999">
        <pz:metadata type="localid">
          <xsl:choose>
            <xsl:when test="tmarc:sa">
              <xsl:value-of select="tmarc:sa"/>
            </xsl:when>
            <xsl:when test="tmarc:sc">
              <xsl:value-of select="tmarc:sc"/>
            </xsl:when> 
            <xsl:otherwise>
              <xsl:value-of select="tmarc:sd"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>


      <!-- passthrough id data -->
      <xsl:for-each select="pz:metadata">
        <xsl:copy-of select="." />
      </xsl:for-each>

      <!-- other stylesheets importing this might want to define this -->
	<xsl:call-template name="record-hook" />

    </pz:record>
  </xsl:template>

  <xsl:template match="text()" />

</xsl:stylesheet>
